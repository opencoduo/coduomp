#ifndef CLIENT_LEGACY_CRT_H
#define CLIENT_LEGACY_CRT_H

#include "compat/crt/msvc_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * Compatibility boundary for C-runtime behavior emitted or imported by the
 * retail MSVC client images.  These names deliberately describe compatibility
 * services rather than recovered game functions.
 */
char *coduo_client_crt_strcpy(char *destination, const char *source);
#ifdef __cplusplus
}
#endif

#endif
