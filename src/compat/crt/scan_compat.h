#ifndef CODUO_CRT_SCAN_COMPAT_H
#define CODUO_CRT_SCAN_COMPAT_H

#include <stdio.h>
#include "compat/crt/msvc_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE: select the original numeric scanner for the
 * audited scalar and vector formats, independently of the build host. */
#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif defined(WINDOWS_BEHAVIOR)
#define coduo_compat_scan_float coduo_crt_scan_float
#define coduo_compat_scan_vec3 coduo_crt_scan_vec3
#define coduo_compat_scan_int coduo_crt_scan_int
#elif defined(LINUX_BEHAVIOR)
#define coduo_compat_scan_float(text, output) sscanf((text), "%f", (output))
#define coduo_compat_scan_int(text, output) sscanf((text), "%d", (output))

/* NOT_FROM_ORIGINAL_SOURCE: evaluate the vector pointer once while retaining
 * the Linux scanner's original format and individual lane addresses. */
static inline int coduo_compat_scan_vec3(const char *text, float *output)
{
    return sscanf(text, "%f %f %f", &output[0], &output[1], &output[2]);
}
#else
#error "Numeric scanning compatibility requires a behavior selection"
#endif

#endif
