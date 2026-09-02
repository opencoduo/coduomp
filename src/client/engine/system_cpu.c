#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "q_shared.h"

#include "system_localization.h"
#include "system_process_lock.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#include <x86intrin.h>
#endif

qboolean sysSseSupported; /* original 0x0489d460 */
double sysCpuFrequencyMHz; /* original 0x009cf1d8 */
int32_t sysPhysicalMemoryMB; /* original 0x009cf1e0 */
int32_t sysVideoMemoryMB; /* original 0x009cf1e4 */

enum {
    SYS_CPUID_VENDOR_LEAF = 0,
    SYS_CPUID_FEATURES_LEAF = 1,
    SYS_CPU_MEASUREMENT_MSEC = 250,
    SYS_BYTES_PER_MEGABYTE = 1024 * 1024,
    SYS_MIN_AVAILABLE_VIRTUAL_MEMORY = 128 * 1024 * 1024,
    SYS_PHYSICAL_MEMORY_CAP_MB = 1024,
    SYS_LOW_MEMORY_THRESHOLD_BYTES = 96 * 1024 * 1024,
    SYS_LOW_MEMORY_DIALOG_FLAGS = 52,
    SYS_DIALOG_RESULT_YES = 6
};

/* Exact .rdata qword at 0x005b9bd0: 0x3fdfffffff000000. */
static const double sysRoundHalfBias = 0.49999999906867743;

#if defined(_M_IX86) || defined(_M_X64) || \
    defined(__i386__) || defined(__x86_64__)
/* Source: CoDUOMP.exe 0x0046e0c0..0x0046e107.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e0c0_0046e108.mcode.
 * Role name: the original Windows-only CPUID leaf wrapper. */
static void Sys_ReadCpuid(uint32_t leaf, uint32_t registers[4])
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    int values[4];

    __cpuid(values, (int)leaf);
    registers[0] = (uint32_t)values[0];
    registers[1] = (uint32_t)values[1];
    registers[2] = (uint32_t)values[2];
    registers[3] = (uint32_t)values[3];
#elif defined(__i386__) || defined(__x86_64__)
    __cpuid_count(
        leaf, 0, registers[0], registers[1],
        registers[2], registers[3]);
#else
    (void)leaf;
    registers[0] = 0;
    registers[1] = 0;
    registers[2] = 0;
    registers[3] = 0;
#endif
}

/* Source: CoDUOMP.exe 0x0046e110..0x0046e14a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e110_0046e14b.mcode.
 * Role name: the original toggles EFLAGS.ID to determine whether CPUID is
 * available. All processors supported by the modern x86 build targets expose
 * CPUID; non-x86 targets do not use this Windows CPU classifier. */
static qboolean Sys_CpuidSupported(void)
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return qtrue;
#elif defined(__i386__) || defined(__x86_64__)
    return __get_cpuid_max(0, NULL) != 0 ? qtrue : qfalse;
#else
    return qfalse;
#endif
}

/* Source: CoDUOMP.exe 0x0046e150..0x0046e1b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e150_0046e1ba.mcode.
 * Role name: extended CPUID leaf 0x80000001 EDX bit 31 is 3DNow. */
