// Source: uo_cgame_mp_x86.dll 0x30015a10..0x30015b43
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015a10_30015b43.mcode
//
// BG_CalculateView_DamageKick — the damage-kick contributor of the
// BG_CalculateWeaponPosition_* family. Given a running weapon/view offset vector `angles`
// and the shared bg_view_angle_state_t block, it evaluates a triangle-wave damage-kick curve
// over the elapsed damage-event time count and adds the result into angles[0] (X, via
// state->viewKickPitch) and angles[2] (Z, via state->viewKickRoll). angles[1] is untouched.
//
// The .mcode pre-hint `CG_DebugArc` is REJECTED: it is a pure size guess (win 0x133 vs
// matched 0x134) from cgame_mp.dll's corpus. This function issues NO trap/syscall and
// draws nothing — no CG_ADD_DEBUG_LINE, no call at all; it is a pure float computation
// that writes two components of an offset vector. Its identity is proven by the shared
// bg_view_angle_state_t block it consumes exactly as the sibling BG_CalculateWeaponPosition_
// BobOffset (0x30015b50) does — same state->ps, the same entityStateFlags & 0x106000
// vehicle-bob gate and vehicleType==1 && vehiclePosition==3 gunner gate — and by writing
// angles[0]/angles[2] scaled by state->viewKickPitch (+0x0c) /
// viewKickRoll (+0x10), which the shared header documents as the DamageKick
// sibling's two axis scales. The family's DamageKick member had a provisional decl in
// client_recovered.h; this is its own .mcode reconstruction and supersedes it.
//
// Non-default register ABI (proven at both callers): the output vector `angles` arrives in
// ESI, the bg_view_angle_state_t block in ECX; no stack args; plain RET. The wrapper at
// 0x30015c30 zeroes `angles` then calls this (ECX = its block) followed by BobOffset; the
// view path at 0x3003fbdb builds the block on the stack (ECX = &block, ESI = &angles).
//
// DamageKick reads state->viewKickStartTime (+0x04) and
// state->time (+0x08) with INTEGER instructions (MOV EDI,[ECX+4]; TEST;
// MOV EDX,[ECX+8]; SUB EDX,EDI; FILD) — they are int32 ms time-deltas here, filled that
// way by both callers. The weapon-angle family uses the distinct, larger
// pm_weapon_angle_state_t record, whose +0x04/+0x08 fields are speed/frameTime;
// those two original records must not be conflated.
//
// .rdata float constants (dumped exact, image_base 0x30000000):
//   0x3007bce0 = 1.0f      0x3007bce4 = 2.0f     0x3007bce8 = 0.5f    0x3007bcec = 0.0f
//   0x3007bdb4 = 0.01f     (bits 0x3c23d70a, = 0.00999999977f)
//   0x3007bf54 = 0.0025f   (bits 0x3b23d70a, = 0.00249999994f)
//   0x300715ec = 100.0f    (bits 0x42c80000)
// Each of these floats is the exact nearest float to its written decimal, so the plain
// literals below compile to the same bit patterns the machine code loads.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>

#if defined(LINUX_BEHAVIOR)
#define VIEWKICK_ADS_SCALE 0.5f
#define VIEWKICK_FADE_IN_MS 100.0f
#define VIEWKICK_FADE_OUT_MS 400.0f

#if EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE: extended carrier for the inlined lean expression. */
static x87f bg_compat_view_lean_fraction_x87(float fraction)
{
    return x87f_mul(
        x87f_sub(x87f_load_f32(2.0f),
                 x87f_abs(x87f_load_f32(fraction))),
        x87f_load_f32(fraction));
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of the two original stores. */
static void bg_compat_add_view_kick_pitch_roll(
    bg_view_angle_state_t *state, vec3_t angles, float fraction)
{
#if EMULATE_X87
    angles[0] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(fraction),
                 x87f_load_f32(state->viewKickPitch)),
        x87f_load_f32(angles[0])));
    angles[2] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(fraction),
                 x87f_load_f32(state->viewKickRoll)),
        x87f_load_f32(angles[2])));
#else
    angles[0] += fraction * state->viewKickPitch;
    angles[2] += fraction * state->viewKickRoll;
#endif
}
#endif

