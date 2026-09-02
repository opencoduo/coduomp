#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * VectorRotate is byte-identical within the four Windows targets at
 * CoDUOMP.exe 0x004318e0, cgame 0x30049a40, UI 0x40001a10, and game
 * 0x20016a90.  Its first row is accumulated Y,Z,X while the other rows use
 * X,Y,Z.  The Linux engine body at 0x08066b4e and game body at RVA 0x0003a62a
 * use X,Y,Z for every row.  Each dot remains in x87 precision until its final
 * binary32 store.
 */
#if defined(WINDOWS_BEHAVIOR)
void VectorRotate(const vec3_t input, const axis_t matrix, vec3_t output)
{
#if EMULATE_X87
    output[0] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(input[1]),
                          x87f_load_f32(matrix[0][1])),
                 x87f_mul(x87f_load_f32(input[2]),
                          x87f_load_f32(matrix[0][2]))),
        x87f_mul(x87f_load_f32(input[0]),
                 x87f_load_f32(matrix[0][0]))));
    output[1] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(input[0]),
                          x87f_load_f32(matrix[1][0])),
                 x87f_mul(x87f_load_f32(input[1]),
                          x87f_load_f32(matrix[1][1]))),
        x87f_mul(x87f_load_f32(input[2]),
                 x87f_load_f32(matrix[1][2]))));
    output[2] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(input[0]),
                          x87f_load_f32(matrix[2][0])),
                 x87f_mul(x87f_load_f32(input[1]),
                          x87f_load_f32(matrix[2][1]))),
        x87f_mul(x87f_load_f32(input[2]),
                 x87f_load_f32(matrix[2][2]))));
#else
    output[0] = (float)(
        ((long double)input[1] * matrix[0][1] +
         (long double)input[2] * matrix[0][2]) +
        (long double)input[0] * matrix[0][0]);
    output[1] = (float)(
        ((long double)input[0] * matrix[1][0] +
         (long double)input[1] * matrix[1][1]) +
        (long double)input[2] * matrix[1][2]);
    output[2] = (float)(
        ((long double)input[0] * matrix[2][0] +
         (long double)input[1] * matrix[2][1]) +
        (long double)input[2] * matrix[2][2]);
#endif
}
#else
void VectorRotate(const vec3_t input, const axis_t matrix, vec3_t output)
{
    for (int32_t row = 0; row < 3; ++row) {
#if EMULATE_X87
        output[row] = x87f_store_f32(x87f_add(
            x87f_add(x87f_mul(x87f_load_f32(input[0]),
                              x87f_load_f32(matrix[row][0])),
                     x87f_mul(x87f_load_f32(input[1]),
                              x87f_load_f32(matrix[row][1]))),
            x87f_mul(x87f_load_f32(input[2]),
                     x87f_load_f32(matrix[row][2]))));
#else
        output[row] = (float)(
            ((long double)input[0] * matrix[row][0] +
             (long double)input[1] * matrix[row][1]) +
            (long double)input[2] * matrix[row][2]);
#endif
    }
}
#endif

/*
 * RotatePointAroundVector is shared by every image.  The four Windows bodies
 * are instruction-identical apart from relocated calls and constants at
 * 0x00431930/0x30049a90/0x40001a60/0x20016ae0.  Linux coduo_lnxded
 * 0x08066bf8 and game RVA 0x0003a6d4 retain the same basis, transpose, two
 * matrix products, and final row transform.
 *
 * Windows forms radians from binary32 pi and 180 constants and uses its
 * characteristic final-row product orders.  Linux uses binary64 pi and 180;
 * the game calls VectorRotate while the engine emits the same X,Y,Z row loop
 * inline.  Complete platform bodies preserve those arithmetic differences.
 * EMULATE_X87 remains independent in both bodies.
 */
#if defined(WINDOWS_BEHAVIOR)
void RotatePointAroundVector(vec3_t output, const vec3_t direction,
                             const vec3_t point, float degrees)
{
    vec3_t right;
    vec3_t up;
    vec3_t forward;
    axis_t basis;
    axis_t inverse;
    axis_t zRotation = {{0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f}};
    axis_t temporary;
    axis_t rotation;
    float radians;
    float sine;
    float cosine;

    memcpy(forward, direction, sizeof(forward));
    PerpendicularVector(right, direction);
    CrossProduct(right, forward, up);

    basis[0][0] = right[0];
    basis[0][1] = up[0];
    basis[0][2] = forward[0];
    basis[1][0] = right[1];
    basis[1][1] = up[1];
    basis[1][2] = forward[1];
    basis[2][0] = right[2];
    basis[2][1] = up[2];
    basis[2][2] = forward[2];

    inverse[0][0] = basis[0][0];
    inverse[0][1] = basis[1][0];
    inverse[0][2] = basis[2][0];
    inverse[1][0] = basis[0][1];
    inverse[1][1] = basis[1][1];
    inverse[1][2] = basis[2][1];
    inverse[2][0] = basis[0][2];
    inverse[2][1] = basis[1][2];
    inverse[2][2] = basis[2][2];

#if EMULATE_X87
    radians = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(degrees),
                 x87f_load_f32(3.1415927410125732f)),
        x87f_load_f32(180.0f)));
