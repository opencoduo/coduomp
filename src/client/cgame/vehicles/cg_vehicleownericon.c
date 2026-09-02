// Source: uo_cgame_mp_x86.dll 0x30021540..0x30021651
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021540_30021651.mcode
//
// CG_VehicleOwnerIcon — the CG_AddCEntity (0x30022170) jump-table handler for
// currentState.eType == 15 (ESI = cent, register ABI). It draws this entity's
// rotating HUD head-icon as three time-staggered, trapezoidally-fading sprite
// "pulses". Each frame it:
//   1. registers (idempotently) the "gfx/hud/headicon" render material;
//   2. copies currentState.pos.trBase (+0x18) into the distinct +0x208
//      interpolated lerpOrigin block that the sprite builder reads as the world
//      origin; and
//   3. loops over three 500ms-offset phases across a 1500ms cycle, computing a
//      per-phase alpha and yaw and submitting one sprite each via
//      CG_AddHudHeadIconSprite (0x300213c0).
//
// The .mcode mechanical name `PM_Weapon_CheckFiringAmmo` is REJECTED: it is a pure
// size match. This function performs no player-move / weapon / ammo work — it is a
// centity render handler that registers a HUD material and emits render sprites
// (trap_R_AddRefEntityToScene via the 0x300213c0 helper). Role name proven from the
// CG_AddCEntity dispatch (eType 15) and the "gfx/hud/headicon" material.
//
// Machine-code facts pinned:
//  - 0x30021548/4d: PUSH 5, PUSH "gfx/hud/headicon@us_rank1.dds" (0x30077184),
//    CALL 0x3003db80, ADD ESP,8 ->
//    qhandle_t = CG_RegisterMaterial("gfx/hud/headicon@us_rank1.dds", 5). That
//    callee is the generic CG material-registration wrapper (already named for its
//    flame caller); this HUD-icon caller confirms it is generic, and it is reused
//    here as-is rather than aliased.
//  - 0x30021560..6c: cent->lerpOrigin[0..2] (+0x208/+0x20c/+0x210) =
//    cent->currentState.pos.trBase (+0x18/+0x1c/+0x20). Raw dword copy into the
//    block the sprite builder reads as its world origin.
//  - loop counter EDI: 0, 500, 1000 (< 1500, step 500). 0x30021635 ADD EDI,0x1f4;
//    0x3002163e CMP EDI,0x5dc; 0x30021644 JL.
//  - 0x30021574..85: signed division. ECX=cg_time; EAX=EDI+cg_time; CDQ; IDIV 1500
//    -> EDX = remainder (phaseRem). Stored [ESP+0x10].
//  - fade curve on phaseRem (trapezoid over the 1500ms cycle):
//      phaseRem <  500 : fade = phaseRem       * 0.002f     (ramp 0 -> 1)     [0x3007bf28]
//      phaseRem <= 1000: fade = 1.0f                                          [0x3f800000]
//      else            : fade = (1500 - phaseRem) * 0.002f  (ramp 1 -> 0)
//  - tail-off gate 0x300215cc..ed: if (cg_time > cent->currentState.iconFadeEndTime - 2000)
//      fade *= (float)(cent->currentState.iconFadeEndTime - cg_time) * 0.0005f  [0x3007c058], scaled
//      down as cg_time approaches iconFadeEndTime. (0xfffff830 == -2000.)
//  - 0x300215f1..fb: alphaScale = pow((double)fade, 1.5) (0x3006bb20 = MSVC _CIpow,
//    base in ST(1), exp 1.5 (double, 0x3007c278) in ST(0)).
//  - spin: `_ftol2` truncates phaseRem * 0.053333335f toward zero;
//          yaw: `_ftol2` truncates cent->currentState.iconBaseYaw - spin toward zero.
//  - 0x30021630: CG_AddHudHeadIconSprite(cent, material, yaw, 0, 180, (float)alphaScale)
//    cdecl, 6 args, ADD ESP,0x18.

#include <math.h>
#include <string.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Trapezoidal-pulse fade ramp slope: 1/500 per ms, applied to the phase remainder
 * to ramp the pulse alpha in/out over the first/last 500ms of the 1500ms cycle.
 * 0x3007bf28 == 0.002f. */
#define CG_HEADICON_PULSE_RAMP 0.002f

/* Tail-off slope over the trailing 2000ms window ending at cent->currentState.iconFadeEndTime:
 * 1/2000 per ms. 0x3007c058 == 0.0005f. */
#define CG_HEADICON_FADEOUT_RATE 0.0005f

/* Per-ms yaw spin rate applied to the phase remainder to rotate the sprite.
 * 0x3007c280 == 0x3d5a740e == 0.053333335f (= 4/75; 0.0533333f would be
 * 9 ULP low at 0x3d5a7405). */
#define CG_HEADICON_SPIN_RATE 0.053333335f

