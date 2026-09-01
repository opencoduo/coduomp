#include "client_math.h"

#include <stdint.h>

/*
 * These geometry leaves are instruction-identical between the original cgame
 * and UI DLLs (addresses are cgame/UI respectively):
 *
 *   NormalFromPoints        0x3004a470 / 0x40002440
 *   ProjectPointOnLine      0x3004a520 / 0x400024f0
 *   AnglesToAxisNegRight    0x3004c200 / 0x40004210
 *   PointBoxDistSqExceeds   0x3004ca90 / 0x40004aa0
 */

void NormalFromPoints(const vec3_t point0, const vec3_t point1,
                      vec3_t normal, const vec3_t point2)
{
    vec3_t edge1;
    vec3_t edge2;

    edge1[0] = point0[0] - point1[0];
    edge1[1] = point0[1] - point1[1];
    edge1[2] = point0[2] - point1[2];
    (void)VectorNormalize(edge1);
    edge2[0] = point0[0] - point2[0];
    edge2[1] = point0[1] - point2[1];
    edge2[2] = point0[2] - point2[2];
    (void)VectorNormalize(edge2);
    normal[0] = (float)((long double)edge2[2] * edge1[1] -
                        (long double)edge2[1] * edge1[2]);
    normal[1] = (float)((long double)edge2[0] * edge1[2] -
                        (long double)edge2[2] * edge1[0]);
    normal[2] = (float)((long double)edge2[1] * edge1[0] -
                        (long double)edge2[0] * edge1[1]);
    (void)VectorNormalize(normal);
}

void ProjectPointOnLine(vec3_t output, const vec3_t start,
                        const vec3_t point, const vec3_t end)
{
    vec3_t pointOffset;
    vec3_t direction;
    float distance;

    _VectorSubtract(point, start, pointOffset);
    _VectorSubtract(end, start, direction);
    (void)VectorNormalize(direction);
    distance = _DotProduct(pointOffset, direction);
    _VectorMA(start, distance, direction, output);
}

void AnglesToAxisNegRight(axis_t output, const vec3_t angles)
{
    vec3_t right;

    AngleVectors(angles, output[0], right, output[2]);
    output[1][0] = 0.0f - right[0];
    output[1][1] = 0.0f - right[1];
    output[1][2] = 0.0f - right[2];
}

qboolean PointBoxDistSqExceeds(const vec3_t point, const vec3_t corner0,
                               const vec3_t corner1,
                               float maximumDistanceSq)
{
    vec3_t delta0;
    vec3_t delta1;
    float distanceSq = 0.0f;

    delta0[0] = corner0[0] - point[0];
    delta0[1] = corner0[1] - point[1];
    delta0[2] = corner0[2] - point[2];
    delta1[0] = corner1[0] - point[0];
    delta1[1] = corner1[1] - point[1];
    delta1[2] = corner1[2] - point[2];

    for (int32_t axis = 0; axis < 3; ++axis) {
        const long double product =
            (long double)delta1[axis] * delta0[axis];

        /* The original x87 parity branch enters for positive or unordered. */
        if (!(product <= 0.0f)) {
            const float square0 = delta0[axis] * delta0[axis];
            const float square1 = delta1[axis] * delta1[axis];

            distanceSq += square1 < square0 ? square1 : square0;
        }
    }

    return distanceSq > maximumDistanceSq ? qtrue : qfalse;
}
