#include "bg_bullet.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "compat/crt/random_compat.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    BG_BULLET_VECTOR_LANES = 3
};

#define BG_BULLET_TRACE_DISTANCE 10000.0f

/*
 * Complete random bullet-spread subsystem.  The Windows cgame/game bodies are
 * instruction-identical after relocating constants, calls, and data:
 *
 *   uo_cgame_mp_x86.dll  gunrandom 0x30049270, BG_Bullet_Endpos 0x3000fa20
 *   uo_game_mp_x86.dll   gunrandom 0x200162c0, BG_Bullet_Endpos 0x2000f7d0
 *
 * Linux game retains the same random-disk and endpoint geometry at RVAs
 * 0x000399e0 and 0x0003011c.  Whole platform bodies preserve the genuine CRT,
 * constant-width, tangent boundary, and lateral-product spill differences.
 */

#if defined(WINDOWS_BEHAVIOR)
void gunrandom(float *x, float *y)
{
    const float angleSample = (float)coduo_server_rand();
    float angleDegrees;
    float radiusSample;
    float radius;
    float radians;
    float sine;
    float cosine;

#if EMULATE_X87
    angleDegrees = x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(angleSample), x87f_load_f32(32768.0f)), x87f_load_f32(360.0f)));
#else
    angleDegrees = (float)(((long double)angleSample / 32768.0f) * 360.0f);
#endif

    radiusSample = (float)coduo_server_rand();
#if EMULATE_X87
    radius = x87f_store_f32(x87f_div(x87f_load_f32(radiusSample), x87f_load_f32(32768.0f)));
    radians = x87f_store_f32(x87f_div(x87f_mul(x87f_load_f32(angleDegrees), x87f_load_f32(3.1415927f)), x87f_load_f32(180.0f)));
#else
    radius = (float)((long double)radiusSample / 32768.0f);
    radians = (float)(((long double)angleDegrees * 3.1415927f) / 180.0f);
#endif

    BG_SinCos(radians, &sine, &cosine);
#if EMULATE_X87
    *x = x87f_store_f32(x87f_mul(x87f_load_f32(cosine), x87f_load_f32(radius)));
    *y = x87f_store_f32(x87f_mul(x87f_load_f32(sine), x87f_load_f32(radius)));
#else
    *x = (float)((long double)cosine * radius);
    *y = (float)((long double)sine * radius);
#endif
}
#else
void gunrandom(float *x, float *y)
{
    const int32_t angleSample = coduo_server_rand();
    float angleDegrees;
    int32_t radiusSample;
    float radius;
    float radians;
    float sine;
    float cosine;

#if EMULATE_X87
    angleDegrees = x87f_store_f32(x87f_mul(x87f_div(x87f_load_i32(angleSample), x87f_load_f32(2147483648.0f)), x87f_load_f32(360.0f)));
#else
    angleDegrees = (float)(((long double)angleSample / 2147483648.0f) * 360.0f);
#endif

    radiusSample = coduo_server_rand();
#if EMULATE_X87
    radius = x87f_store_f32(x87f_div(x87f_load_i32(radiusSample), x87f_load_f32(2147483648.0f)));
    radians = x87f_store_f32(x87f_div(x87f_mul(x87f_load_f32(angleDegrees), x87f_load_f64(3.141592653589793)), x87f_load_f64(180.0)));
#else
    radius = (float)((long double)radiusSample / 2147483648.0f);
    radians = (float)(((long double)angleDegrees * 3.141592653589793L) / 180.0L);
#endif

    BG_SinCos(radians, &sine, &cosine);
#if EMULATE_X87
    *x = x87f_store_f32(x87f_mul(x87f_load_f32(radius), x87f_load_f32(cosine)));
    *y = x87f_store_f32(x87f_mul(x87f_load_f32(radius), x87f_load_f32(sine)));
#else
    *x = (float)((long double)radius * cosine);
    *y = (float)((long double)radius * sine);
#endif
}
#endif

