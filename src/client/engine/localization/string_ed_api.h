#ifndef CODUOMP_STRING_ED_API_H
#define CODUOMP_STRING_ED_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *SEH_StringEd_GetString(const char *key);
void SEH_StringEd_Clear(int32_t preserveFlagData);

#ifdef __cplusplus
}
#endif

#endif
