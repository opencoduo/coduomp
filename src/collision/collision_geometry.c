#include "collision_geometry.h"

#include "compat/coduo_x87emu.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_geometry.c requires a platform behavior mode"
#endif

enum {
    CM_VECTOR_AXIS_COUNT = 3
};

#define CM_FAST_INV_SQRT_MAGIC UINT32_C(0x5f3759df)
#define CM_FLOAT_SIGN_BIT UINT32_C(0x80000000)

/*
 * Complete collision projection/distance cluster:
 *
 *   CoDUOMP.exe   0x00425e20..0x00425ff8
 *   coduo_lnxded  0x08057c66..0x08057fe2
 *
 * The signatures and geometric decisions agree.  The whole-function bodies
 * below preserve genuine compiler/platform differences in x87 operation
 * order and binary32 spill points.
 */

/* Windows retains all three point-line deltas in x87 registers and recomputes
 * the dot product as (Z+Y)+X for each output lane. */
#if defined(WINDOWS_BEHAVIOR)
void CM_ProjectPointOntoLine(const vec3_t point, const vec3_t linePoint, const vec3_t lineDirection, vec3_t projected)
{
#if EMULATE_X87
    const x87f deltaX = x87f_sub(x87f_load_f32(point[0]), x87f_load_f32(linePoint[0]));
    const x87f deltaY = x87f_sub(x87f_load_f32(point[1]), x87f_load_f32(linePoint[1]));
    const x87f deltaZ = x87f_sub(x87f_load_f32(point[2]), x87f_load_f32(linePoint[2]));

    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        const x87f dot =
            x87f_add(x87f_add(x87f_mul(deltaZ, x87f_load_f32(lineDirection[2])), x87f_mul(deltaY, x87f_load_f32(lineDirection[1]))),
                     x87f_mul(deltaX, x87f_load_f32(lineDirection[0])));
        projected[axis] = x87f_store_f32(x87f_add(x87f_mul(dot, x87f_load_f32(lineDirection[axis])), x87f_load_f32(linePoint[axis])));
    }
#else
    const long double deltaX = (long double)point[0] - (long double)linePoint[0];
    const long double deltaY = (long double)point[1] - (long double)linePoint[1];
    const long double deltaZ = (long double)point[2] - (long double)linePoint[2];

    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        const long double dot =
            (deltaZ * (long double)lineDirection[2] + deltaY * (long double)lineDirection[1]) + deltaX * (long double)lineDirection[0];
        projected[axis] = (float)(dot * (long double)lineDirection[axis] + (long double)linePoint[axis]);
    }
#endif
}
#else
/* Linux stores each delta as binary32, then evaluates each dot as (X+Y)+Z. */
void CM_ProjectPointOntoLine(const vec3_t point, const vec3_t linePoint, const vec3_t lineDirection, vec3_t projected)
{
    vec3_t delta;

#if EMULATE_X87
    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        delta[axis] = x87f_store_f32(x87f_sub(x87f_load_f32(point[axis]), x87f_load_f32(linePoint[axis])));
    }
    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        const x87f dot = x87f_add(x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(lineDirection[0])),
                                           x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(lineDirection[1]))),
                                  x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(lineDirection[2])));
        projected[axis] = x87f_store_f32(x87f_add(x87f_mul(dot, x87f_load_f32(lineDirection[axis])), x87f_load_f32(linePoint[axis])));
    }
#else
    delta[0] = point[0] - linePoint[0];
    delta[1] = point[1] - linePoint[1];
    delta[2] = point[2] - linePoint[2];
    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        const long double dot =
            ((long double)delta[0] * (long double)lineDirection[0] + (long double)delta[1] * (long double)lineDirection[1]) +
            (long double)delta[2] * (long double)lineDirection[2];
        projected[axis] = (float)(dot * (long double)lineDirection[axis] + (long double)linePoint[axis]);
    }
#endif
}
#endif

/* The endpoint scan and strict nearest-endpoint choice agree.  Windows keeps
 * the selected point deltas and (Z+Y)+X squared sum live through return. */
