#ifndef CODUOMP_CASE_SENSITIVE_FS_H
#define CODUOMP_CASE_SENSITIVE_FS_H

#include "../q_shared.h"

#include <stddef.h>
#include <stdio.h>

#ifndef CASE_SENSITIVE_FS
#if defined(__linux__)
#define CASE_SENSITIVE_FS 1
#else
#define CASE_SENSITIVE_FS 0
#endif
#endif

#if CASE_SENSITIVE_FS && defined(_WIN32)
#error CASE_SENSITIVE_FS is not supported for Windows builds
#endif

qboolean coduomp_resolve_case_path(const char *trustedRoot, const char *requestedPath, char *resolvedPath, size_t resolvedPathSize);
FILE *coduomp_fopen_case_read(const char *trustedRoot, const char *requestedPath);
void coduomp_case_path_cache_clear(void);

#endif
