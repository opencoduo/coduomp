#ifndef CODUO_COLLISION_POINT_CONTENTS_H
#define CODUO_COLLISION_POINT_CONTENTS_H

#include "qcommon/q_vector_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t CM_PointContents(const vec3_t point, int32_t modelHandle);
int32_t CM_TransformedPointContents(const vec3_t point, int32_t modelHandle, const vec3_t origin, const vec3_t angles);

#ifdef __cplusplus
}
#endif

#endif
