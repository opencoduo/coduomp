#ifndef CODUO_CRT_ATOI_COMPAT_H
#define CODUO_CRT_ATOI_COMPAT_H

#include <stdlib.h>
#include "compat/crt/msvc_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE: select the original platform's integer parser.
 * Windows accumulates modulo 2^32 even when the decimal text exceeds the
 * integer range; the host CRT may instead saturate at its native long limit. */
#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif defined(WINDOWS_BEHAVIOR)
#define coduo_compat_atoi coduo_crt_atoi
#elif defined(LINUX_BEHAVIOR)
#define coduo_compat_atoi atoi
#else
#error "Integer parsing compatibility requires a behavior selection"
#endif

#endif
