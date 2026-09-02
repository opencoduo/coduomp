#include "q_math.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>

enum {
    ANGLE_MOD_MASK = 65535
};

/*
 * The original bodies use the same BAMS operation:
 *
 *   CoDUOMP.exe                 0x00433b70
 *   uo_cgame_mp_x86.dll        0x3004bcd0
 *   uo_ui_mp_x86.dll           0x40003ca0
 *   game.mp.uo.i386.so         RVA 0x0003dacd
 *   coduo_lnxded               0x08069da8
 *
 * Windows spills the masked 0..65535 integer through binary32 before the final
 * multiply; Linux multiplies the integer directly. Every value in that domain
 * is exactly representable as binary32, so the two instruction forms have the
 * same result and status behavior. Supporting Mac cgame/game retain the
 * canonical float AngleMod(float) contract.
 */
float AngleMod(float angle)
{
    const float angleToShort = 182.04444885253906f;
    const float shortToAngle = 0.0054931640625f;
    int32_t packed;
    float packedFloat;

#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
    packed = (int32_t)(uint32_t)x87f_store_i64_trunc(x87f_mul(x87f_load_f32(angle), x87f_load_f32(angleToShort)));
#else
    packed = x87f_store_i32_trunc(x87f_mul(x87f_load_f32(angle), x87f_load_f32(angleToShort)));
#endif
    packed &= ANGLE_MOD_MASK;
    packedFloat = x87f_store_f32(x87f_load_i32(packed));
    return x87f_store_f32(x87f_mul(x87f_load_f32(packedFloat), x87f_load_f32(shortToAngle)));
#else
    packed = coduo_fp_to_i32_extended((long double)angle * (long double)angleToShort);
    packed &= ANGLE_MOD_MASK;
    packedFloat = (float)packed;
    return (float)((double)packedFloat * shortToAngle);
#endif
}
