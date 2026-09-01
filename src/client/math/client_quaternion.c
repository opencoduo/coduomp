#include "client_math.h"

#include "compat/coduo_native_x87.h"

/*
 * The three per-axis quaternion constructors are instruction-identical between
 * uo_cgame_mp_x86.dll and uo_ui_mp_x86.dll:
 *
 *   QuatFromAngleY  0x3004bad0 / 0x40003aa0
 *   QuatFromAngleZ  0x3004bb20 / 0x40003af0
 *   QuatFromAngleX  0x3004bb70 / 0x40003b40
 */

void QuatFromAngleY(vec4_t output, float angle)
{
    const float halfAngle = angle * 0.008726646f;
    float sine;
    float cosine;

    output[0] = 0.0f;
    output[2] = 0.0f;
    coduo_x87_sincosf(halfAngle, &sine, &cosine);
    output[3] = cosine;
    output[1] = sine;
}

void QuatFromAngleZ(vec4_t output, float angle)
{
    const float halfAngle = angle * 0.008726646f;
    float sine;
    float cosine;

    output[0] = 0.0f;
    output[1] = 0.0f;
    coduo_x87_sincosf(halfAngle, &sine, &cosine);
    output[3] = cosine;
    output[2] = sine;
}

void QuatFromAngleX(float angle, vec4_t output)
{
    const float halfAngle = angle * 0.008726646f;
    float sine;
    float cosine;

    output[1] = 0.0f;
    output[2] = 0.0f;
    coduo_x87_sincosf(halfAngle, &sine, &cosine);
    output[3] = cosine;
    output[0] = sine;
}
