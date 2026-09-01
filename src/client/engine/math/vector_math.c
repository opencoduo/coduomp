#include "vector_math.h"
#include "compat/coduo_native_x87.h"

#include "../platform/crt_boundary.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* Source: CoDUOMP.exe 0x00431110..0x004311ae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00431110_004311af.mcode.
 * Name: same-module Mac symbol gunrandom. Both random integers are stored to
 * float before scaling; Windows evaluates sine and cosine together with one
 * FSINCOS instruction. */
void gunrandom(float *x, float *y)
{
    const float degrees =
        ((float)coduo_crt_rand() / 32768.0f) * 360.0f;
    const float radius =
        (float)coduo_crt_rand() / 32768.0f;
    const float radians =
        degrees * 3.1415927410125732f / 180.0f;
    float sine;
    float cosine;
    coduo_x87_sincosf(radians, &sine, &cosine);

    *x = (float)((long double)radius * sine);
    *y = (float)((long double)radius * cosine);
}

/* Source: CoDUOMP.exe 0x004c5560..0x004c5580.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5560_004c5581.mcode.
 * The exact double at 0x005b9bd0 is 0x3fdfffffff000000, or
 * 0.5 - 2^-30. The Windows x87 sequence subtracts it from the float input and
 * converts with FISTP under the active floating-point rounding mode. With the
 * engine's default round-to-nearest mode this is the source-level FastFloor
 * operation. The role-based name complements same-module Mac FastRound; that
 * platform emits no separate FastFloor symbol. */
int32_t FastFloor(float value)
{
    const double floorBias =
        0.499999999068677425384521484375; /* 0.5 - 2^-30 */
    const double rounded = rint((double)value - floorBias);

    /* Masked x87 FISTP produces its signed 32-bit indefinite result for NaN
     * or an out-of-range conversion. Preserve that result on hosts whose
     * native integer conversion would otherwise be undefined. */
    if (!(rounded >= (double)INT32_MIN && rounded <= (double)INT32_MAX))
        return INT32_MIN;

    return (int32_t)rounded;
}

/* Source: CoDUOMP.exe 0x004a6170..0x004a61c2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a6170_004a61c3.mcode.
 * Name and signature: exact same-module Mac symbol
 * AxisTransformVector(float const (*)[3], float, float, float, float *). The
 * Windows build carries axis in EAX and transformed in ECX. */
void AxisTransformVector(const axis_t axis, float x, float y, float z,
                         vec3_t transformed)
{
    transformed[0] =
        (y * axis[1][0] + z * axis[2][0]) + x * axis[0][0];
    transformed[1] =
        (x * axis[0][1] + y * axis[1][1]) + z * axis[2][1];
    transformed[2] =
        (x * axis[0][2] + y * axis[1][2]) + z * axis[2][2];
}
