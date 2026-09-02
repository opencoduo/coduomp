#include "collision_geometry.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_transforms.c requires a platform behavior mode"
#endif

/*
 * Complete collision-local rotation helper cluster:
 *
 *   RotatePoint          CoDUOMP.exe 0x00425d70, coduo_lnxded 0x08057b18
 *   TransposeMatrix      CoDUOMP.exe 0x00425dc0, coduo_lnxded 0x08057bbf
 *   CreateRotationMatrix CoDUOMP.exe 0x00425df0, coduo_lnxded 0x08057c2a
 *
 * CreateRotationMatrix and TransposeMatrix have common source behavior.
 * RotatePoint retains the genuine per-platform x87 graph below.
 */

void CreateRotationMatrix(const vec3_t angles, axis_t matrix)
{
    AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
    VectorInverse(matrix[1]);
}

#if defined(WINDOWS_BEHAVIOR)
void RotatePoint(vec3_t point, const axis_t matrix)
{
#if EMULATE_X87
    const x87f x = x87f_load_f32(point[0]);
    const x87f y = x87f_load_f32(point[1]);
    const x87f z = x87f_load_f32(point[2]);

    for (int32_t row = 0; row < 3; ++row) {
        point[row] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(z, x87f_load_f32(matrix[row][2])), x87f_mul(y, x87f_load_f32(matrix[row][1]))),
                                    x87f_mul(x, x87f_load_f32(matrix[row][0]))));
    }
#else
    const long double x = (long double)point[0];
    const long double y = (long double)point[1];
    const long double z = (long double)point[2];

    for (int32_t row = 0; row < 3; ++row) {
        point[row] = (float)((z * (long double)matrix[row][2] + y * (long double)matrix[row][1]) + x * (long double)matrix[row][0]);
    }
#endif
}
#else
void RotatePoint(vec3_t point, const axis_t matrix)
{
    const float x = point[0];
    const float y = point[1];
    const float z = point[2];

#if EMULATE_X87
    for (int32_t row = 0; row < 3; ++row) {
        point[row] = x87f_store_f32(x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(matrix[row][0]), x87f_load_f32(x)), x87f_mul(x87f_load_f32(matrix[row][1]), x87f_load_f32(y))),
            x87f_mul(x87f_load_f32(matrix[row][2]), x87f_load_f32(z))));
    }
#else
    for (int32_t row = 0; row < 3; ++row) {
        point[row] = (float)(((long double)matrix[row][0] * (long double)x + (long double)matrix[row][1] * (long double)y) +
                             (long double)matrix[row][2] * (long double)z);
    }
#endif
}
#endif

void TransposeMatrix(const axis_t input, axis_t output)
{
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            output[row][column] = input[column][row];
        }
    }
}
