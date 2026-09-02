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
 * Complete direction-to-angle conversion bank.  Each Windows instruction
 * stream is identical apart from relocations in all four authoritative images:
 *
 *                         yaw         signed yaw  pitch       signed pitch
 *   CoDUOMP.exe           0x00431c80  0x00431cf0  0x00431d40  0x00431e00
 *   uo_cgame_mp_x86.dll  0x30049de0  0x30049e50  0x30049ea0  0x30049f60
 *   uo_ui_mp_x86.dll     0x40001db0  0x40001e20  0x40001e70  0x40001f30
 *   uo_game_mp_x86.dll   0x20016e30  0x20016ea0  0x20016ef0  0x20016fb0
 *
 *                         angles      signed angles
 *   CoDUOMP.exe           0x00431ea0  0x00431fc0
 *   uo_cgame_mp_x86.dll  0x3004a000  0x3004a120
 *   uo_ui_mp_x86.dll     0x40001fd0  0x400020f0
 *   uo_game_mp_x86.dll   0x20017050  0x20017170
 *
 * The Linux engine bodies at 0x08066ff0..0x080674d7 and game-module bodies at
 * RVAs 0x0003aafb..0x0003b0bc retain the same branches, formulae, binary32
 * stores, and public results.  The sole source-visible platform discrepancy is
 * the pi constant: Windows promotes binary32 pi to binary64, while Linux uses
 * binary64 pi.  The horizontal-length realization also differs at the ABI
 * boundary: Windows _CIsqrt consumes the live x87 sum, while Linux passes a
 * binary64 argument to sqrt.  Those narrow differences are retained below.
 */

#if defined(WINDOWS_BEHAVIOR)
#define Q_DIRECTION_PI 3.1415927410125732
#else
#define Q_DIRECTION_PI 3.141592653589793
#endif

#if EMULATE_X87
#define Q_DIRECTION_DEGREES(y, x, scale) \
    x87f_store_f32( \
        x87f_div(x87f_mul(x87f_load_f64(atan2((double)(y), (double)(x))), x87f_load_f64((double)(scale))), x87f_load_f64(Q_DIRECTION_PI)))
#if defined(WINDOWS_BEHAVIOR)
#define Q_DIRECTION_HORIZONTAL(x, y) \
    x87f_store_f32(x87f_sqrt(x87f_add(x87f_mul(x87f_load_f32(x), x87f_load_f32(x)), x87f_mul(x87f_load_f32(y), x87f_load_f32(y)))))
#else
#define Q_DIRECTION_HORIZONTAL(x, y) \
    ((float)sqrt(x87f_store_f64(x87f_add(x87f_mul(x87f_load_f32(x), x87f_load_f32(x)), x87f_mul(x87f_load_f32(y), x87f_load_f32(y))))))
#endif
#define Q_DIRECTION_WRAP(value) x87f_store_f32(x87f_add(x87f_load_f32(value), x87f_load_f32(360.0f)))
#else
#define Q_DIRECTION_DEGREES(y, x, scale) \
    ((float)(((long double)atan2((double)(y), (double)(x)) * (long double)(scale)) / (long double)Q_DIRECTION_PI))
#if defined(WINDOWS_BEHAVIOR)
#define Q_DIRECTION_HORIZONTAL(x, y) ((float)coduo_x87_sqrtl((long double)(x) * (long double)(x) + (long double)(y) * (long double)(y)))
#else
#define Q_DIRECTION_HORIZONTAL(x, y) ((float)sqrt((double)((long double)(x) * (long double)(x) + (long double)(y) * (long double)(y))))
#endif
#define Q_DIRECTION_WRAP(value) ((float)((long double)(value) + 360.0L))
#endif

float vectoyaw(const vec3_t direction)
{
    float yaw;

    if (direction[1] == 0.0f && direction[0] == 0.0f) {
        return 0.0f;
    }

    yaw = Q_DIRECTION_DEGREES(direction[1], direction[0], 180.0);
    if (yaw < 0.0f) {
        yaw = Q_DIRECTION_WRAP(yaw);
    }
    return yaw;
}

float vectosignedyaw(const vec2_t direction)
{
    if (direction[1] == 0.0f && direction[0] == 0.0f) {
        return 0.0f;
    }

    return Q_DIRECTION_DEGREES(direction[1], direction[0], 180.0);
}

float vectopitch(const vec3_t direction)
{
    float pitch;

    if (direction[1] == 0.0f && direction[0] == 0.0f) {
        return direction[2] > 0.0f ? 270.0f : 90.0f;
    }

    const float horizontal = Q_DIRECTION_HORIZONTAL(direction[0], direction[1]);
    pitch = Q_DIRECTION_DEGREES(direction[2], horizontal, -180.0);
    if (pitch < 0.0f) {
        pitch = Q_DIRECTION_WRAP(pitch);
    }
    return pitch;
}

float vectosignedpitch(const vec3_t direction)
{
    if (direction[1] == 0.0f && direction[0] == 0.0f) {
        return direction[2] > 0.0f ? -90.0f : 90.0f;
    }

    const float horizontal = Q_DIRECTION_HORIZONTAL(direction[0], direction[1]);
    return Q_DIRECTION_DEGREES(direction[2], horizontal, -180.0);
}

void vectoangles(const vec3_t direction, vec3_t angles)
{
    float pitch;
    float yaw;

    if (direction[1] == 0.0f && direction[0] == 0.0f) {
        yaw = 0.0f;
        pitch = direction[2] > 0.0f ? 270.0f : 90.0f;
    } else {
        float horizontal;

        yaw = Q_DIRECTION_DEGREES(direction[1], direction[0], 180.0);
        if (yaw < 0.0f) {
            yaw = Q_DIRECTION_WRAP(yaw);
        }
        horizontal = Q_DIRECTION_HORIZONTAL(direction[0], direction[1]);
        pitch = Q_DIRECTION_DEGREES(direction[2], horizontal, -180.0);
        if (pitch < 0.0f) {
            pitch = Q_DIRECTION_WRAP(pitch);
        }
    }

    angles[0] = pitch;
    angles[1] = yaw;
    angles[2] = 0.0f;
}

void vectosignedangles(const vec3_t direction, vec3_t angles)
{
    float pitch;
    float yaw;

    if (direction[1] == 0.0f && direction[0] == 0.0f) {
        yaw = 0.0f;
        pitch = direction[2] > 0.0f ? -90.0f : 90.0f;
    } else {
        float horizontal;

        yaw = Q_DIRECTION_DEGREES(direction[1], direction[0], 180.0);
        horizontal = Q_DIRECTION_HORIZONTAL(direction[0], direction[1]);
        pitch = Q_DIRECTION_DEGREES(direction[2], horizontal, -180.0);
    }

    angles[0] = pitch;
    angles[1] = yaw;
    angles[2] = 0.0f;
}

#undef Q_DIRECTION_WRAP
#undef Q_DIRECTION_HORIZONTAL
#undef Q_DIRECTION_DEGREES
#undef Q_DIRECTION_PI
