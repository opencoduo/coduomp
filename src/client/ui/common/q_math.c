#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40003f90..0x40003fe6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40003f90_40003fe6.mcode
// Class 10 (docs/float-rounding-audit-classes.md): the DLL INLINES the whole
// packed-angle normalize here (no CALL): FLD [ESP+4]; FSUB [ESP+8] keeps
// (left - right) in an 80-bit x87 register -- it is NOT stored to a float slot --
// straight into FMUL [0x40035950] (65536/360); _ftol2 truncate @0x4002cda0;
// AND 0xffff; FILD; FSTP (round); FMUL [0x4003594c] (360/65536); FSTP (round);
// FCOMP [0x40035940] (180.0f); if > 180 FSUB [0x40035944] (360.0f). Calling
// AngleNormalize180(left - right) instead rounds (left - right) to float
// at the call boundary before the *65536/360 multiply, a rounding-placement
// divergence. Inline it, keeping (left - right) unrounded into the multiply --
// the body mirrors AngleNormalize360 plus the >180 subtract, and the (int32_t)
// cast models _ftol2's truncate-toward-zero exactly as the siblings do.
// (cgame_mp's AngleDelta at 0x3004bfc0 genuinely CALLs the helper, so only
// ui_mp inlines; powl/_CIpow-style transcendental caveats do not apply here.)
float AngleDelta(float left, float right)
{
    const uint32_t packed =
        (uint32_t)coduo_fp_to_i32_extended(
            ((long double)left - right) * (65536.0f / 360.0f)) &
        65535u;
    const float packedFloat = (float)(int32_t)packed;
    float delta =
        (float)((long double)packedFloat * (360.0f / 65536.0f));

    if (delta > 180.0f) {
        delta = (float)((long double)delta - 360.0f);
    }
    return delta;
}
