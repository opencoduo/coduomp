#include "q_math.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>

enum {
    ANGLE_SHORT_MASK = 65535
};

#define ANGLE_SHORT_SCALE 182.04445f
#define ANGLE_SHORT_TO_DEGREES 0.0054931640625f
#define ANGLE_HALF_CIRCLE 180.0f
#define ANGLE_FULL_CIRCLE 360.0f

/*
 * The BAMS bodies agree across the authoritative binaries:
 *
 *   CoDUOMP.exe                 0x00433cd0, 0x00433d00
 *   uo_cgame_mp_x86.dll        0x3004be30, 0x3004be60
 *   uo_ui_mp_x86.dll           0x40003e00, 0x40003e30
 *   uo_game_mp_x86.dll         0x20018e80, 0x20018eb0
 *   coduo_lnxded               0x08069f27, 0x08069f7a
 *   game.mp.uo.i386.so         RVA 0x0003dc8c, 0x0003dcdf
 *
 * The multiply of two binary32 operands is exact at both original x87
 * precision settings. The platform conversion adapter retains Windows'
 * `_ftol2` low-dword contract and Linux's signed-dword FISTP contract before
 * the common low-16-bit mask.
 */
float AngleNormalize360(float angle)
{
    const uint32_t packed = coduo_fp_to_u32_extended((long double)angle * (long double)ANGLE_SHORT_SCALE) & ANGLE_SHORT_MASK;

    return (float)packed * ANGLE_SHORT_TO_DEGREES;
}

float AngleNormalize180(float angle)
{
    float normalized = AngleNormalize360(angle);

    if (normalized > ANGLE_HALF_CIRCLE) {
        normalized = (float)((long double)normalized - (long double)ANGLE_FULL_CIRCLE);
    }
    return normalized;
}

/*
 * The loop-based Accurate pair likewise agrees behaviorally. Every add or
 * subtract is stored back to binary32 before the next comparison. Supporting
 * Mac client/game symbols use these canonical Quake3 names as well. The
 * Windows cgame bodies are 0x3004bec0 and 0x3004bf40; the Windows UI bodies
 * are 0x40003e90 and 0x40003f10.
 */
float AngleNormalize360Accurate(float angle)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (angle < 0.0f) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        do {
            const float previous = angle;
#if EMULATE_X87
            angle = x87f_store_f32(x87f_add(x87f_load_f32(angle), x87f_load_f32(ANGLE_FULL_CIRCLE)));
#else
            angle = (float)((long double)angle + (long double)ANGLE_FULL_CIRCLE);
#endif
            if (angle == previous) {
                break;
            }
        } while (angle < 0.0f);
        return angle;
    }

    if (angle >= ANGLE_FULL_CIRCLE) {
        do {
            const float previous = angle;
#if EMULATE_X87
            angle = x87f_store_f32(x87f_sub(x87f_load_f32(angle), x87f_load_f32(ANGLE_FULL_CIRCLE)));
#else
            angle = (float)((long double)angle - (long double)ANGLE_FULL_CIRCLE);
#endif
            if (angle == previous) {
                break;
            }
        } while (angle >= ANGLE_FULL_CIRCLE);
    }
    return angle;
}

float AngleNormalize180Accurate(float angle)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (angle <= -ANGLE_HALF_CIRCLE) {
        do {
            const float previous = angle;
#if EMULATE_X87
            angle = x87f_store_f32(x87f_add(x87f_load_f32(angle), x87f_load_f32(ANGLE_FULL_CIRCLE)));
#else
            angle = (float)((long double)angle + (long double)ANGLE_FULL_CIRCLE);
#endif
            if (angle == previous) {
                break;
            }
        } while (angle <= -ANGLE_HALF_CIRCLE);
    } else if (angle > ANGLE_HALF_CIRCLE) {
        do {
            const float previous = angle;
#if EMULATE_X87
            angle = x87f_store_f32(x87f_sub(x87f_load_f32(angle), x87f_load_f32(ANGLE_FULL_CIRCLE)));
#else
            angle = (float)((long double)angle - (long double)ANGLE_FULL_CIRCLE);
#endif
            if (angle == previous) {
                break;
            }
        } while (angle > ANGLE_HALF_CIRCLE);
    }
    return angle;
}
