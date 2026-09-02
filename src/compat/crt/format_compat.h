#ifndef CODUO_FORMAT_COMPAT_H
#define CODUO_FORMAT_COMPAT_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int coduo_crt_vsnprintf(char *destination, size_t count, const char *format, va_list arguments);
int coduo_crt_snprintf(char *destination, size_t count, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