#if defined(WINDOWS_BEHAVIOR)
void BG_Bullet_Endpos(float spread, vec3_t end, const float *muzzlePoints)
{
    const float *const forward = muzzlePoints;
    const float *const right = muzzlePoints + 3;
    const float *const up = muzzlePoints + 6;
    const float *const origin = muzzlePoints + 9;
    float randomRight;
    float randomUp;
    float radius;

#if EMULATE_X87
    const x87f radians = x87f_mul(x87f_mul(x87f_load_f32(spread), x87f_load_f32(3.1415927f)), x87f_load_f32(0.0055555557f));
    const double radiansCarrier = x87f_store_f64(radians);
    const double tangent = (double)coduo_x87_tanl(radiansCarrier);

    radius = x87f_store_f32(x87f_mul(x87f_load_f64(tangent), x87f_load_f32(BG_BULLET_TRACE_DISTANCE)));
#else
    long double radians = (long double)spread * 3.1415927f;

    radians *= 0.0055555557f;
    radius = (float)(coduo_x87_tanl(radians) * (long double)BG_BULLET_TRACE_DISTANCE);
#endif

    gunrandom(&randomRight, &randomUp);

#if EMULATE_X87
    const x87f lateralRight = x87f_mul(x87f_load_f32(randomRight), x87f_load_f32(radius));
    const x87f lateralUp = x87f_mul(x87f_load_f32(randomUp), x87f_load_f32(radius));

    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = x87f_store_f32(
            x87f_add(x87f_mul(x87f_load_f32(forward[lane]), x87f_load_f32(BG_BULLET_TRACE_DISTANCE)), x87f_load_f32(origin[lane])));
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = x87f_store_f32(x87f_add(x87f_mul(lateralRight, x87f_load_f32(right[lane])), x87f_load_f32(end[lane])));
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = x87f_store_f32(x87f_add(x87f_mul(lateralUp, x87f_load_f32(up[lane])), x87f_load_f32(end[lane])));
    }
#else
    const long double lateralRight = (long double)randomRight * radius;
    const long double lateralUp = (long double)randomUp * radius;

    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = (float)((long double)forward[lane] * BG_BULLET_TRACE_DISTANCE + origin[lane]);
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = (float)(lateralRight * right[lane] + end[lane]);
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = (float)(lateralUp * up[lane] + end[lane]);
    }
#endif
}
#else
void BG_Bullet_Endpos(float spread, vec3_t end, const float *muzzlePoints)
{
    const float *const forward = muzzlePoints;
    const float *const right = muzzlePoints + 3;
    const float *const up = muzzlePoints + 6;
    const float *const origin = muzzlePoints + 9;
    double tangentArgument;
    float tangent;
    float radius;
    float randomRight;
    float randomUp;

#if EMULATE_X87
    tangentArgument = x87f_store_f64(x87f_div(x87f_mul(x87f_load_f32(spread), x87f_load_f64(3.141592653589793)), x87f_load_f64(180.0)));
#else
    tangentArgument = (double)(((long double)spread * 3.141592653589793L) / 180.0L);
#endif
    tangent = (float)tan(tangentArgument);
#if EMULATE_X87
    radius = x87f_store_f32(x87f_mul(x87f_load_f32(tangent), x87f_load_f32(BG_BULLET_TRACE_DISTANCE)));
#else
    radius = tangent * BG_BULLET_TRACE_DISTANCE;
#endif

    gunrandom(&randomRight, &randomUp);
#if EMULATE_X87
    randomRight = x87f_store_f32(x87f_mul(x87f_load_f32(randomRight), x87f_load_f32(radius)));
    randomUp = x87f_store_f32(x87f_mul(x87f_load_f32(randomUp), x87f_load_f32(radius)));

    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = x87f_store_f32(
            x87f_add(x87f_mul(x87f_load_f32(forward[lane]), x87f_load_f32(BG_BULLET_TRACE_DISTANCE)), x87f_load_f32(origin[lane])));
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(right[lane]), x87f_load_f32(randomRight)), x87f_load_f32(end[lane])));
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(up[lane]), x87f_load_f32(randomUp)), x87f_load_f32(end[lane])));
    }
#else
    randomRight *= radius;
    randomUp *= radius;

    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] = forward[lane] * BG_BULLET_TRACE_DISTANCE + origin[lane];
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] += right[lane] * randomRight;
    }
    for (int32_t lane = 0; lane < BG_BULLET_VECTOR_LANES; ++lane) {
        end[lane] += up[lane] * randomUp;
    }
#endif
}
#endif

#undef BG_BULLET_TRACE_DISTANCE
