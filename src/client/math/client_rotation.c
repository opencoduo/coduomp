#include "client_math.h"

#include "compat/coduo_native_x87.h"

#include <stdint.h>

/*
 * These complete rotation helpers are instruction-identical between the two
 * original Windows client modules apart from rebased constant references:
 *
 *   uo_cgame_mp_x86.dll  RotatePoint2D           0x3004b6b0
 *   uo_ui_mp_x86.dll     RotatePoint2D           0x40003680
 *   uo_cgame_mp_x86.dll  RotatePointByAngles     0x3004cd90
 *   uo_ui_mp_x86.dll     RotatePointByAngles     0x40004da0
 *   uo_cgame_mp_x86.dll  RotatePointAroundOrigin 0x3004ceb0
 *   uo_ui_mp_x86.dll     RotatePointAroundOrigin 0x40004ec0
 *
 * The long-double expressions model the unspilled x87 operation chains; the
 * explicit float and double stores retain the original spill widths.
 */

void RotatePoint2D(vec2_t point, float degrees)
{
    const float radians = (float)((long double)degrees * 0.017453292f);
    float sine;
    float cosine;
    const float x = point[0];
    const float y = point[1];

    coduo_x87_sincosf(radians, &sine, &cosine);
    point[0] = (float)((long double)cosine * x - (long double)sine * y);
    point[1] = (float)((long double)sine * x + (long double)cosine * y);
}

void RotatePointByAngles(const vec3_t point, const vec3_t angles, vec3_t output)
{
    const int32_t componentA[3] = {1, 2, 0};
    const int32_t componentB[3] = {2, 0, 1};
    vec3_t rotated = {point[0], point[1], point[2]};

    for (int32_t axis = 0; axis < 3; ++axis) {
        if (angles[axis] != 0.0f) {
            const double radians = (double)((long double)angles[axis] * 3.14159274f / 180.0);
            double sine;
            double cosine;
            const int32_t a = componentA[axis];
            const int32_t b = componentB[axis];
            const float oldA = rotated[a];
            const float oldB = rotated[b];

            coduo_x87_sincos(radians, &sine, &cosine);
            rotated[a] = (float)((long double)oldA * cosine - (long double)oldB * sine);
            rotated[b] = (float)((long double)oldB * cosine + (long double)oldA * sine);
        }
    }

    output[0] = rotated[0];
    output[1] = rotated[1];
    output[2] = rotated[2];
}

void RotatePointAroundOrigin(const vec3_t point, const vec3_t origin, const vec3_t angles, vec3_t output)
{
    vec3_t difference;
    vec3_t rotated;

    difference[0] = point[0] - origin[0];
    difference[1] = point[1] - origin[1];
    difference[2] = point[2] - origin[2];
    RotatePointByAngles(difference, angles, rotated);
    output[0] = rotated[0] + origin[0];
    output[1] = rotated[1] + origin[1];
    output[2] = rotated[2] + origin[2];
}
