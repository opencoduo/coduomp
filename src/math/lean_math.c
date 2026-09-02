#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#include <math.h>
#include <stddef.h>

/*
 * Complete shared lean-curve helper cluster.  The three canonical names are
 * retained by the supporting Mac engine, cgame, and game images.  The Windows
 * cgame and UI bodies are instruction-identical, and the Windows engine bodies
 * use the same operation graph:
 *
 *   CoDUOMP.exe             0x00450530, 0x00450560, 0x00450580
 *   uo_cgame_mp_x86.dll    0x3004f320, 0x3004f350, 0x3004f370
 *   uo_ui_mp_x86.dll       0x40007350, 0x40007380, 0x400073a0
 *   game.mp.uo.i386.so     0x00094623, 0x00094651, 0x00094688
 *
 * Linux game retains calls to GetLeanFraction and AngleVectors where the
 * optimized Windows bodies inline them.  Those are compiler decisions, not
 * separate platform behavior.  GetLeanFraction and UnGetLeanFraction have
 * binary32 source interfaces; native i386 may nevertheless retain the result
 * in ST0 until its caller spills it.
 */

#if EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE: native non-x87 representation adapter used only
 * within this shared original-function cluster. */
static x87f coduo_compat_get_lean_fraction_x87(float fraction)
{
    return x87f_mul(x87f_sub(x87f_load_f32(2.0f), x87f_abs(x87f_load_f32(fraction))), x87f_load_f32(fraction));
}
#endif

float GetLeanFraction(float fraction)
{
#if EMULATE_X87
    return x87f_store_f32(coduo_compat_get_lean_fraction_x87(fraction));
#else
    return (2.0f - fabsf(fraction)) * fraction;
#endif
}

float UnGetLeanFraction(float fraction)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_sqrt(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(fraction)))));
#else
    return (float)(1.0L - coduo_x87_sqrtl(1.0L - (long double)fraction));
#endif
}

void AddLeanToPosition(vec3_t position, float yaw, float leanFraction, float maxLean, float side)
{
    float lean;
    vec3_t angles;
    vec3_t right;

    if (leanFraction == 0.0f) {
        return;
    }

#if EMULATE_X87
    lean = x87f_store_f32(coduo_compat_get_lean_fraction_x87(leanFraction));
    angles[2] = x87f_store_f32(x87f_mul(x87f_load_f32(lean), x87f_load_f32(maxLean)));
#else
    lean = GetLeanFraction(leanFraction);
    angles[2] = lean * maxLean;
#endif
    angles[0] = 0.0f;
    angles[1] = yaw;
    AngleVectors(angles, NULL, right, NULL);

#if EMULATE_X87
    lean = x87f_store_f32(x87f_mul(x87f_load_f32(lean), x87f_load_f32(side)));
    position[0] = x87f_store_f32(x87f_add(x87f_load_f32(position[0]), x87f_mul(x87f_load_f32(right[0]), x87f_load_f32(lean))));
    position[1] = x87f_store_f32(x87f_add(x87f_load_f32(position[1]), x87f_mul(x87f_load_f32(right[1]), x87f_load_f32(lean))));
    position[2] = x87f_store_f32(x87f_add(x87f_load_f32(position[2]), x87f_mul(x87f_load_f32(right[2]), x87f_load_f32(lean))));
#else
    lean *= side;
    position[0] += right[0] * lean;
    position[1] += right[1] * lean;
    position[2] += right[2] * lean;
#endif
}
