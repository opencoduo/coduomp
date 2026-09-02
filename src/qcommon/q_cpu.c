#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "q_cpu.h"

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

/* Source: CoDUOMP.exe 0x00488350..0x00488354 and coduo_lnxded
 * 0x080b0876..0x080b0890. Name and no-argument return type: exact Mac client
 * symbol rdtsc. Both authoritative i386 bodies execute RDTSC and return EAX,
 * the low counter word. Windows preserves EDX while Linux leaves the high
 * counter word there; EDX is caller-clobbered in both cdecl ABIs, so that
 * instruction-level difference is outside the source contract. */
uint32_t rdtsc(void)
{
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    return (uint32_t)__rdtsc();
#elif defined(__APPLE__)
    /* The supporting Mac client reads the PowerPC time base. */
    return (uint32_t)mach_absolute_time();
#else
    struct timespec counter;

    (void)clock_gettime(CLOCK_MONOTONIC, &counter);
    return (uint32_t)((uint64_t)counter.tv_sec * UINT64_C(1000000000) + (uint64_t)counter.tv_nsec);
#endif
}
