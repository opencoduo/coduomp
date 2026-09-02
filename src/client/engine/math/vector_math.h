#ifndef CODUOMP_VECTOR_MATH_H
#define CODUOMP_VECTOR_MATH_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t FastFloor(float value);
void gunrandom(float *x, float *y);
void AxisTransformVector(const axis_t axis, float x, float y, float z, vec3_t transformed);
#ifdef __cplusplus
}
#endif

#endif
