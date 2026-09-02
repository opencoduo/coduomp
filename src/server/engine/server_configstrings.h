#ifndef SHARED_SERVER_CONFIGSTRINGS_H
#define SHARED_SERVER_CONFIGSTRINGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_SetConfigstring(int32_t index, const char *value);
void SV_GetConfigstring(int32_t index, char *buffer, int32_t bufferSize);
const char *SV_GetConfigstringConst(int32_t index);
const char *SV_GetConfigValueForKey(int32_t base, int32_t count,
                                    const char *key);
void SV_SetConfigValueForKey(int32_t base, int32_t count,
                             const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif
