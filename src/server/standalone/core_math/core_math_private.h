#ifndef CODUO_CORE_MATH_PRIVATE_H
#define CODUO_CORE_MATH_PRIVATE_H

#include <float.h>
#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "math/q_math.h"

#define CODUO_FLOAT_SIGN_BIT (1U << 31)

#ifndef CODUO_ENGINE_HOST_LONG_DOUBLE_IS_X87
#define CODUO_ENGINE_HOST_LONG_DOUBLE_IS_X87 (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384)
#endif

#ifndef CODUO_ENGINE_HAS_X87_INLINE_ASM
#if (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(__x86_64__)) && \
    CODUO_ENGINE_HOST_LONG_DOUBLE_IS_X87 && (defined(__GNUC__) || defined(__clang__))
#define CODUO_ENGINE_HAS_X87_INLINE_ASM 1
#else
#define CODUO_ENGINE_HAS_X87_INLINE_ASM 0
#endif
#endif

void gunrandom(float *x, float *y);
void TriangleNormal(const vec3_t point, const vec3_t edgePoint0, const vec3_t edgePoint1, vec3_t out);
void MatrixTransformPoint43Compact(const vec3_t in, const matrix43_t *matrix, vec3_t out);
void MatrixTransformPoint43Affine(const vec3_t in, const DObjSkelMat *matrix, vec3_t out);
void MatrixTransformPoint43CompactInPlace(vec3_t point, const matrix43_t *matrix);
#endif