enum {
    CG_HEADICON_CYCLE_MS = 1500, /* full pulse cycle length (IDIV divisor 0x5dc) */
    CG_HEADICON_PHASE_MS = 500,  /* per-phase offset / ramp width (0x1f4) */
    CG_HEADICON_HOLD_MS = 1000, /* full-alpha hold boundary (0x3e8) */
    CG_HEADICON_FADEOUT_MS = 2000, /* tail-off window before iconFadeEndTime */
    CG_HEADICON_ARG_ANGLE = 180   /* fixed secondary angle arg (0xb4) */
};

void CG_VehicleOwnerIcon(centity_t *cent)
{
    /* Registration is idempotent inside the callee; the returned handle is reused
     * for every sprite this frame. The flame-caller name is kept (same address,
     * no alias); the generic role is confirmed by this HUD-icon call site. */
    qhandle_t material = CG_RegisterMaterial("gfx/hud/headicon@us_rank1.dds", 5);

    /* Publish the current render origin into the +0x208 block the sprite builder
     * reads as the world origin (raw component copy). */
    memcpy(cent->lerpOrigin, cent->currentState.origin, sizeof(cent->lerpOrigin));

    for (int32_t phaseOffset = 0; phaseOffset < CG_HEADICON_CYCLE_MS; phaseOffset += CG_HEADICON_PHASE_MS) {
        int32_t now = coduo_int32_from_bits((uint32_t)cg_time);
        /* Signed remainder of (phaseOffset + cg_time) / 1500 (CDQ + IDIV). For the
         * positive time base this lands in [0, 1499]. */
        int32_t phaseTime = coduo_int32_from_bits((uint32_t)phaseOffset + (uint32_t)now);
        int32_t phaseRem = phaseTime % CG_HEADICON_CYCLE_MS;

        float fade;
        if (phaseRem < CG_HEADICON_PHASE_MS) {
            /* Bare FILD feeds the FMUL directly (0x30021591..95): no FSTP DWORD
             * before the multiply, so no (float) cast (Class 4). */
            fade = (float)((long double)phaseRem * (long double)CG_HEADICON_PULSE_RAMP);
        } else if (phaseRem <= CG_HEADICON_HOLD_MS) {
            fade = 1.0f;
        } else {
            /* (1500 - phaseRem) is an integer MOV'd to a slot then FILD'd
             * (0x300215ae..b8) — bare FILD into the FMUL, no (float) cast. */
            int32_t descending = coduo_int32_from_bits((uint32_t)CG_HEADICON_CYCLE_MS - (uint32_t)phaseRem);
            fade = (float)((long double)descending * (long double)CG_HEADICON_PULSE_RAMP);
        }

        /* Tail-off: once cg_time is within the 2000ms window ending at
         * iconFadeEndTime, scale the pulse alpha down toward 0 (signed compare). */
        int32_t fadeEnd = cent->currentState.iconFadeEndTime;
        int32_t fadeWindowStart = coduo_int32_from_bits((uint32_t)fadeEnd - (uint32_t)CG_HEADICON_FADEOUT_MS);
        if (now > fadeWindowStart) {
            /* (iconFadeEndTime - cg_time) is an integer MOV'd to a slot then FILD'd
             * (0x300215db..df) and fed straight through FMUL fade; FMUL 0.0005f with
             * no intermediate float store — bare FILD, so no (float) cast (Class 4). */
            int32_t remaining = coduo_int32_from_bits((uint32_t)fadeEnd - (uint32_t)now);
            fade = (float)(((long double)remaining * (long double)fade) * (long double)CG_HEADICON_FADEOUT_RATE);
        }

        /* alphaScale = pow(fade, 1.5): _CIpow leaves the raw 80-bit result in
         * ST(0), FSTP'd to float once at 0x30021601 (the sprite arg). fade is a
         * float loaded FLD float into ST(1); powl keeps the base and result
         * 80-bit so the single (float) narrows once (a double pow return would
         * round twice). Exp 1.5 is the double at 0x3007c278. */
        float alphaScale = (float)powl((long double)fade, 1.5L);

        /* Yaw spin: rotate the icon by the phase-driven angle relative to its base
         * yaw. Both conversions use _ftol2 (0x3006be3c), truncating toward zero. */
        /* Both int operands enter through bare FILD (phaseRem at 0x30021609, spin at
         * 0x3002161e) with no FSTP DWORD before the FMUL/FSUBR — no (float) casts
         * (Class 4). coduo_fp_to_i32_extended consumes the raw st(0) and truncates. */
        int32_t spin = coduo_fp_to_i32_extended((long double)phaseRem * (long double)CG_HEADICON_SPIN_RATE);
        int32_t yaw = coduo_fp_to_i32_extended((long double)cent->currentState.iconBaseYaw - (long double)spin);

        CG_AddHudHeadIconSprite(cent, material, yaw, 0, CG_HEADICON_ARG_ANGLE, alphaScale);
    }
}
