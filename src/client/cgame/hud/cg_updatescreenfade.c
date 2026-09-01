// Source: uo_cgame_mp_x86.dll 0x3001ab90..0x3001acb6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001ab90_3001acb6.mcode
//
// CG_UpdateScreenFade — advance and draw the full-screen fade/flash overlay.
//
// NAME: reuses the converged symbol CG_UpdateScreenFade already assigned to
// 0x3001ab90 by the CG_Draw2D callee cluster in client_recovered.h
// (ABI void(void), which this body confirms: no args, no register inputs, no
// consumed return, plain RET). The body both advances the fade state and, when
// visible, draws it via CG_FillRect over the virtual 640x480 screen.
//
// The .mcode header's mechanical pre-hint `script_func_getangledelta` (a pure
// size-match, win size 0x126 == corpus size 0x126) is REJECTED. This function
// takes no angle-vector arguments, does no AngleSubtract / [-180,180] wrap, and
// uses no 180/360 constants. It reads a fixed BSS state block, does a time-driven
// RGBA lerp with 1.0f/2.0f/0.5f/0.0f from the .rdata float pool at 0x3007bce0,
// and fills the whole screen. It is a HUD fade draw, not script angle math.
//
// .rdata float pool (dumped from the binary, do not infer neighbours):
//   0x3007bce0 = 1.0f, 0x3007bce4 = 2.0f, 0x3007bce8 = 0.5f, 0x3007bcec = 0.0f.
// Only 0x3007bce0 (1.0f) and 0x3007bcec (0.0f) are referenced here.
//
// State: cg_screenFade (globals.h), a 10-dword BSS block at 0x3048adc0..0x3048ade4:
//   .endTime   int    (+0x00)  cg_time ms at which the fade completes
//   .rate      float  (+0x04)  1/duration; 0.0f => inactive (early-out)
//   .color[4]  float  (+0x08)  RGBA composed and drawn this frame
//   .fromColor[4] float (+0x18) stored source RGBA of the fade
//
// Time delta is computed signed: (int)(endTime - cg_time). cg_time is the
// cgame time base at 0x304831b0 (uint32_t); the SUB/JG at 0x3001abb1..bc treat
// the result as a signed ms remaining.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_UpdateScreenFade(void)
{
    /* 0x3001ab90..aba6: FLD 0.0 / FLD rate / FUCOMPP; equal => JNP taken => exit.
     * The fade is inactive while rate == 0.0f. */
    if (cg_screenFade.rate == 0.0f) {
        return;
    }

    /* 0x3001abac..bc: signed ms remaining = endTime - cg_time; JG => still active. */
    int32_t endTime = cg_screenFade.endTime;
    int32_t now = cg_time;
    int32_t remaining = coduo_int32_from_bits(
        (uint32_t)endTime - (uint32_t)now);

    if (remaining > 0) {
        /* 0x3001ac17..ac1a: frac = (float)remaining * rate. long double: the FILD/FMUL
         * result is never stored -- frac stays in ST1 (80-bit) across all four
         * component lerps below; a float local would insert a rounding the DLL lacks. */
        long double frac =
            (long double)remaining * (long double)cg_screenFade.rate;
        /* 0x3001ac20..ac26: t = 1.0f - frac (1.0f from 0x3007bce0); also kept
         * unstored in ST0 across the loop. */
        long double t = (long double)1.0f - frac;

        /* 0x3001ac28..ac7c: per component, out[i] = color[i]*frac + fromColor[i]*t.
         * x87 keeps t in ST0 and frac in ST1 across all four components; each
         * block computes fromColor[i]*t (FMUL ST1) plus color[i]*frac (FMUL ST3). */
        vec4_t out;
        out[0] = (float)(
            (long double)cg_screenFade.fromColor[0] * t
            + (long double)cg_screenFade.color[0] * frac);
        out[1] = (float)(
            (long double)cg_screenFade.fromColor[1] * t
            + (long double)cg_screenFade.color[1] * frac);
        out[2] = (float)(
            (long double)cg_screenFade.fromColor[2] * t
            + (long double)cg_screenFade.color[2] * frac);
        out[3] = (float)(
            (long double)cg_screenFade.fromColor[3] * t
            + (long double)cg_screenFade.color[3] * frac);

        /* 0x3001ac84..ac95: FLD 0.0 / FLD out[3] / FUCOMPP; equal => JNP => exit.
         * Skip the draw entirely when the composed alpha is zero. */
        if (out[3] != 0.0f) {
            /* 0x3001ac97..acaa: CG_FillRect(0, 0, 640.0f, 480.0f, out).
             * 0x44200000 = 640.0f (width), 0x43f00000 = 480.0f (height). */
            /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): preserve the
             * complete effect's native-drawable coverage on widescreen. */
            cgame_compat_fill_native_screen_effect(out);
        }
        return; /* 0x3001acb2: RET (shared epilogue) */
    }

    /* remaining <= 0: the fade has reached its end time. Latch the current color
     * to the stored source color (0x3001abc4..abf4: color[] <- fromColor[]). */
    uint32_t from0Bits = (uint32_t)CG_FloatBits(cg_screenFade.fromColor[0]);
    float fromAlphaForCompare = cg_screenFade.fromColor[3];
    uint32_t from1Bits = (uint32_t)CG_FloatBits(cg_screenFade.fromColor[1]);
    uint32_t from2Bits = (uint32_t)CG_FloatBits(cg_screenFade.fromColor[2]);
    qboolean settledVisible = fromAlphaForCompare != 0.0f;
    cg_screenFade.color[0] = CG_FloatFromBits(from0Bits);
    uint32_t from3Bits = (uint32_t)CG_FloatBits(cg_screenFade.fromColor[3]);
    cg_screenFade.color[3] = CG_FloatFromBits(from3Bits);
    cg_screenFade.color[1] = CG_FloatFromBits(from1Bits);
    cg_screenFade.color[2] = CG_FloatFromBits(from2Bits);

    /* 0x3001abbe..abfd: FLD 0.0 / FLD fromColor[3] / FUCOMPP; JP taken when
     * fromColor[3] != 0.0f (draws the settled color), else fall through. */
    if (settledVisible) {
        /* 0x3001ac0d..ac12 then 0x3001ac9c..acaa: push &cg_screenFade.color, then
         * CG_FillRect(0, 0, 640.0f, 480.0f, cg_screenFade.color). The fade stays
         * active (rate is NOT cleared here), so the settled color keeps drawing. */
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): settled full-screen
         * fades retain the same native coverage as their animated phase. */
        cgame_compat_fill_native_screen_effect(cg_screenFade.color);
        return; /* 0x3001acb2: RET */
    }

    /* 0x3001abff..ac0c: fromColor[3] == 0.0f => the fade is fully done; clear
     * rate to mark inactive and return. */
    cg_screenFade.rate = 0.0f;
}
