#ifndef CODUO_MSVC_COMPAT_H
#define CODUO_MSVC_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t coduo_crt_atoi(const char *string);
int32_t coduo_crt_stricmp(const char *left, const char *right);
int32_t coduo_crt_strnicmp(const char *left, const char *right, size_t count);
char *coduo_crt_strlwr(char *text);
char *coduo_crt_strupr(char *text);
int32_t coduo_crt_isalpha(int32_t character);
int32_t coduo_crt_isalnum(int32_t character);
int32_t coduo_crt_isspace(int32_t character);
int32_t coduo_crt_tolower(int32_t character);
int32_t coduo_crt_toupper(int32_t character);

#ifdef __cplusplus
}
#endif

#endif
