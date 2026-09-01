#ifndef CODUOMP_CRT_BOUNDARY_H
#define CODUOMP_CRT_BOUNDARY_H

#include "client/common/client_legacy_crt.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/crt/format_compat.h"
#include "compat/crt/random_compat.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int coduomp_crt_mkdir(const char *path);
char *coduomp_crt_getcwd(char *buffer, size_t size);
int coduomp_crt_putenv_copy(const char *assignment);

#ifdef __cplusplus
}
#endif

#endif
