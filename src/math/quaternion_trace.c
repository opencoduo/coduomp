#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#include <math.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The complete quaternion trace/yaw cluster is shared by the Windows engine,
 * cgame, UI, and game module.  The four Windows copies are instruction-
 * identical apart from relocated constants and calls:
 *
 *                         engine      cgame       UI          game
 * QuatEigenTrace          0x4337f0    0x3004b950  0x40003920  0x200189a0
 * AngleEigenTrace         0x4338a0    0x3004ba00  0x400039d0  0x20018a50
 * QuatRatioEigenTrace     0x4338d0    0x3004ba30  0x40003a00  0x20018a80
 * RotationToYaw           0x433910    0x3004ba70  0x40003a40  0x20018ac0
 *
 * Linux retains the same public cluster in the dedicated engine at
 * 0x0806998f..0x08069b14 and in game.mp.uo.i386.so at RVAs
 * 0x0003d602..0x0003d7d6.  The Linux game symbols establish the canonical
 * names, including AngleEigenTrace; SinSquaredDegrees was only a descriptive
 * reconstruction name.
 */

/*
 * Windows folds the denominator w,z,y,x and returns the z,y,x numerator sum
 * live under PC=53.  Linux folds x,y,z,w, then explicitly stores the final
 * x,y,z sum to binary32 before reloading it.  long double is the project-wide
 * source carrier for this cross-platform ST0 contract; the emulated Windows
 * body transfers its PC=53 result through exact binary64 storage.
 */
#if defined(WINDOWS_BEHAVIOR)
long double QuatEigenTrace(const vec4_t quaternion)
{
    float xSquared;
    float ySquared;
    float zSquared;
    float lengthSquared;

#if EMULATE_X87
    xSquared = x87f_store_f32(x87f_mul(x87f_load_f32(quaternion[0]), x87f_load_f32(quaternion[0])));
    ySquared = x87f_store_f32(x87f_mul(x87f_load_f32(quaternion[1]), x87f_load_f32(quaternion[1])));
    zSquared = x87f_store_f32(x87f_mul(x87f_load_f32(quaternion[2]), x87f_load_f32(quaternion[2])));
    lengthSquared = x87f_store_f32(
        x87f_add(x87f_add(x87f_add(x87f_mul(x87f_load_f32(quaternion[3]), x87f_load_f32(quaternion[3])), x87f_load_f32(zSquared)),
                          x87f_load_f32(ySquared)),
                 x87f_load_f32(xSquared)));

    if (lengthSquared != 0.0f) {
        const float reciprocal = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(lengthSquared)));
        const float scaledX = x87f_store_f32(x87f_mul(x87f_load_f32(reciprocal), x87f_load_f32(xSquared)));
        const float scaledY = x87f_store_f32(x87f_mul(x87f_load_f32(reciprocal), x87f_load_f32(ySquared)));
        const float scaledZ = x87f_store_f32(x87f_mul(x87f_load_f32(reciprocal), x87f_load_f32(zSquared)));
        const x87f result = x87f_add(x87f_add(x87f_load_f32(scaledZ), x87f_load_f32(scaledY)), x87f_load_f32(scaledX));

        return (long double)x87f_store_f64(result);
    }
#else
    xSquared = (float)((long double)quaternion[0] * quaternion[0]);
    ySquared = (float)((long double)quaternion[1] * quaternion[1]);
    zSquared = (float)((long double)quaternion[2] * quaternion[2]);
    lengthSquared = (float)((((long double)quaternion[3] * quaternion[3] + zSquared) + ySquared) + xSquared);

    if (lengthSquared != 0.0f) {
        const float reciprocal = (float)((long double)1.0f / lengthSquared);
        const float scaledX = (float)((long double)reciprocal * xSquared);
        const float scaledY = (float)((long double)reciprocal * ySquared);
        const float scaledZ = (float)((long double)reciprocal * zSquared);

        return ((long double)scaledZ + scaledY) + scaledX;
    }
#endif

    return 0.0f;
}
#else
long double QuatEigenTrace(const vec4_t quaternion)
{
    float xSquared;
    float ySquared;
    float zSquared;
    float lengthSquared;

#if EMULATE_X87
    xSquared = x87f_store_f32(x87f_mul(x87f_load_f32(quaternion[0]), x87f_load_f32(quaternion[0])));
    ySquared = x87f_store_f32(x87f_mul(x87f_load_f32(quaternion[1]), x87f_load_f32(quaternion[1])));
    zSquared = x87f_store_f32(x87f_mul(x87f_load_f32(quaternion[2]), x87f_load_f32(quaternion[2])));
    lengthSquared = x87f_store_f32(x87f_add(x87f_add(x87f_add(x87f_load_f32(xSquared), x87f_load_f32(ySquared)), x87f_load_f32(zSquared)),
                                            x87f_mul(x87f_load_f32(quaternion[3]), x87f_load_f32(quaternion[3]))));

    if (lengthSquared != 0.0f) {
        const float reciprocal = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(lengthSquared)));
        const float scaledX = x87f_store_f32(x87f_mul(x87f_load_f32(xSquared), x87f_load_f32(reciprocal)));
        const float scaledY = x87f_store_f32(x87f_mul(x87f_load_f32(ySquared), x87f_load_f32(reciprocal)));
        const float scaledZ = x87f_store_f32(x87f_mul(x87f_load_f32(zSquared), x87f_load_f32(reciprocal)));

        return x87f_store_f32(x87f_add(x87f_add(x87f_load_f32(scaledX), x87f_load_f32(scaledY)), x87f_load_f32(scaledZ)));
    }
