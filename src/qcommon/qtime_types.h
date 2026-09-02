#ifndef QCOMMON_QTIME_TYPES_H
#define QCOMMON_QTIME_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* REAL_TIME syscall record. All engine and module binaries copy the same nine
 * consecutive 32-bit localtime fields. */
typedef struct qtime_s {
    int32_t tm_sec;
    int32_t tm_min;
    int32_t tm_hour;
    int32_t tm_mday;
    int32_t tm_mon;
    int32_t tm_year;
    int32_t tm_wday;
    int32_t tm_yday;
    int32_t tm_isdst;
} qtime_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(qtime_t) == 0x24,
               "qtime_t size mismatch");
_Static_assert(offsetof(qtime_t, tm_sec) == 0x00,
               "qtime_t.tm_sec offset mismatch");
_Static_assert(offsetof(qtime_t, tm_isdst) == 0x20,
               "qtime_t.tm_isdst offset mismatch");
#endif

#endif