#if defined(WINDOWS_BEHAVIOR)
long double CM_DistanceSquaredPointToSegment(const vec3_t point, const vec3_t start, const vec3_t end, const vec3_t direction)
{
    vec3_t projected;
    CM_ProjectPointOntoLine(point, start, direction, projected);

    int32_t axis;
    for (axis = 0; axis < CM_VECTOR_AXIS_COUNT && !(projected[axis] > start[axis] && projected[axis] > end[axis]) &&
                   !(start[axis] > projected[axis] && end[axis] > projected[axis]);
         ++axis) {
    }

    const float *endpoint;
    if (axis < CM_VECTOR_AXIS_COUNT) {
#if EMULATE_X87
        endpoint = x87f_lt_signaling(x87f_abs(x87f_sub(x87f_load_f32(projected[axis]), x87f_load_f32(start[axis]))),
                                     x87f_abs(x87f_sub(x87f_load_f32(projected[axis]), x87f_load_f32(end[axis]))))
                       ? start
                       : end;
#else
        endpoint =
            fabsl((long double)projected[axis] - (long double)start[axis]) < fabsl((long double)projected[axis] - (long double)end[axis])
                ? start
                : end;
#endif
    } else {
        endpoint = projected;
    }

#if EMULATE_X87
    const x87f deltaX = x87f_sub(x87f_load_f32(point[0]), x87f_load_f32(endpoint[0]));
    const x87f deltaY = x87f_sub(x87f_load_f32(point[1]), x87f_load_f32(endpoint[1]));
    const x87f deltaZ = x87f_sub(x87f_load_f32(point[2]), x87f_load_f32(endpoint[2]));
    const x87f squared = x87f_add(x87f_add(x87f_mul(deltaZ, deltaZ), x87f_mul(deltaY, deltaY)), x87f_mul(deltaX, deltaX));
    /* Windows PC=53 values are carried exactly by the host double return. */
    return (long double)x87f_store_f64(squared);
#else
    const long double deltaX = (long double)point[0] - (long double)endpoint[0];
    const long double deltaY = (long double)point[1] - (long double)endpoint[1];
    const long double deltaZ = (long double)point[2] - (long double)endpoint[2];
    return (deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX;
#endif
}
#else
/* Linux spills each selected point delta and the completed (X+Y)+Z sum to
 * binary32 before reloading the return value. */
long double CM_DistanceSquaredPointToSegment(const vec3_t point, const vec3_t start, const vec3_t end, const vec3_t direction)
{
    vec3_t projected;
    CM_ProjectPointOntoLine(point, start, direction, projected);

    int32_t axis;
    for (axis = 0; axis < CM_VECTOR_AXIS_COUNT && !(projected[axis] > start[axis] && projected[axis] > end[axis]) &&
                   !(start[axis] > projected[axis] && end[axis] > projected[axis]);
         ++axis) {
    }

    const float *endpoint;
    if (axis < CM_VECTOR_AXIS_COUNT) {
#if EMULATE_X87
        endpoint = x87f_lt(x87f_abs(x87f_sub(x87f_load_f32(projected[axis]), x87f_load_f32(start[axis]))),
                           x87f_abs(x87f_sub(x87f_load_f32(projected[axis]), x87f_load_f32(end[axis]))))
                       ? start
                       : end;
#else
        endpoint =
            fabsl((long double)projected[axis] - (long double)start[axis]) < fabsl((long double)projected[axis] - (long double)end[axis])
                ? start
                : end;
#endif
    } else {
        endpoint = projected;
    }

    vec3_t delta;
#if EMULATE_X87
    for (int32_t lane = 0; lane < CM_VECTOR_AXIS_COUNT; ++lane) {
        delta[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(point[lane]), x87f_load_f32(endpoint[lane])));
    }
    return (long double)x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])), x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2]))));
#else
    delta[0] = point[0] - endpoint[0];
    delta[1] = point[1] - endpoint[1];
    delta[2] = point[2] - endpoint[2];
    const float squared = (float)(((long double)delta[0] * (long double)delta[0] + (long double)delta[1] * (long double)delta[1]) +
                                  (long double)delta[2] * (long double)delta[2]);
    return (long double)squared;
#endif
}
#endif

