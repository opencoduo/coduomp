// Source: uo_cgame_mp_x86.dll 0x300151d0..0x300152e3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300151d0_300152e3.mcode
//
// BG_CalculateWeaponPosition_IdleAngles - one of the five angle-component contributors
// accumulated by BG_CalculateWeaponAngles (0x30015920). It adds a slow,
// low-frequency idle "breathing" sway to the running weapon/view ANGLE offset vector
// `angles` (angles[0]=pitch, angles[1]=yaw, angles[2]=roll), driven by three de-tuned sines of the
// client game time.
//
// The .mcode header's size-only pre-hint CG_CheckAmmo (win size 0x113 ~ 0x114) is
// REJECTED: this function performs no ammo/weapon-switch/HUD logic. It reads the
// current weapon's idle-sway magnitude keys (weaponInfo_t idleSwayAds/idleSwayHip at
// +0x2cc/+0x2d0) and its stance idle-sway scales (idleSwayCrouchScale/idleSwayProneScale
// at +0x2d4/+0x2d8), maintains a smoothed idle-sway scale on the state block, and writes
// a 3-float angle drift into `angles`. Resolved by behavior + call graph (it is the
// AddIdleSway slot of the BG_CalculateWeaponAngles family) and by the
// server weaponInfo_s idle-sway field cluster (idleSway* @0x2cc..0x2d8) it consumes.
//
// Register ABI (non-default; proven at the caller 0x30015920): `angles` arrives in EDI
// (BG_CalculateWeaponAngles keeps angles in EDI from its own prologue
// `MOV EDI,EAX`, and it is live across the `CALL 0x300151d0`), and the state parameter
// block arrives in ECX. No stack args; plain RET (ECX/EDX/EAX are scratch; ESI is the
// function's own PUSH/POP). Expressed here as (angles, state). The address suffix on the
// prior provisional name BG_CalcWeaponAngles_AddAdsSpread_300151d0 is dropped: this is
// idle sway, not ADS spread.
//
// The state block is the shared pm_weapon_angle_state_s (pm_weapon_angle_state_t): state->ps at
// +0x00, state->frameTime (float seconds) at +0x08, state->idleScale (float, the smoothed
// idle-sway scale this function maintains) at +0x18, state->time (int ms = cg.time) at
// +0x1c. frameTime/idleScale offsets and their float type are proven by the machine code
// here (FLD [state+0x08], FLD/FST/FSTP [state+0x18]) plus the caller's float store into
// state+0x08 (0x30046570: FILD cg.frametime / FMUL 0.001f / FSTP [state+0x08]); the earlier
// int32 curTime label at +0x08 was a provisional misread and has been superseded.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>

/*
 * .rdata float constants used here (exact addresses dumped from the binary):
 *   0x3007bce0 = 1.0f     (default stance idle-sway scale: neither prone nor crouch)
 *   0x3007bce8 = 0.5f     (idle-sway ramp step = frameTime * 0.5)
 *   0x3007bcec = 0.0f     (comparison zero)
 *   0x3007bfb0 = 80.0f    (default idle-sway magnitude when idleSwayHip == 0.0f, non-ADS)
 *   0x3007c058 = 0.0005f  (roll  angles[2] sine frequency, * cg.time)
 *   0x3007c054 = 0.04f    (roll  angles[2] sine amplitude scale)
 *   0x3007c050 = 0.0007f  (yaw   angles[1] sine frequency, * cg.time)
 *   0x3007bd94 = 0.001f   (pitch angles[0] sine frequency, * cg.time)
 *   0x3007bdb4 = 0.01f    (yaw angles[1] and pitch angles[0] sine amplitude scale)
 * All are exactly the natural literals below (values verified against the .rdata bytes).
 */