#else
    xSquared = quaternion[0] * quaternion[0];
    ySquared = quaternion[1] * quaternion[1];
    zSquared = quaternion[2] * quaternion[2];
    lengthSquared = ((xSquared + ySquared) + zSquared) + quaternion[3] * quaternion[3];

    if (lengthSquared != 0.0f) {
        const float reciprocal = 1.0f / lengthSquared;
        const float scaledX = xSquared * reciprocal;
        const float scaledY = ySquared * reciprocal;
        const float scaledZ = zSquared * reciprocal;
        const float result = (scaledX + scaledY) + scaledZ;

        return result;
    }
#endif

    return 0.0f;
}
#endif

/*
 * Windows supplies a binary32 pi/180 operand directly to the x87 sine
 * intrinsic.  Linux multiplies by the exact binary64 pi/180 constant and
 * stores the product to binary64 for its system sin call.  Both square a
 * binary64 sine result and narrow the public result to binary32.
 */
#if defined(WINDOWS_BEHAVIOR)
float AngleEigenTrace(float degrees)
{
    const double sine = (double)coduo_x87_sinl((long double)degrees * 0.01745329238474369f);

    return (float)(sine * sine);
}
#else
float AngleEigenTrace(float degrees)
{
    double radians;
    double sine;

#if EMULATE_X87
    radians = x87f_store_f64(x87f_mul(x87f_load_f32(degrees), x87f_load_f64(0.017453292519943295)));
    sine = sin(radians);
    return x87f_store_f32(x87f_mul(x87f_load_f64(sine), x87f_load_f64(sine)));
#else
    radians = (double)((long double)degrees * (long double)0.017453292519943295);
    sine = sin(radians);
    return (float)((long double)sine * sine);
#endif
}
#endif

/*
 * All six authoritative bodies conjugate the right operand, then evaluate
 * inverse(right) * left through the original second-times-first QuatMultiply
 * convention.  The Windows compiler inlines QuatInverse; Linux calls it.
 */
long double QuatRatioEigenTrace(const vec4_t left, const vec4_t right)
{
    vec4_t inverse;
    vec4_t product;

    QuatInverse(right, inverse);
    QuatMultiply(left, inverse, product);
    return QuatEigenTrace(product);
}

/*
 * Both platforms explicitly store x*x, x*x+y*y, and 2/length to binary32,
 * then store each atan2 argument to binary64.  Windows uses a float-derived
 * 180/pi value (57.2957763671875); Linux uses the exact binary64 value
 * 57.29577951308232.  The final result is binary32 on both platforms.
 */
#if defined(WINDOWS_BEHAVIOR)
float RotationToYaw(const vec2_t rotation)
{
    float xSquared;
    float lengthSquared;
    float scale;
    double numerator;
    double denominator;

#if EMULATE_X87
    xSquared = x87f_store_f32(x87f_mul(x87f_load_f32(rotation[0]), x87f_load_f32(rotation[0])));
    lengthSquared = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(rotation[1]), x87f_load_f32(rotation[1])), x87f_load_f32(xSquared)));
    scale = x87f_store_f32(x87f_div(x87f_load_f32(2.0f), x87f_load_f32(lengthSquared)));
    numerator = x87f_store_f64(x87f_mul(x87f_mul(x87f_load_f32(rotation[1]), x87f_load_f32(rotation[0])), x87f_load_f32(scale)));
    denominator = x87f_store_f64(x87f_sub(x87f_load_f32(1.0f), x87f_mul(x87f_load_f32(scale), x87f_load_f32(xSquared))));
    return x87f_store_f32(x87f_mul(x87f_load_f64(atan2(numerator, denominator)), x87f_load_f64(57.2957763671875)));
#else
    xSquared = (float)((long double)rotation[0] * rotation[0]);
    lengthSquared = (float)((long double)rotation[1] * rotation[1] + xSquared);
    scale = (float)((long double)2.0f / lengthSquared);
    numerator = (double)((long double)rotation[1] * rotation[0] * scale);
    denominator = (double)((long double)1.0f - (long double)scale * xSquared);
    return (float)((long double)atan2(numerator, denominator) * (long double)57.2957763671875);
#endif
}
#else
float RotationToYaw(const vec2_t rotation)
{
    float xSquared;
    float lengthSquared;
    float scale;
    double numerator;
    double denominator;

#if EMULATE_X87
    xSquared = x87f_store_f32(x87f_mul(x87f_load_f32(rotation[0]), x87f_load_f32(rotation[0])));
    lengthSquared = x87f_store_f32(x87f_add(x87f_load_f32(xSquared), x87f_mul(x87f_load_f32(rotation[1]), x87f_load_f32(rotation[1]))));
    scale = x87f_store_f32(x87f_div(x87f_load_f32(2.0f), x87f_load_f32(lengthSquared)));
    denominator = x87f_store_f64(x87f_sub(x87f_load_f64(1.0), x87f_mul(x87f_load_f32(xSquared), x87f_load_f32(scale))));
    numerator = x87f_store_f64(x87f_mul(x87f_mul(x87f_load_f32(rotation[0]), x87f_load_f32(rotation[1])), x87f_load_f32(scale)));
    return x87f_store_f32(x87f_mul(x87f_load_f64(atan2(numerator, denominator)), x87f_load_f64(57.29577951308232)));
#else
    xSquared = rotation[0] * rotation[0];
    lengthSquared = xSquared + rotation[1] * rotation[1];
    scale = 2.0f / lengthSquared;
    denominator = (double)((long double)1.0 - (long double)xSquared * scale);
    numerator = (double)((long double)rotation[0] * rotation[1] * scale);
    return (float)((long double)atan2(numerator, denominator) * (long double)57.29577951308232);
#endif
}
#endif