#if defined(WINDOWS_BEHAVIOR)
long double CM_DistanceSquared(const vec3_t start, const vec3_t end)
{
#if EMULATE_X87
    const x87f deltaX = x87f_sub(x87f_load_f32(end[0]), x87f_load_f32(start[0]));
    const x87f deltaY = x87f_sub(x87f_load_f32(end[1]), x87f_load_f32(start[1]));
    const x87f deltaZ = x87f_sub(x87f_load_f32(end[2]), x87f_load_f32(start[2]));
    return (long double)x87f_store_f64(x87f_add(x87f_add(x87f_mul(deltaZ, deltaZ), x87f_mul(deltaY, deltaY)), x87f_mul(deltaX, deltaX)));
#else
    const long double deltaX = (long double)end[0] - (long double)start[0];
    const long double deltaY = (long double)end[1] - (long double)start[1];
    const long double deltaZ = (long double)end[2] - (long double)start[2];
    return (deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX;
#endif
}
#else
long double CM_DistanceSquared(const vec3_t start, const vec3_t end)
{
    vec3_t delta;
#if EMULATE_X87
    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        delta[axis] = x87f_store_f32(x87f_sub(x87f_load_f32(end[axis]), x87f_load_f32(start[axis])));
    }
    /* Linux callers that keep this PC=64 result live require per-site x87
     * emulation; the public C return carrier narrows to binary64 on arm64. */
    return (long double)x87f_store_f64(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])), x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2]))));
#else
    delta[0] = end[0] - start[0];
    delta[1] = end[1] - start[1];
    delta[2] = end[2] - start[2];
    return ((long double)delta[0] * (long double)delta[0] + (long double)delta[1] * (long double)delta[1]) +
           (long double)delta[2] * (long double)delta[2];
#endif
}
#endif

/* Both binaries use the signed shift form of the 0x5f3759df seed.  Windows
 * keeps value*0.5 live, stores only the first Newton result as binary32, and
 * carries the second refinement directly into the final multiply.  Linux
 * spills the half-value and both Newton results as binary32. */
#if defined(WINDOWS_BEHAVIOR)
long double CM_FastSqrt(float value)
{
    uint32_t bits;
    float inverseRoot;
    memcpy(&bits, &value, sizeof(bits));
    bits = CM_FAST_INV_SQRT_MAGIC - ((bits >> 1) | (bits & CM_FLOAT_SIGN_BIT));
    memcpy(&inverseRoot, &bits, sizeof(inverseRoot));

#if EMULATE_X87
    const x87f half = x87f_mul(x87f_load_f32(value), x87f_load_f32(0.5f));
    x87f inverse = x87f_load_f32(inverseRoot);
    inverseRoot = x87f_store_f32(x87f_mul(inverse, x87f_sub(x87f_load_f32(1.5f), x87f_mul(x87f_mul(half, inverse), inverse))));
    inverse = x87f_load_f32(inverseRoot);
    const x87f refined = x87f_mul(inverse, x87f_sub(x87f_load_f32(1.5f), x87f_mul(x87f_mul(half, inverse), inverse)));
    return (long double)x87f_store_f64(x87f_mul(x87f_load_f32(value), refined));
#else
    const long double half = (long double)value * 0.5L;
    inverseRoot = (float)((long double)inverseRoot * (1.5L - half * (long double)inverseRoot * (long double)inverseRoot));
    const long double refined = (long double)inverseRoot * (1.5L - half * (long double)inverseRoot * (long double)inverseRoot);
    return (long double)value * refined;
#endif
}
#else
long double CM_FastSqrt(float value)
{
    uint32_t bits;
    float inverseRoot;
#if EMULATE_X87
    const float half = x87f_store_f32(x87f_mul(x87f_load_f32(value), x87f_load_f32(0.5f)));
#else
    const float half = value * 0.5f;
#endif

    memcpy(&bits, &value, sizeof(bits));
    bits = CM_FAST_INV_SQRT_MAGIC - ((bits >> 1) | (bits & CM_FLOAT_SIGN_BIT));
    memcpy(&inverseRoot, &bits, sizeof(inverseRoot));

#if EMULATE_X87
    for (int32_t iteration = 0; iteration < 2; ++iteration) {
        const x87f inverse = x87f_load_f32(inverseRoot);
        inverseRoot =
            x87f_store_f32(x87f_mul(inverse, x87f_sub(x87f_load_f32(1.5f), x87f_mul(x87f_mul(x87f_load_f32(half), inverse), inverse))));
    }
    return (long double)x87f_store_f64(x87f_mul(x87f_load_f32(value), x87f_load_f32(inverseRoot)));
#else
    inverseRoot =
        (float)((long double)inverseRoot * ((long double)1.5f - (long double)half * (long double)inverseRoot * (long double)inverseRoot));
    inverseRoot =
        (float)((long double)inverseRoot * ((long double)1.5f - (long double)half * (long double)inverseRoot * (long double)inverseRoot));
    return (long double)value * (long double)inverseRoot;
#endif
}
#endif

#undef CM_FLOAT_SIGN_BIT
#undef CM_FAST_INV_SQRT_MAGIC
