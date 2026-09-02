#ifndef CODUO_CRT_QSORT_COMPAT_H
#define CODUO_CRT_QSORT_COMPAT_H

#include <stddef.h>

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * Client-only original call sites use coduo_crt_qsort and therefore always
 * use the statically linked MSVC behavior. Shared server code uses coduo_qsort,
 * selected by WINDOWS_BEHAVIOR or LINUX_BEHAVIOR.
 */
void coduo_crt_qsort(void *base, size_t count, size_t width, int (*compare)(const void *, const void *));
void coduo_qsort(void *base, size_t count, size_t width, int (*compare)(const void *, const void *));

#endif /* CODUO_CRT_QSORT_COMPAT_H */
