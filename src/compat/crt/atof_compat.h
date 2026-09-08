#ifndef CODUO_CRT_ATOF_COMPAT_H
#define CODUO_CRT_ATOF_COMPAT_H

#include <stdlib.h>
#include "compat/crt/msvc_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE: select the original platform's decimal parser
 * independently of the host platform used to build or run the recovered code. */
#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif defined(WINDOWS_BEHAVIOR)
#define coduo_compat_atof coduo_crt_atof
#elif defined(LINUX_BEHAVIOR)
#define coduo_compat_atof atof
#else
#error "Decimal parsing compatibility requires a behavior selection"
#endif

#endif