void BG_CalculateWeaponPosition_IdleAngles(pm_weapon_angle_state_t *state, vec3_t angles)
{
#if defined(WINDOWS_BEHAVIOR)
    /* 0x300151d0..0x300151e2: weaponInfo = bg_weaponInfos[state->ps->currentWeapon].
     * (EDX = bg_weaponInfos base @0x30134cd8; ESI = state->ps = *ECX;
     *  EAX = ps->currentWeapon @+0xd8; EDX = bg_weaponInfos[currentWeapon].) */
    weaponInfo_t **weaponTable = bg_weaponInfos;
    playerState_t *ps = state->ps;
    weaponInfo_t *weaponInfo = weaponTable[ps->currentWeapon];

    /* 0x300151e2..0x30015229: swayMag = base idle-sway magnitude.
     * If the weapon supports ADS (adsEnabled != 0), lerp from idleSwayHip toward
     * idleSwayAds by the current ADS fraction:
     *   swayMag = (idleSwayAds - idleSwayHip) * ps->adsFraction + idleSwayHip
     * (FLD [wi+0x2cc] / FSUB [wi+0x2d0] / FMUL [ps+0xe0] / FADD [wi+0x2d0]).
     * Otherwise use idleSwayHip, but fall back to 80.0f when idleSwayHip is exactly
     * 0.0f. The non-ADS compare is FLD 0.0f (ST1) / FLD idleSwayHip (ST0) / FUCOMPP /
     * FNSTSW / TEST AH,0x44 / JNP: the JNP is taken only when idleSwayHip == 0.0f
     * (equal -> single C3 bit -> odd parity), selecting 80.0f; any nonzero (or NaN)
     * keeps idleSwayHip. */
    /* swayMag is never spilled: it rides the x87 stack (ST1/ST2) from here
     * through the final amp FMUL at 0x30015294 and the FSTP ST0 discard at
     * 0x300152e0 -- long double, so the ADS lerp chain stays 80-bit as in
     * the DLL. */
    long double swayMag;
    if (weaponInfo->adsEnabled != 0) {
        swayMag = ((long double)weaponInfo->idleSwayAds - weaponInfo->idleSwayHip) * ps->adsFraction + weaponInfo->idleSwayHip;
    } else if (weaponInfo->idleSwayHip != 0.0f) {
        swayMag = weaponInfo->idleSwayHip;
    } else {
        swayMag = 80.0f;
    }

    /* 0x30015229..0x3001524e: stanceScale = idle-sway scale target for the current
     * stance (read from ps->entityStateFlags @+0x84; prone checked before crouch):
     *   prone  (& EF_PRONE)     -> idleSwayProneScale  (wi+0x2d8)
     *   crouch (& EF_CROUCHING) -> idleSwayCrouchScale (wi+0x2d4)
     *   else                           -> 1.0f
     * (The POP ESI at 0x30015231 restores ESI; entityStateFlags was already loaded.) */
    uint32_t eFlags = ps->entityStateFlags;
    float stanceScale;
    if (eFlags & EF_PRONE) {
        stanceScale = weaponInfo->idleSwayProneScale;
    } else if (eFlags & EF_CROUCHING) {
        stanceScale = weaponInfo->idleSwayCrouchScale;
    } else {
        stanceScale = 1.0f;
    }

    /* 0x3001524e..0x30015292: ramp state->idleScale toward stanceScale by a single
     * step of (state->frameTime * 0.5) per call, clamping so it does not overshoot the
     * target.
     *
     * Machine detail: FLD idleScale / FLD stanceScale(dup) / FUCOMPP compares
     * stanceScale vs idleScale; when they are already equal the whole ramp is skipped
     * (JNP 0x30015292). Otherwise FCOM decides the direction (stanceScale > idleScale
     * -> add path; < -> subtract path). Each path takes one step (step = frameTime*0.5),
     * FST-stores the result back into idleScale, then re-compares against the target:
     * if it reached/passed the target it is clamped to stanceScale (FSTP idleScale at
     * 0x3001527e). The subtract path's overshoot test (TEST AH,0x5 / JNP 0x3001527e)
     * reuses that same clamp store. Net effect is a monotone one-step approach with
     * overshoot clamp; there is no multi-step loop. */
    float idleScale = state->idleScale;
    if (idleScale != stanceScale) {
        /* step = frameTime*0.5 stays in ST0 unrounded into the FADD/FSUBR at
         * 0x3001526f/0x30015283 (only the resulting idleScale is FST-stored),
         * so it is long double. */
        long double step = (long double)state->frameTime * 0.5f;
        if (stanceScale > idleScale) {
            long double nextIdleScale = (long double)idleScale + step;
            idleScale = (float)nextIdleScale; /* FST [state+0x18], x87 value retained */
            if (nextIdleScale > stanceScale) {
                idleScale = stanceScale; /* clamp: do not overshoot upward */
            }
        } else {
            long double nextIdleScale = (long double)idleScale - step;
            idleScale = (float)nextIdleScale; /* FST [state+0x18], x87 value retained */
            if (nextIdleScale < stanceScale) {
                idleScale = stanceScale; /* clamp: do not overshoot downward */
            }
        }
        state->idleScale = idleScale;
    }

    /* 0x30015294..0x300152e2: amplitude = swayMag * idleScale, then add three de-tuned
     * sines of the client game time (state->time, an int ms count) into `angles`.
     * The three axes use distinct low frequencies and per-axis amplitude scales, so the
     * sway never repeats exactly. Each write is angles[i] += sin(time * freq) * amp * scale.
     * Order below matches the instruction stream exactly (roll, then yaw, then pitch). */
    /* amp = swayMag * idleScale is likewise never stored (FMUL at 0x30015294
     * leaves it in ST1 for the three FMUL ST1 sine scalings; the trailing
     * FSTP ST0 discards it) -- long double. */
    long double amp = swayMag * idleScale;
    angles[2] =
        (float)(__builtin_sinl((long double)state->time * (long double)0.0005f) * amp * (long double)0.04f + (long double)angles[2]);
    angles[1] =
        (float)(__builtin_sinl((long double)state->time * (long double)0.0007f) * amp * (long double)0.01f + (long double)angles[1]);
    angles[0] = (float)(__builtin_sinl((long double)state->time * (long double)0.001f) * amp * (long double)0.01f + (long double)angles[0]);
    /* 0x300152e0 FSTP ST0 discards the leftover `amp`; 0x300152e2 RET. */
#else
    playerState_t *ps = state->ps;
    const weaponInfo_t *weapon = BG_GetInfoForWeapon(ps->currentWeapon);
    float idleAmount;
    float targetScale;

    if (weapon->adsEnabled != 0) {
#if EMULATE_X87
        idleAmount = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->idleSwayHip),
            x87f_mul(x87f_sub(x87f_load_f32(weapon->idleSwayAds), x87f_load_f32(weapon->idleSwayHip)), x87f_load_f32(ps->adsFraction))));
