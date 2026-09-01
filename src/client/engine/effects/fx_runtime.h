#ifndef CODUOMP_FX_RUNTIME_H
#define CODUOMP_FX_RUNTIME_H

#include "compat/coduo_fp_conversion.h"

#include <stdint.h>

#include "../animation/dobj.h"

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *fx_debugBolt; /* 0x04dc8824 */

int32_t FX_GetBoneIndex(int32_t entityNum, const char *tagName);
void FX_SetWind(const vec2_t angles, float strength);
void ClampVec(const vec3_t color, uint8_t colorBytes[3]);

#ifdef __cplusplus
}
#endif

#endif
