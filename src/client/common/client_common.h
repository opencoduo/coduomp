#ifndef CLIENT_COMMON_H
#define CLIENT_COMMON_H

#include "qcommon/q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

float Com_ClampFloat(float minimum, float maximum, float value);
void Com_FormatLocalizedFloat(char *buffer, uint32_t bufferSize,
                              int32_t precision, language_t language,
                              float value);

#ifdef __cplusplus
}
#endif

#endif