#else
    radians = (float)(((long double)degrees * 3.1415927410125732f) /
                      180.0f);
#endif
    coduo_x87_sincosf(radians, &sine, &cosine);
    zRotation[0][0] = cosine;
    zRotation[0][1] = sine;
#if EMULATE_X87
    zRotation[1][0] = x87f_store_f32(x87f_neg(x87f_load_f32(sine)));
#else
    zRotation[1][0] = -sine;
#endif
    zRotation[1][1] = cosine;
    zRotation[2][2] = 1.0f;

    MatrixMultiply(basis, zRotation, temporary);
    MatrixMultiply(temporary, inverse, rotation);

#if EMULATE_X87
    output[0] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(rotation[0][2]),
                          x87f_load_f32(point[2])),
                 x87f_mul(x87f_load_f32(rotation[0][1]),
                          x87f_load_f32(point[1]))),
        x87f_mul(x87f_load_f32(rotation[0][0]),
                 x87f_load_f32(point[0]))));
    output[1] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(rotation[1][1]),
                          x87f_load_f32(point[1])),
                 x87f_mul(x87f_load_f32(rotation[1][0]),
                          x87f_load_f32(point[0]))),
        x87f_mul(x87f_load_f32(rotation[1][2]),
                 x87f_load_f32(point[2]))));
    output[2] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(rotation[2][1]),
                          x87f_load_f32(point[1])),
                 x87f_mul(x87f_load_f32(rotation[2][0]),
                          x87f_load_f32(point[0]))),
        x87f_mul(x87f_load_f32(rotation[2][2]),
                 x87f_load_f32(point[2]))));
#else
    output[0] = (float)(
        ((long double)rotation[0][2] * point[2] +
         (long double)rotation[0][1] * point[1]) +
        (long double)rotation[0][0] * point[0]);
    output[1] = (float)(
        ((long double)rotation[1][1] * point[1] +
         (long double)rotation[1][0] * point[0]) +
        (long double)rotation[1][2] * point[2]);
    output[2] = (float)(
        ((long double)rotation[2][1] * point[1] +
         (long double)rotation[2][0] * point[0]) +
        (long double)rotation[2][2] * point[2]);
#endif
}
#else
void RotatePointAroundVector(vec3_t output, const vec3_t direction,
                             const vec3_t point, float degrees)
{
    vec3_t right;
    vec3_t up;
    vec3_t forward;
    axis_t basis;
    axis_t inverse;
    axis_t zRotation = {{0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f}};
    axis_t temporary;
    axis_t rotation;
    float radians;
    float sine;
    float cosine;
    uint32_t sineBits;

    memcpy(forward, direction, sizeof(forward));
    PerpendicularVector(right, direction);
    CrossProduct(right, forward, up);

    basis[0][0] = right[0];
    basis[0][1] = up[0];
    basis[0][2] = forward[0];
    basis[1][0] = right[1];
    basis[1][1] = up[1];
    basis[1][2] = forward[1];
    basis[2][0] = right[2];
    basis[2][1] = up[2];
    basis[2][2] = forward[2];

    inverse[0][0] = basis[0][0];
    inverse[0][1] = basis[1][0];
    inverse[0][2] = basis[2][0];
    inverse[1][0] = basis[0][1];
    inverse[1][1] = basis[1][1];
    inverse[1][2] = basis[2][1];
    inverse[2][0] = basis[0][2];
    inverse[2][1] = basis[1][2];
    inverse[2][2] = basis[2][2];

#if EMULATE_X87
    radians = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(degrees),
                 x87f_load_f64(3.141592653589793)),
        x87f_load_f64(180.0)));
#else
    radians = (float)(((long double)degrees * 3.141592653589793) /
                      180.0);
#endif
    coduo_x87_sincosf(radians, &sine, &cosine);
    zRotation[0][0] = cosine;
    zRotation[0][1] = sine;
    memcpy(&sineBits, &sine, sizeof(sineBits));
    sineBits ^= UINT32_C(0x80000000);
    memcpy(&zRotation[1][0], &sineBits, sizeof(zRotation[1][0]));
    zRotation[1][1] = cosine;
    zRotation[2][2] = 1.0f;

    MatrixMultiply(basis, zRotation, temporary);
    MatrixMultiply(temporary, inverse, rotation);
    VectorRotate(point, (const float (*)[3])rotation, output);
}
#endif

/*
 * RotateAroundDirection has the same data dependencies in all six images.
 * Windows is byte-identical at 0x00431b50/0x30049cb0/0x40001c80/0x20016d00;
 * Linux agrees at coduo_lnxded 0x08066e65 and game RVA 0x0003a94f.  An
 * unordered yaw follows the rotation path in every body.
 */
void RotateAroundDirection(axis_t axis, float yaw)
{
    PerpendicularVector(axis[1], axis[0]);
    if (yaw != 0.0f) {
        vec3_t right;

        memcpy(right, axis[1], sizeof(right));
        RotatePointAroundVector(axis[1], axis[0], right, yaw);
    }
    CrossProduct(axis[0], axis[1], axis[2]);
}
