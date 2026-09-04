#ifndef CODUO_SHARED_Q_MATH_H
#define CODUO_SHARED_Q_MATH_H

#include <stdint.h>

#include "qcommon/q_collision_types.h"
#include "q_matrix_types.h"
#include "qcommon/q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUMVERTEXNORMALS 162

extern vec3_t bytedirs[NUMVERTEXNORMALS];
extern const vec3_t vec3_origin;
extern uint32_t sharedRandSeed;

int32_t Q_rand(int32_t *seed);
float Q_random(int32_t *seed);
float Q_crandom(int32_t *seed);
void Rand_Init(uint32_t seed);
float flrand(float minimum, float maximum);
int32_t irand(int32_t minimum, int32_t maximum);
float Q_SwayRand(float sineRate, float cosineRate, float milliseconds);
float Q_fabs(float value);
float GetLeanFraction(float fraction);
float UnGetLeanFraction(float fraction);
void AddLeanToPosition(vec3_t position, float yaw, float leanFraction,
                       float maxLean, float side);
void BG_SinCos(float angle, float *sinOut, float *cosOut);
void CG_SinCos(double angle, double *sinOut, double *cosOut);
void VectorAngleMultiply(vec2_t vector, float angleDegrees);
void PitchToQuaternion(float degrees, vec4_t quaternion);
void YawToQuaternion(float degrees, vec4_t quaternion);
void RollToQuaternion(float degrees, vec4_t quaternion);
void ConvertQuatToMat(float quaternionAndMatrix[9]);
long double QuatEigenTrace(const vec4_t quaternion);
float AngleEigenTrace(float degrees);
long double QuatRatioEigenTrace(const vec4_t left, const vec4_t right);
float RotationToYaw(const vec2_t rotation);
int32_t FastRound(float value);
float Q_acos(float value);
float _DotProduct(const vec3_t left, const vec3_t right);
float _VectorLength(const vec3_t vector);
float VectorDistance(const vec3_t first, const vec3_t second);
float VectorDistanceSquared(const vec3_t first, const vec3_t second);
float VectorDistance2D(const vec3_t first, const vec3_t second);
float VectorDistanceSquared2D(const vec3_t first, const vec3_t second);
float VectorMax(const vec3_t vector);
float vectoyaw(const vec3_t direction);
float vectosignedyaw(const vec2_t direction);
float vectopitch(const vec3_t direction);
float vectosignedpitch(const vec3_t direction);
void vectoangles(const vec3_t direction, vec3_t angles);
void vectosignedangles(const vec3_t direction, vec3_t angles);
void AxisToAngles(const axis_t axis, vec3_t angles);
void Axis4ToAngles(const DObjSkelMat *matrix, vec3_t angles);
void AxisToSignedAngles(const axis_t axis, vec3_t angles);
void AnglesToAxis(const vec3_t angles, axis_t axis);
void OrientationPosToWorldPos(const orientation_t *orientation,
                              const vec3_t position, vec3_t worldPosition);
void OrientationDirToWorldDir(const orientation_t *orientation,
                              const vec3_t direction, vec3_t worldDirection);
void OrientationPosFromWorldPos(const orientation_t *orientation,
                                const vec3_t worldPosition, vec3_t position);
void OrientationDirFromWorldDir(const orientation_t *orientation,
                                const vec3_t worldDirection, vec3_t direction);
void MatrixTransposeTransformVector43(const vec3_t input,
                                      const matrix43_t *matrix,
                                      vec3_t output);
void MatrixTransformVector43Equals(vec3_t vector,
                                   const matrix43_t *matrix);
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs);
uint32_t ColorBytes3(float red, float green, float blue);
uint32_t ColorBytes4(float red, float green, float blue, float alpha);
float NormalizeColor(const vec3_t input, vec3_t output);
float ColorNormalize(const vec3_t input, vec3_t output);
float LerpAngle(float from, float to, float fraction);
float AngleMod(float angle);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);
float AngleNormalize360Accurate(float angle);
float AngleNormalize180Accurate(float angle);
float AngleDelta(float first, float second);
float AngleSubtract(float first, float second);
void AnglesSubtract(const vec3_t first, const vec3_t second, vec3_t result);
int32_t Q_log2(int32_t value);
int8_t ClampChar(int32_t value);
int16_t ClampShort(int32_t value);
uint8_t DirToByte(const vec3_t direction);
void ByteToDir(int32_t value, vec3_t direction);
void SetPlaneSignbits(cplane_t *plane);
qboolean BoxDistSqrdExceeds(const vec3_t mins, const vec3_t maxs,
                            const vec3_t point, float distanceSquared);
float Q_rint(float value);
void VectorRotateAngles(const vec3_t input, const vec3_t angles,
                        vec3_t output);
void VectorRotateAnglesAroundPoint(const vec3_t point,
                                   const vec3_t angles,
                                   const vec3_t origin,
                                   vec3_t output);
enum {
    BOX_ON_PLANE_SIDE_FRONT = 1,
    BOX_ON_PLANE_SIDE_BACK = 2,
    BOX_ON_PLANE_SIDE_CROSS =
        BOX_ON_PLANE_SIDE_FRONT | BOX_ON_PLANE_SIDE_BACK
};
int32_t BoxOnPlaneSide(const vec3_t mins, const vec3_t maxs,
                       const cplane_t *plane);