static qboolean Sys_CpuHas3DNow(void)
{
    uint32_t registers[4];

    /* 0x0046e15e..0x0046e169 issues leaf zero before querying the extended
     * leaf range. Its result is deliberately overwritten by the next call. */
    Sys_ReadCpuid(SYS_CPUID_VENDOR_LEAF, registers);
    Sys_ReadCpuid(UINT32_C(0x80000000), registers);
    if (registers[0] < UINT32_C(0x80000000))
        return qfalse;

    Sys_ReadCpuid(UINT32_C(0x80000001), registers);
    return (registers[3] & UINT32_C(0x80000000)) != 0
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0046e1c0..0x0046e1eb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e1c0_0046e1ec.mcode.
 * Role name: CPUID leaf 1 EDX bit 25 is SSE. */
static qboolean Sys_CpuHasSse(void)
{
    uint32_t registers[4];

    Sys_ReadCpuid(SYS_CPUID_FEATURES_LEAF, registers);
    return (registers[3] & (UINT32_C(1) << 25)) != 0
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0046e1f0..0x0046e21b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e1f0_0046e21c.mcode.
 * Role name: CPUID leaf 1 EDX bit 23 is MMX. */
static qboolean Sys_CpuHasMmx(void)
{
    uint32_t registers[4];

    Sys_ReadCpuid(SYS_CPUID_FEATURES_LEAF, registers);
    return (registers[3] & (UINT32_C(1) << 23)) != 0
               ? qtrue
               : qfalse;
}
#endif

/* Source: CoDUOMP.exe 0x0046e220..0x0046e269.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e220_0046e26a.mcode and the
 * CPU-description switch at 0x0046bf76..0x0046c044.
 * Exact Windows source name is unavailable; the role name distinguishes this
 * feature classifier from the public Sys_GetProcessorId platform-id helper.
 * The enum values are proven by the switch table and its exact strings. */
sysCpuClass_t Sys_DetectCpuClass(void)
{
#if !defined(_M_IX86) && !defined(_M_X64) && \
    !defined(__i386__) && !defined(__x86_64__)
    /* NOT_FROM_ORIGINAL_SOURCE: the shipped Windows executable only ran on
     * x86. Native non-x86 hosts are represented by the original generic class
     * instead of being mislabeled as an unsupported pre-Pentium x86 CPU. */
    return CPUID_GENERIC;
#else
    if (Sys_CpuidSupported() == qfalse)
        return CPUID_INTEL_UNSUPPORTED;
    if (Sys_CpuHasMmx() == qfalse)
        return CPUID_INTEL_PENTIUM;
    if (Sys_CpuHas3DNow() != qfalse)
        return CPUID_AMD_3DNOW;
    if (Sys_CpuHasSse() != qfalse)
        return CPUID_INTEL_KATMAI;
    return CPUID_INTEL_MMX;
#endif
}

/* Source: CoDUOMP.exe 0x00468f50..0x00468f70.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00468f50_00468f71.mcode.
 * Exact source name is unavailable because this Windows-only helper has no
 * direct call site and no same-module Mac counterpart. The x87 body adds the
 * exact double at 0x005b9bd0 before FISTP converts under the active rounding
 * mode. Masked invalid conversions produce INT32_MIN. */
int32_t Sys_RoundPositiveFloatToInt(float value)
{
    const double rounded =
        rint((double)value + sysRoundHalfBias);

    if (!(rounded >= (double)INT32_MIN &&
          rounded <= (double)INT32_MAX)) {
        return INT32_MIN;
    }
    return (int32_t)rounded;
}

/* Source: CoDUOMP.exe 0x00468f80..0x0046904f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00468f80_00469050.mcode.
 * Role name: the sole caller stores the result as sys_cpuMHz and later prints
 * it as GHz after multiplying by 0.001. The Windows path preserves the
 * Sleep/QPC/RDTSC order and the original 250 ms observation interval. */
double Sys_GetCpuFrequencyMHz(void)
{
#if defined(_WIN32) && \
    (defined(_M_IX86) || defined(_M_X64) || \
     defined(__i386__) || defined(__x86_64__))
    LARGE_INTEGER performanceFrequency;
    LARGE_INTEGER counter;
    LARGE_INTEGER counterStart;
    LARGE_INTEGER counterEnd;
    uint64_t cyclesStart;
    uint64_t cyclesEnd;

    Sleep(0);
    performanceFrequency.QuadPart = 0;
    counter.QuadPart = 0;
    counterStart.QuadPart = 0;
    counterEnd.QuadPart = 0;
    QueryPerformanceFrequency(&performanceFrequency);
    QueryPerformanceCounter(&counter);
    cyclesStart = __rdtsc();
    QueryPerformanceCounter(&counterStart);
    Sleep(SYS_CPU_MEASUREMENT_MSEC);
    cyclesEnd = __rdtsc();
    QueryPerformanceCounter(&counterEnd);

    return (double)((cyclesEnd - cyclesStart) *
                    (uint64_t)performanceFrequency.QuadPart) *
           0.000001 /
           (double)(counterEnd.QuadPart - counterStart.QuadPart);
#elif defined(__APPLE__)
    uint64_t frequencyHz = 0;
    size_t frequencySize = sizeof(frequencyHz);

    /* NOT_FROM_ORIGINAL_SOURCE: native replacement for the Windows-only
     * RDTSC/QPC measurement on Apple targets, including Apple Silicon. */
    if (sysctlbyname("hw.cpufrequency_max", &frequencyHz, &frequencySize,
                     NULL, 0) != 0) {
        frequencySize = sizeof(frequencyHz);
        if (sysctlbyname("hw.cpufrequency", &frequencyHz, &frequencySize,
                         NULL, 0) != 0) {
            return 0.0;
        }
    }
    return (double)frequencyHz * 0.000001;
#elif defined(__i386__) || defined(__x86_64__)
    struct timespec delay = {
        0, SYS_CPU_MEASUREMENT_MSEC * 1000000L
    };
    struct timespec counterStart;
    struct timespec counterEnd;
    uint64_t cyclesStart;
    uint64_t cyclesEnd;
    double elapsedSeconds;

    /* NOT_FROM_ORIGINAL_SOURCE: POSIX equivalent of the Windows
     * RDTSC/QPC measurement for native Linux x86 builds. */
    clock_gettime(CLOCK_MONOTONIC, &counterStart);
    cyclesStart = __rdtsc();
    nanosleep(&delay, NULL);
    cyclesEnd = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &counterEnd);
    elapsedSeconds =
        (double)(counterEnd.tv_sec - counterStart.tv_sec) +
        (double)(counterEnd.tv_nsec - counterStart.tv_nsec) * 0.000000001;
    if (elapsedSeconds <= 0.0)
        return 0.0;
    return (double)(cyclesEnd - cyclesStart) * 0.000001 / elapsedSeconds;
#else
    return 0.0;
#endif
}

/* Source: CoDUOMP.exe 0x00469050..0x00469255.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469050_00469256.mcode.
 * Role name: the sole caller stores this value in the global printed as
 * "System memory is %i MB (capped at 1 GB)". Windows dynamically probes
 * GlobalMemoryStatusEx so the executable still runs on systems that only
 * provide GlobalMemoryStatus. */
int32_t Sys_GetPhysicalMemoryMB(void)
{
    uint64_t totalPhysicalBytes;

#if defined(_WIN32)
    typedef BOOL (WINAPI *global_memory_status_ex_t)(LPMEMORYSTATUSEX);
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    global_memory_status_ex_t getMemoryStatusEx = NULL;
    uint64_t availableVirtualBytes;

    if (kernel32 != NULL) {
        getMemoryStatusEx = (global_memory_status_ex_t)(uintptr_t)
            GetProcAddress(kernel32, "GlobalMemoryStatusEx");
    }

    if (getMemoryStatusEx != NULL) {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        getMemoryStatusEx(&status);
        availableVirtualBytes = status.ullAvailVirtual;
        totalPhysicalBytes = status.ullTotalPhys;
    } else {
        MEMORYSTATUS status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatus(&status);
        availableVirtualBytes = status.dwAvailVirtual;
        totalPhysicalBytes = status.dwTotalPhys;
    }

    if (availableVirtualBytes < SYS_MIN_AVAILABLE_VIRTUAL_MEMORY) {
        /* 0x0046909d..0x004690bd and 0x004691ab..0x004691cb resolve
         * the caption first, then the dialog body. */
        const char *title = Sys_LocalizeString("WIN_LOW_MEMORY_TITLE");
        const char *body = Sys_LocalizeString("WIN_LOW_MEMORY_BODY");
        if (MessageBoxA(NULL, body, title, SYS_LOW_MEMORY_DIALOG_FLAGS) !=
            SYS_DIALOG_RESULT_YES) {
            Sys_DeleteProcessLockFile();
            exit(0);
        }
    }
#elif defined(__APPLE__)
    size_t memorySize = sizeof(totalPhysicalBytes);

    /* NOT_FROM_ORIGINAL_SOURCE: native physical-memory query replacing the
     * Windows GlobalMemoryStatusEx/GlobalMemoryStatus compatibility path. */
    if (sysctlbyname("hw.memsize", &totalPhysicalBytes, &memorySize,
                     NULL, 0) != 0) {
        totalPhysicalBytes = 0;
    }
#else
    long pageCount;
    long pageSize;

    /* NOT_FROM_ORIGINAL_SOURCE: POSIX physical-memory query replacing the
     * Windows GlobalMemoryStatusEx/GlobalMemoryStatus compatibility path. */
    pageCount = sysconf(_SC_PHYS_PAGES);
    pageSize = sysconf(_SC_PAGESIZE);
    if (pageCount <= 0 || pageSize <= 0)
        totalPhysicalBytes = 0;
    else
        totalPhysicalBytes = (uint64_t)pageCount * (uint64_t)pageSize;
#endif

    const float physicalMegabytes =
        (float)totalPhysicalBytes * (1.0f / SYS_BYTES_PER_MEGABYTE);
    int32_t roundedMegabytes =
        Sys_RoundPositiveFloatToInt(physicalMegabytes);

    /* 0x0046912f..0x00469179 and 0x0046921a..0x00469241 compare
     * the signed FISTP result multiplied by one MiB with the unsigned
     * physical-byte count. A masked-invalid INT32_MIN result cannot match. */
    if (roundedMegabytes < 0 ||
        (uint64_t)roundedMegabytes * SYS_BYTES_PER_MEGABYTE !=
            totalPhysicalBytes ||
        roundedMegabytes > SYS_PHYSICAL_MEMORY_CAP_MB) {
        roundedMegabytes = SYS_PHYSICAL_MEMORY_CAP_MB;
    }
    return roundedMegabytes;
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
qboolean Sys_LowPhysicalMemory(void)
{
    return sysPhysicalMemoryMB <= SYS_LOW_MEMORY_THRESHOLD_BYTES
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x00469960..0x0046998f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469960_00469990.mcode.
 * Provisional role name: this Windows-only routine executes CPUID leaf 1,
 * tests EDX bit 25, and executes XORPS once before returning true. The Mac
 * binary has no corresponding SSE probe. Non-x86 targets must return false so
 * the renderer never selects its x86-specific SSE surface handlers. */
qboolean Sys_DetectSSESupport(void)
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    int registers[4];

    __cpuid(registers, 1);
    if ((registers[3] & (1 << 25)) == 0)
        return qfalse;
#if defined(_M_IX86)
    __asm {
        xorps xmm0, xmm0
    }
#endif
    return qtrue;
#elif defined(__i386__) || defined(__x86_64__)
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0 &&
        (edx & bit_SSE) != 0) {
        __asm__ volatile("xorps %%xmm0, %%xmm0" : : : "xmm0");
        return qtrue;
    }
#endif
    return qfalse;
}
