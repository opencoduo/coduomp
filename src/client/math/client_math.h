#ifndef CODUO_SHARED_CLIENT_MATH_H
#define CODUO_SHARED_CLIENT_MATH_H

#include "math/q_math.h"
#include "qcommon/q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void NormalFromPoints(const vec3_t point0, const vec3_t point1,
                      vec3_t normal, const vec3_t point2);
void ProjectPointOnLine(vec3_t output, const vec3_t start,
                        const vec3_t point, const vec3_t end);
void CG_ComposeBoneMatrix(const DObjSkelMat *parent,
                          const matrix43_t *local, DObjSkelMat *output);
void RotatePoint2D(vec2_t point, float degrees);
void QuatFromAngleY(vec4_t output, float angle);
void QuatFromAngleZ(vec4_t output, float angle);
void QuatFromAngleX(float angle, vec4_t output);
void AnglesToAxisNegRight(axis_t output, const vec3_t angles);
void RotatePointByAngles(const vec3_t point, const vec3_t angles,
                         vec3_t output);
void RotatePointAroundOrigin(const vec3_t point, const vec3_t origin,
                             const vec3_t angles, vec3_t output);
qboolean PointBoxDistSqExceeds(const vec3_t point, const vec3_t corner0,
                               const vec3_t corner1,
                               float maximumDistanceSq);
int32_t Script_BiasedRoundToInt(float value);
void SnapVector(vec3_t vector);
void VectorAdd5(const float left[5], const float right[5], float output[5]);
void VectorScale5(const float input[5], float scale, float output[5]);
void VectorCopy3Secondary(const vec3_t input, vec3_t output);

#ifdef __cplusplus
}
#endif

#endif
