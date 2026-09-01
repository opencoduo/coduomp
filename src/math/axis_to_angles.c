#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

/*
 * Complete axis-to-Euler conversion bank.  The three Windows instruction
 * streams are identical across all four authoritative images after accounting
 * only for relocated calls and constants:
 *
 *                         AxisToAngles  Axis4ToAngles AxisToSignedAngles
 *   CoDUOMP.exe           0x00434140    0x00434290    0x004343e0
 *   uo_cgame_mp_x86.dll  0x3004c2a0    0x3004c3f0    0x3004c540
 *   uo_ui_mp_x86.dll     0x400042b0    0x40004400    0x40004550
 *   uo_game_mp_x86.dll   0x200192f0    0x20019440    0x20019590
 *
 * The former cgame/UI CG_ProjectTagToScreen2D and
 * CG_ProjectTagToScreen2D_Raw reconstructions were naming artifacts: their
 * complete bodies and adjoining bank positions prove that they are
 * Axis4ToAngles and AxisToSignedAngles respectively.
 *
 * Linux game exports the same bank at RVAs 0x0003e350, 0x0003e4c8, and
 * 0x0003e640.  The engine retains the same data dependencies at 0x0806a544,
 * 0x0806a6a4, and 0x0806a804.  Windows loads binary32 pi and 180; Linux loads
 * binary64 pi and 180.  That constant-width difference is the sole
 * source-level platform variant here.  Reordered commutative add operands,
 * FCHS versus a sign-bit XOR, and memory-zero versus FLDZ comparisons are
 * compiler realizations of the same source operations and do not require
 * separate function bodies.
 */

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#if defined(WINDOWS_BEHAVIOR)
#define Q_AXIS_ANGLE_PI 3.1415927410125732f
#define Q_AXIS_ANGLE_HALF_CIRCLE 180.0f
#else
#define Q_AXIS_ANGLE_PI 3.141592653589793
#define Q_AXIS_ANGLE_HALF_CIRCLE 180.0
#endif

#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
#define Q_AXIS_ANGLE_RADIANS(value)                                         \
    x87f_store_f32(x87f_div(                                               \
        x87f_mul(x87f_neg(x87f_load_f32(value)),                           \
                 x87f_load_f32(Q_AXIS_ANGLE_PI)),                          \
        x87f_load_f32(Q_AXIS_ANGLE_HALF_CIRCLE)))
#else
#define Q_AXIS_ANGLE_RADIANS(value)                                         \
    x87f_store_f32(x87f_div(                                               \
        x87f_mul(x87f_neg(x87f_load_f32(value)),                           \
                 x87f_load_f64(Q_AXIS_ANGLE_PI)),                          \
        x87f_load_f64(Q_AXIS_ANGLE_HALF_CIRCLE)))
#endif
#define Q_AXIS_ANGLE_ADD_PRODUCTS(a, b, c, d)                              \
    x87f_store_f32(x87f_add(                                               \
        x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),                     \
        x87f_mul(x87f_load_f32(c), x87f_load_f32(d))))
#define Q_AXIS_ANGLE_SUB_PRODUCTS(a, b, c, d)                              \
    x87f_store_f32(x87f_sub(                                               \
        x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),                     \
        x87f_mul(x87f_load_f32(c), x87f_load_f32(d))))
#define Q_AXIS_ANGLE_ADD(a, b)                                             \
    x87f_store_f32(x87f_add(x87f_load_f32(a), x87f_load_f32(b)))
#else
#define Q_AXIS_ANGLE_RADIANS(value)                                        \
    ((float)((-(long double)(value) *                                     \
              (long double)Q_AXIS_ANGLE_PI) /                             \
             (long double)Q_AXIS_ANGLE_HALF_CIRCLE))
#define Q_AXIS_ANGLE_ADD_PRODUCTS(a, b, c, d)                              \
    ((float)((long double)(a) * (long double)(b) +                        \
             (long double)(c) * (long double)(d)))
#define Q_AXIS_ANGLE_SUB_PRODUCTS(a, b, c, d)                              \
    ((float)((long double)(a) * (long double)(b) -                        \
             (long double)(c) * (long double)(d)))
#define Q_AXIS_ANGLE_ADD(a, b)                                             \
    ((float)((long double)(a) + (long double)(b)))
#endif