void BG_CalculateView_DamageKick(bg_view_angle_state_t *state,
                                 vec3_t angles)
{
#if defined(WINDOWS_BEHAVIOR)
    /* 0x30015a12..0x30015a17: gate on viewKickStartTime (+0x04). */
    int32_t startDelta = state->viewKickStartTime;
    if (startDelta == 0) {
        return;
    }

    /* 0x30015a1d..0x30015a3f: same vehicle-bob gate as BobOffset. When the player state
     * carries any vehicle-bob entity flag, the land bob is kept only for the gunner pose
     * (vehicleType == 1 && vehiclePosition == 3); otherwise bail with `angles` unchanged.
     * (TEST [ps+0x84],0x106000 / JZ skips the check for a non-vehicle player.) */
    playerState_t *ps = state->ps;
    if (ps->entityStateFlags & EF_RESTRICTED_MASK) {
        if (ps->vehicleType != 1) {         /* CMP [ps+0x618],1 / JNZ 0x30015b40 */
            return;
        }
        if (ps->vehiclePosition != 3) {     /* CMP [ps+0x614],3 / JNZ 0x30015b40 */
            return;
        }
    }

    /* 0x30015a45..0x30015a99: ADS envelope `scale`.
     *   scale = 1.0f - ps->adsFraction * 0.5f
     * Then, only when the player is aiming (adsFraction != 0.0f) AND the current weapon's
     * adsOverlayReticle (+0x284) is set, fold in the reciprocal factor:
     *   scale *= (1.0f + ps->adsFraction * 0.5f)
     * (0x30015a63 FUCOMPP ps->adsFraction vs 0.0f; TEST AH,0x44 / JNP skips when equal.
     * 0x30015a6c loads bg_weaponInfos[ps->currentWeapon]; TEST [+0x284] / JZ skips when 0.) */
    /* scale is never spilled: it rides the x87 stack from 0x30015a51 through
     * the final FMULP ST2 at 0x30015b23, so it is long double (a float local
     * would round where the DLL does not). */
    long double scale = 1.0f - (long double)ps->adsFraction * 0.5f; /* [0x3007bce8]=0.5, [0x3007bce0]=1.0 */
    if (ps->adsFraction != 0.0f) {                                  /* [0x3007bcec]=0.0 */
        int32_t currentWeapon = ps->currentWeapon;
        weaponInfo_t **weaponTable = bg_weaponInfos;
        weaponInfo_t *weaponInfo = weaponTable[currentWeapon];       /* [0x30134cd8][currentWeapon] */
        if (weaponInfo->adsOverlayReticle != 0) {                  /* +0x284 */
            scale *= 1.0f + (long double)ps->adsFraction * 0.5f;    /* FMULP, 80-bit */
        }
    }

    /* 0x30015a9b..0x30015aa4: elapsed damage-kick count loaded directly into
     * x87 extended precision with FILD dword; there is no float conversion. */
    int32_t elapsed = coduo_int32_from_bits(
        (uint32_t)state->time - (uint32_t)startDelta);
    long double t = (long double)elapsed; /* FILD dword: no intervening float rounding */

    /* 0x30015aa8..0x30015b23: piecewise triangle-wave curve, scaled by the ADS envelope.
     * FCOM t vs 100.0f; TEST AH,5 / JP selects the branch (t < 100.0f vs t >= 100.0f). */
    /* curve too stays in ST0 unrounded into the angles[]-store FMULs at
     * 0x30015b28/0x30015b31; only a float COPY of u/v is spilled (FST, no
     * pop) to feed the AND-0x7fffffff fabs, while the multiply consumes the
     * unrounded 80-bit value -- modeled by the (float) copies below. */
    long double curve;
    if (t < 100.0f) {                                              /* [0x300715ec]=100.0 */
        /* 0x30015ab5..0x30015ad8: rising ramp. u = t * 0.01f;
         *   curve = scale * (2.0f - fabsf((float)u)) * u
         * (fabsf via AND 0x7fffffff on the stored bit pattern; [0x3007bdb4]=0.01,
         * [0x3007bce4]=2.0. FST at 0x30015abb rounds only the fabs copy.) */
        long double u = (long double)t * 0.01f;
        float uf = (float)u;                                       /* FST [ESP+0x4] */
        curve = scale *
            ((2.0L - (long double)fabsf(uf)) * u);
    } else {
        /* 0x30015ada..0x30015b1d: falling tail.
         *   w = 1.0f - (t - 100.0f) * 0.0025f      // [0x3007bf54]=0.0025, [0x3007bce0]=1.0
         * w and v ride the x87 stack unrounded; only the fabs copy of v is
         * stored (FST at 0x30015aff).
         * If w <= 0.0f (or unordered) the whole contribution is discarded with `angles`
         * unchanged (0x30015aec FCOM w vs 0.0f; TEST AH,0x41 / JNZ 0x30015b3c drops the
         * x87 stack and returns). Otherwise:
         *   curve = scale * (1.0f - (2.0f - fabsf((float)v)) * v) */
        long double w = 1.0f - ((long double)t - 100.0f) * 0.0025f;
        if (!(w > 0.0f)) {                                        /* w <= 0 or NaN -> no write */
            return;
        }
        long double v = 1.0f - w;
        float vf = (float)v;                                       /* FST [ESP+0x4] */
        curve = scale *
            (1.0L - (2.0L - (long double)fabsf(vf)) * v);
    }

    /* 0x30015b28..0x30015b37: accumulate the curve into the X and Z axes.
     *   angles[0] += curve * state->viewKickPitch   // FADD [ESI]
     *   angles[2] += curve * state->viewKickRoll   // FADD [ESI+8] */
    angles[0] += curve * state->viewKickPitch;      /* +0x0c */
    angles[2] += curve * state->viewKickRoll;      /* +0x10 */
#else
    playerState_t *ps;
    const weaponInfo_t *weapon;
    float adsFraction;
    float scale;
    float elapsed;

    if (state->viewKickStartTime == 0) {
        return;
    }

    ps = state->ps;
    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0 &&
        BG_AllowPlayerWeaponAtVehiclePos(
            ps->vehicleType, ps->vehiclePosition) == 0) {
        return;
    }

    weapon = BG_GetInfoForWeapon(ps->currentWeapon);
    adsFraction = ps->adsFraction;
