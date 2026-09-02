#ifndef CODUO_COLLISION_GEOMETRY_H
#define CODUO_COLLISION_GEOMETRY_H

#include "qcommon/q_vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void CM_ProjectPointOntoLine(const vec3_t point, const vec3_t linePoint, const vec3_t lineDirection, vec3_t projected);
long double CM_DistanceSquaredPointToSegment(const vec3_t point, const vec3_t start, const vec3_t end, const vec3_t direction);
long double CM_DistanceSquared(const vec3_t start, const vec3_t end);
long double CM_FastSqrt(float value);
void CreateRotationMatrix(const vec3_t angles, axis_t matrix);
void RotatePoint(vec3_t point, const axis_t matrix);
void TransposeMatrix(const axis_t input, axis_t output);

#ifdef __cplusplus
}
#endif

#endif