void AxisToAngles(const axis_t axis, vec3_t angles)
{
    float rightX;
    float rightY;
    float rightZ;
    float sine;
    float cosine;
    float planar;
    float rotatedY;
    vec3_t rotated;
    float roll;

    vectoangles(axis[0], angles);
    rightX = axis[1][0];
    rightY = axis[1][1];
    rightZ = axis[1][2];
    coduo_x87_sincosf(Q_AXIS_ANGLE_RADIANS(angles[1]), &sine, &cosine);
    planar = Q_AXIS_ANGLE_SUB_PRODUCTS(cosine, rightX, sine, rightY);
    rotatedY = Q_AXIS_ANGLE_ADD_PRODUCTS(sine, rightX, cosine, rightY);
    coduo_x87_sincosf(Q_AXIS_ANGLE_RADIANS(angles[0]), &sine, &cosine);
    rotated[0] = Q_AXIS_ANGLE_ADD_PRODUCTS(sine, rightZ, cosine, planar);
    rotated[1] = rotatedY;
    rotated[2] = Q_AXIS_ANGLE_SUB_PRODUCTS(cosine, rightZ, sine, planar);
    roll = vectosignedpitch(rotated);

    if (rotatedY < 0.0f) {
        const float adjustment = roll < 0.0f ? 180.0f : -180.0f;
        angles[2] = Q_AXIS_ANGLE_ADD(roll, adjustment);
    } else {
        angles[2] = -roll;
    }
}

void Axis4ToAngles(const DObjSkelMat *matrix, vec3_t angles)
{
    float rightX;
    float rightY;
    float rightZ;
    float sine;
    float cosine;
    float planar;
    float rotatedY;
    vec3_t rotated;
    float roll;

    vectoangles(matrix->axis[0], angles);
    rightX = matrix->axis[1][0];
    rightY = matrix->axis[1][1];
    rightZ = matrix->axis[1][2];
    coduo_x87_sincosf(Q_AXIS_ANGLE_RADIANS(angles[1]), &sine, &cosine);
    planar = Q_AXIS_ANGLE_SUB_PRODUCTS(cosine, rightX, sine, rightY);
    rotatedY = Q_AXIS_ANGLE_ADD_PRODUCTS(sine, rightX, cosine, rightY);
    coduo_x87_sincosf(Q_AXIS_ANGLE_RADIANS(angles[0]), &sine, &cosine);
    rotated[0] = Q_AXIS_ANGLE_ADD_PRODUCTS(sine, rightZ, cosine, planar);
    rotated[1] = rotatedY;
    rotated[2] = Q_AXIS_ANGLE_SUB_PRODUCTS(cosine, rightZ, sine, planar);
    roll = vectosignedpitch(rotated);

    if (rotatedY < 0.0f) {
        const float adjustment = roll < 0.0f ? 180.0f : -180.0f;
        angles[2] = Q_AXIS_ANGLE_ADD(roll, adjustment);
    } else {
        angles[2] = -roll;
    }
}

void AxisToSignedAngles(const axis_t axis, vec3_t angles)
{
    float rightX;
    float rightY;
    float rightZ;
    float sine;
    float cosine;
    float planar;
    float rotatedY;
    vec3_t rotated;
    float roll;

    vectosignedangles(axis[0], angles);
    rightX = axis[1][0];
    rightY = axis[1][1];
    rightZ = axis[1][2];
    coduo_x87_sincosf(Q_AXIS_ANGLE_RADIANS(angles[1]), &sine, &cosine);
    planar = Q_AXIS_ANGLE_SUB_PRODUCTS(cosine, rightX, sine, rightY);
    rotatedY = Q_AXIS_ANGLE_ADD_PRODUCTS(sine, rightX, cosine, rightY);
    coduo_x87_sincosf(Q_AXIS_ANGLE_RADIANS(angles[0]), &sine, &cosine);
    rotated[0] = Q_AXIS_ANGLE_ADD_PRODUCTS(sine, rightZ, cosine, planar);
    rotated[1] = rotatedY;
    rotated[2] = Q_AXIS_ANGLE_SUB_PRODUCTS(cosine, rightZ, sine, planar);
    roll = vectosignedpitch(rotated);

    if (rotatedY < 0.0f) {
        const float adjustment = roll < 0.0f ? 180.0f : -180.0f;
        angles[2] = Q_AXIS_ANGLE_ADD(roll, adjustment);
    } else {
        angles[2] = -roll;
    }
}

#undef Q_AXIS_ANGLE_ADD
#undef Q_AXIS_ANGLE_SUB_PRODUCTS
#undef Q_AXIS_ANGLE_ADD_PRODUCTS
#undef Q_AXIS_ANGLE_RADIANS
#undef Q_AXIS_ANGLE_HALF_CIRCLE
#undef Q_AXIS_ANGLE_PI