#if EMULATE_X87
    scale = x87f_store_f32(x87f_sub(
        x87f_load_f32(1.0f),
        x87f_mul(x87f_load_f32(adsFraction),
                 x87f_load_f32(VIEWKICK_ADS_SCALE))));
#else
    scale = 1.0f - adsFraction * VIEWKICK_ADS_SCALE;
#endif

    if (adsFraction != 0.0f && weapon->adsOverlayReticle != 0) {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_mul(
            x87f_load_f32(scale),
            x87f_add(
                x87f_mul(x87f_load_f32(adsFraction),
                         x87f_load_f32(VIEWKICK_ADS_SCALE)),
                x87f_load_f32(1.0f))));
#else
        scale *= adsFraction * VIEWKICK_ADS_SCALE + 1.0f;
#endif
    }

    elapsed = (float)coduo_int32_from_bits(
        (uint32_t)state->time - (uint32_t)state->viewKickStartTime);
    if (elapsed < VIEWKICK_FADE_IN_MS) {
#if EMULATE_X87
        const float leanArg = x87f_store_f32(x87f_div(
            x87f_load_f32(elapsed),
            x87f_load_f32(VIEWKICK_FADE_IN_MS)));
        bg_compat_add_view_kick_pitch_roll(
            state, angles,
            x87f_store_f32(x87f_mul(
                bg_compat_view_lean_fraction_x87(leanArg),
                x87f_load_f32(scale))));
#else
        bg_compat_add_view_kick_pitch_roll(
            state, angles,
            GetLeanFraction(elapsed / VIEWKICK_FADE_IN_MS) * scale);
#endif
    } else {
        float fadeOutFraction;

#if EMULATE_X87
        fadeOutFraction = x87f_store_f32(x87f_sub(
            x87f_load_f32(1.0f),
            x87f_div(
                x87f_sub(x87f_load_f32(elapsed),
                         x87f_load_f32(VIEWKICK_FADE_IN_MS)),
                x87f_load_f32(VIEWKICK_FADE_OUT_MS))));
#else
        fadeOutFraction =
            1.0f - (elapsed - VIEWKICK_FADE_IN_MS) /
                       VIEWKICK_FADE_OUT_MS;
#endif
        if (fadeOutFraction > 0.0f) {
            float fadeOutLean;

#if EMULATE_X87
            const float leanArg = x87f_store_f32(x87f_sub(
                x87f_load_f32(1.0f),
                x87f_load_f32(fadeOutFraction)));
            fadeOutLean = x87f_store_f32(x87f_sub(
                x87f_load_f32(1.0f),
                bg_compat_view_lean_fraction_x87(leanArg)));
#else
            fadeOutLean =
                1.0f - GetLeanFraction(1.0f - fadeOutFraction);
#endif
            bg_compat_add_view_kick_pitch_roll(
                state, angles, fadeOutLean * scale);
        }
    }
#endif
}

#if defined(LINUX_BEHAVIOR)
#undef VIEWKICK_ADS_SCALE
#undef VIEWKICK_FADE_IN_MS
#undef VIEWKICK_FADE_OUT_MS
#endif