void _VectorSubtract(const vec3_t first, const vec3_t second, vec3_t result);
void _VectorAdd(const vec3_t first, const vec3_t second, vec3_t result);
void _VectorCopy(const vec3_t source, vec3_t destination);
void _VectorScale(const vec3_t vector, float scale, vec3_t result);
void _VectorMA(const vec3_t start, float scale, const vec3_t direction,
               vec3_t result);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right,
                  vec3_t up);
void YawVectors(float yaw, vec3_t forward, vec3_t right);
void YawToAxis(float yaw, axis_t axis);
float PitchForYawOnNormal(float yaw, const vec3_t normal);
void VectorSnap(vec3_t vector);
float Q_rsqrt(float value);
float VectorNormalize2D(vec2_t vector);
float VectorNormalize(vec3_t vector);
float FloatRoundNearest(float value);
float RoundFloat(float value, int32_t decimals);
float VectorNormalize4D(vec4_t vector);
float VectorNormalize2(const vec3_t input, vec3_t output);
void VectorNormalizeFast(vec3_t vector);
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);
void PerpendicularVector(vec3_t output, const vec3_t source);
int32_t PlaneFromPoints(vec4_t plane, const vec3_t point0,
                        const vec3_t point1, const vec3_t point2);
void ProjectPointOnPlane(vec3_t output, const vec3_t point,
                         const vec3_t normal);
int32_t VectorCompareEpsilon(const vec3_t first, const vec3_t second);
void CrossProduct(const vec3_t left, const vec3_t right, vec3_t output);
void QuatMultiply(const vec4_t first, const vec4_t second, vec4_t product);
void VectorInverse(vec3_t vector);
void Vector4Scale(const vec4_t input, float scale, vec4_t output);
void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs);
void ExpandBounds(const vec3_t addMins, const vec3_t addMaxs,
                  vec3_t mins, vec3_t maxs);
void AxisClear(axis_t axis);
void AxisCopy(const axis_t input, axis_t output);
void MatrixTranspose(const axis_t input, axis_t output);
void MatrixTransposeTransformVector(const vec3_t vector, axis_t matrix,
                                    vec3_t transformed);
void MatrixTransformVector43(const vec3_t input,
                             const matrix43_t *matrix, vec3_t output);
void LocalMatrixTransformVector43(const vec3_t input,
                                  const float matrix[16], vec3_t output);
void DObjSkelMatrixTransformVector43(const vec3_t input,
                                     const float matrix[16], vec3_t output);
void DObjQuatToMatrix43(const vec4_t quat, DObjSkelMat *matrix);
void DObjMatrixTransformVector43InPlace(vec3_t point,
                                        const DObjSkelMat *matrix);
void DObjMatrixTransformVector43(const vec3_t point,
                                 const DObjSkelMat *matrix,
                                 vec3_t output);
void DObjMatrixInverseTransformVector43(const vec3_t point,
                                        const DObjSkelMat *matrix,
                                        vec3_t output);
void XSurfaceTransformPoint43(const vec3_t point,
                              const DObjSkelMat *matrix,
                              vec3_t output);
void XSurfaceAccumulateWeightedPoint43(
    const vec3_t point, float weight,
    const DObjSkelMat *matrix, vec3_t output);
void XSurfaceTransformNormal43(const vec3_t normal,
                               const DObjSkelMat *matrix,
                               vec3_t output);
void XSurfaceTransformVectorRows43(
    const vec3_t vector, const DObjSkelMat *matrix,
    vec3_t output);
void DObjQuatMultiplyIntoFirst(vec4_t quat, const vec4_t rhs);
void DObjQuatMultiplyIntoSecond(const vec4_t lhs, vec4_t quat);
void DObjSkelMatrixMultiply43(const DObjSkelMat *left,
                              const matrix43_t *right, matrix43_t *output);
void DObjSkel2MatrixMultiply43(const DObjSkelMat *left,
                               const matrix43_t *right,
                               DObjSkelMat *output);
void MatrixInverse(axis_t input, axis_t output);
void MatrixInverse44(float input[4][4], float output[4][4]);
void MatrixMultiply(axis_t left, axis_t right, axis_t output);
void MatrixMultiplyEquals(axis_t left, axis_t right);
void MatrixMultiply34(const float left[3][4], const float right[3][4],
                      float output[3][4]);
void MatrixMultiply43(const matrix43_t *left, const matrix43_t *right,
                      matrix43_t *output);
void MatrixTransformVector(const vec3_t input, const axis_t matrix,
                           vec3_t output);
void MatrixInverseOrthogonal43(const matrix43_t *input,
                               matrix43_t *output);
void VectorRotate(const vec3_t input, const axis_t matrix, vec3_t output);
void RotatePointAroundVector(vec3_t output, const vec3_t direction,
                             const vec3_t point, float degrees);
void RotateAroundDirection(axis_t axis, float yaw);
void NormalToLatLong(const vec3_t normal, uint8_t encoded[2]);
void VectorPolar(vec3_t output, float radius, float angle);
void CrossProductUp(const vec3_t input, vec3_t output);
void GetPerpendicularViewVector(const vec3_t point, const vec3_t point1,
                                const vec3_t point2, vec3_t perpendicular);
void ProjectPointOntoVector(const vec3_t point, const vec3_t lineStart,
                            const vec3_t lineEnd, vec3_t projected);
void QuatInverse(const vec4_t input, vec4_t output);
void Vec10Copy(const uint32_t input[10], uint32_t output[10]);
void _Vector5Add(const float first[5], const float second[5],
                 float result[5]);
void _Vector5Scale(const float input[5], float scale, float result[5]);
void _Vector53Copy(const uint32_t input[3], uint32_t result[3]);

#ifdef __cplusplus
}
#endif

#endif
