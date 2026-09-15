#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "q_shared.h"

#include <limits.h>
#include <math.h>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#else
#include <time.h>
#endif

qboolean sysMillisecondsInitialized; /* original 0x0389fddc */
uint32_t sysMillisecondsBase;         /* original 0x0489bc3c */
static char sysDateTimeStamp[16];     /* original 0x009d0504..0x009d0513 */

enum {
    SYS_DATE_BUFFER_BYTES = 10,
    SYS_TIME_BUFFER_BYTES = 7,
    SYS_TIME_BUFFER_OFFSET = 9
};

/* NOT_FROM_ORIGINAL_SOURCE: portable boundary replacing the original direct
 * WinMM timeGetTime import. Both implementations intentionally retain the
 * original wrapping uint32_t millisecond counter. */
static uint32_t Sys_RawMilliseconds(void)
{
#if defined(_WIN32)
    return (uint32_t)timeGetTime();
#else
    struct timespec now;

    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((uint64_t)now.tv_sec * UINT64_C(1000) +
                      (uint64_t)now.tv_nsec / UINT64_C(1000000));
#endif
}

/* Source: CoDUOMP.exe 0x0046dff0..0x0046e01b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046dff0_0046e01b.mcode.
 * Name: exact same-module Mac symbol Sys_Milliseconds. */
uint32_t Sys_Milliseconds(void)
{
    if (sysMillisecondsInitialized == qfalse) {
        sysMillisecondsBase = Sys_RawMilliseconds();
        sysMillisecondsInitialized = qtrue;
    }

    return Sys_RawMilliseconds() - sysMillisecondsBase;
}

/* Source: CoDUOMP.exe 0x0046e020..0x0046e05b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e020_0046e05c.mcode.
 * Name: exact same-module Mac symbol Sys_DateTimeStamp. The Windows calls
 * produce "yyyyMMdd-HHmmss" in one static sixteen-byte buffer; the second
 * call intentionally begins over the date call's terminating byte. */
char *Sys_DateTimeStamp(void)
{
#if defined(_WIN32)
    (void)GetDateFormatA(
        LOCALE_SYSTEM_DEFAULT, 0, NULL, "yyyyMMdd-",
        sysDateTimeStamp, SYS_DATE_BUFFER_BYTES);
    (void)GetTimeFormatA(
        LOCALE_SYSTEM_DEFAULT, 0, NULL, "HHmmss",
        &sysDateTimeStamp[SYS_TIME_BUFFER_OFFSET],
        SYS_TIME_BUFFER_BYTES);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native libc replacement for the two Win32
     * locale-formatting calls. The explicit formats preserve the boundary
     * string consumed by cgame. */
    const time_t now = time(NULL);
    struct tm localTime;

    if (localtime_r(&now, &localTime) == NULL) {
        sysDateTimeStamp[0] = '\0';
        return sysDateTimeStamp;
    }
    (void)strftime(
        sysDateTimeStamp, SYS_DATE_BUFFER_BYTES,
        "%Y%m%d-", &localTime);
    (void)strftime(
        &sysDateTimeStamp[SYS_TIME_BUFFER_OFFSET],
        SYS_TIME_BUFFER_BYTES, "%H%M%S", &localTime);
#endif
    return sysDateTimeStamp;
}

/* Source: CoDUOMP.exe 0x0046e060..0x0046e06f, recovered from the executable
 * gap between Sys_DateTimeStamp and Sys_SnapVector.
 * Exact source name is unavailable: there are no direct callers and no Mac
 * counterpart. The role name states the complete proven behavior: load one
 * float, FISTP it under the active rounding mode, and return the signed dword.
 * Masked x87 invalid conversions produce INT32_MIN. */
int32_t Sys_RoundFloatToInt(float value)
{
    const double rounded = rint((double)value);

    if (!(rounded >= (double)INT32_MIN &&
          rounded <= (double)INT32_MAX)) {
        return INT32_MIN;
    }
    return (int32_t)rounded;
}

/* Source: CoDUOMP.exe 0x0046e070..0x0046e0b4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e070_0046e0b5.mcode.
 * Name: exact same-module Mac symbol Sys_SnapVector. Each input float is
 * rounded according to the active floating-point rounding mode and stored
 * back as an exactly integral float. */
void Sys_SnapVector(vec3_t vector)
{
    for (int32_t component = 0; component < 3; ++component) {
        /* FISTP first produces the target signed dword (including INT32_MIN
         * for masked invalid conversions), then FILD/FSTP converts that
         * integer back to a float. */
        vector[component] =
            (float)Sys_RoundFloatToInt(vector[component]);
    }
}