#else
        idleAmount = weapon->idleSwayHip + (weapon->idleSwayAds - weapon->idleSwayHip) * ps->adsFraction;
#endif
    } else {
        idleAmount = weapon->idleSwayHip;
        if (idleAmount == 0.0f) {
            idleAmount = 80.0f;
        }
    }

    if ((ps->entityStateFlags & EF_PRONE) != 0) {
        targetScale = weapon->idleSwayProneScale;
    } else if ((ps->entityStateFlags & EF_CROUCHING) != 0) {
        targetScale = weapon->idleSwayCrouchScale;
    } else {
        targetScale = 1.0f;
    }

    if (state->idleScale != targetScale) {
        if (state->idleScale < targetScale) {
#if EMULATE_X87
            state->idleScale =
                x87f_store_f32(x87f_add(x87f_load_f32(state->idleScale), x87f_mul(x87f_load_f32(state->frameTime), x87f_load_f32(0.5f))));
#else
            state->idleScale += state->frameTime * 0.5f;
#endif
            if (targetScale < state->idleScale) {
                state->idleScale = targetScale;
            }
        } else {
#if EMULATE_X87
            state->idleScale =
                x87f_store_f32(x87f_sub(x87f_load_f32(state->idleScale), x87f_mul(x87f_load_f32(state->frameTime), x87f_load_f32(0.5f))));
#else
            state->idleScale -= state->frameTime * 0.5f;
#endif
            if (state->idleScale < targetScale) {
                state->idleScale = targetScale;
            }
        }
    }

#if EMULATE_X87
    idleAmount = x87f_store_f32(x87f_mul(x87f_load_f32(idleAmount), x87f_load_f32(state->idleScale)));
#define BG_LINUX_IDLE_SINE(freq) ((float)CoduoLibm_Sin(x87f_store_f64(x87f_mul(x87f_load_i32(state->time), x87f_load_f32(freq)))))
    angles[2] = x87f_store_f32(
        x87f_add(x87f_load_f32(angles[2]),
                 x87f_mul(x87f_mul(x87f_load_f32(idleAmount), x87f_load_f32(BG_LINUX_IDLE_SINE(0.0005f))), x87f_load_f32(0.04f))));
    angles[1] = x87f_store_f32(
        x87f_add(x87f_load_f32(angles[1]),
                 x87f_mul(x87f_mul(x87f_load_f32(idleAmount), x87f_load_f32(BG_LINUX_IDLE_SINE(0.0007f))), x87f_load_f32(0.01f))));
    angles[0] = x87f_store_f32(
        x87f_add(x87f_load_f32(angles[0]),
                 x87f_mul(x87f_mul(x87f_load_f32(idleAmount), x87f_load_f32(BG_LINUX_IDLE_SINE(0.001f))), x87f_load_f32(0.01f))));
#undef BG_LINUX_IDLE_SINE
#else
    idleAmount *= state->idleScale;
    angles[2] += idleAmount * (float)CoduoLibm_Sin((double)state->time * 0.0005f) * 0.04f;
    angles[1] += idleAmount * (float)CoduoLibm_Sin((double)state->time * 0.0007f) * 0.01f;
    angles[0] += idleAmount * (float)CoduoLibm_Sin((double)state->time * 0.001f) * 0.01f;
#endif
#endif
}
