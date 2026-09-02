// Sources: uo_cgame_mp_x86.dll 0x300152f0..0x300154c2 and
// uo_game_mp_x86.dll 0x20015230..0x20015402. The bodies are instruction-
// identical after relocating their weapon table, cvar objects, and constants.
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300152f0_300154c2.mcode
//
// BG_CalculateWeaponPosition_BobOffset — the view-bob-driven contribution to the animated
// weapon-angle offset. One of the five sibling accumulators called (in order) by
// BG_CalculateWeaponAngles (0x30015920); it adds a pitch/yaw/roll
// bob wobble derived from the player's bobCycle phase and movement speed into the
// running angle vector `angles`, attenuated by how far the player is aiming down
// sights. The `.mcode` header name BG_GetTotalAmmoReserve is a pure size match
// (win size 0x1d2 == matched size 0x1d2) and is REJECTED: this function reads no
// ammo array, sums nothing, and returns void after writing a 3-float angle vector.
//
// Name: the provisional caller-observed decl BG_CalculateWeaponPosition_BobOffset_300152f0
// (client_recovered.h) is adopted and shortened here — its (vec3_t angles,
// pm_weapon_angle_state_t *state) shape is CONFIRMED by this body: `angles` arrives in EBX and
// `state` in EAX (proven at the sole caller 0x300159ce: MOV EBX,EDI / MOV EAX,ESI),
// no stack args, plain RET. The "_300152f0" address suffix is dropped now that the
// signature is proven from this function's own bytes; the family suffix "Sway" is
// provisional (exact original name unresolved).
//
// EBP/ESI/EDI save/restore and the SUB/ADD ESP frame are i386 calling-convention
// detail, not source-level behavior.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>

/*
 * The bob amplitude is clamped to at most 10.0 before use (FCOM against 10.0 @
 * 0x3007bda4; the product is kept when <= 10.0, replaced by 10.0 when strictly
 * greater). Modelled as min(x, 10.0).
 */
enum {
    BG_SWAY_MAX_BOB_AMP = 10
};

void BG_CalculateWeaponPosition_BobOffset(pm_weapon_angle_state_t *state, vec3_t angles)
{
#if defined(WINDOWS_BEHAVIOR)
    /* 0x300152f3: EDX = state->ps. */
    playerState_t *ps = state->ps;

    /* 0x300152f5..0x30015303: wi = bg_weaponInfos[ps->currentWeapon] (table @
     * 0x30134cd8). Only wi->adsBobFactor (+0x290) is read, in the ADS block. */
    int32_t currentWeapon = ps->currentWeapon;
    weaponInfo_t **weaponTable = bg_weaponInfos;
    weaponInfo_t *wi = weaponTable[currentWeapon];

    /* 0x30015306..0x3001532e: bob phase.
     *   bobByte = ps->bobCycle & 0xff        (FILD of the masked low byte)
     *   phase   = bobByte * (2*PI/255) + 2*PI + 7.0685835
     * Constants are the exact 32-bit .rdata floats: 0.024639944 (0x3007be48 =
     * 2*PI/255), 6.2831855 = 2*PI (0x3007be44), 7.0685835 (0x3007bf64). */
    int bobByte = ps->bobCycle & 0xff;
    float phase = (float)((long double)bobByte * (long double)0.024639944f + (long double)6.2831855f + (long double)7.0685835f);

    /* 0x30015332..0x3001533f: ampInput = state->speed * 0.16 (0x3007bf60). */
    float ampInput = state->speed * 0.16f;

    /* 0x30015347..0x3001534b: vertical bob factor. Args are pushed right-to-left
     * (phase, ampInput, 10.0) with ps handed to the callee in EAX (non-default
     * ABI). Result returns in ST0. */
    /* 0x30015350: the raw ST0 return is multiplied by -1.0 (0x3007bdb0) and
     * stays in the x87 stack unrounded through the ADS scaling until the
     * angles[0] accumulate store -- no float local in the DLL. */
    long double pitch = BG_GetVerticalBobFactor(ps, phase, ampInput, (float)BG_SWAY_MAX_BOB_AMP) * -1.0f;

    int32_t viewHeightTarget = ps->viewHeightTarget;
    int32_t proneViewHeight = ps->proneViewHeight;
    float yawStance;
    if (viewHeightTarget == proneViewHeight) {
        yawStance = BG_BOB_AMPLITUDE_PRONE;
    } else if (viewHeightTarget == ps->crouchViewHeight) {
        yawStance = BG_BOB_AMPLITUDE_DUCKED;
    } else {
        yawStance = BG_BOB_AMPLITUDE_STANDING;
    }

    /* 0x30015387..0x300153b4: yaw component.
     *   amp2 = min(stance * ampInput, 10.0)
     *   yaw  = -sin(phase) * amp2
     * (FSIN of phase, FMUL amp2, FMUL -1.0 @0x3007bdb0.) */
    long double amp2 = (long double)yawStance * (long double)ampInput;
    if (amp2 > (long double)BG_SWAY_MAX_BOB_AMP) {
        amp2 = (long double)BG_SWAY_MAX_BOB_AMP;
    }
    float yaw = (float)(__builtin_sinl((long double)phase) * amp2 * -1.0L);

    /* 0x300153b6..0x300153c8: roll phase, shifted by 0.47123894 (0x3007bf5c =
     * 0x3ef1463b; 0.4712389f would be 1 ULP low at 0x3ef1463a), and the 1.5x
     * (0x3007be70) amplitude used by the roll term. */
    float rollPhase = phase - 0.47123894f;
    long double rollAmpBase = (long double)ampInput * 1.5L;
    float rollTestStance;
    if (viewHeightTarget == proneViewHeight) {
        rollTestStance = BG_BOB_AMPLITUDE_PRONE;
    } else if (viewHeightTarget == ps->crouchViewHeight) {
        rollTestStance = BG_BOB_AMPLITUDE_DUCKED;
    } else {
        rollTestStance = BG_BOB_AMPLITUDE_STANDING;
    }
    long double rollAmp = (long double)rollTestStance * rollAmpBase;
    if (rollAmp > (long double)BG_SWAY_MAX_BOB_AMP) {
        rollAmp = (long double)BG_SWAY_MAX_BOB_AMP;
    }

    /* 0x30015405..0x3001545f: roll component. sin(rollPhase) is evaluated, then the
     * roll term is only applied on the half-cycle where sin(rollPhase)*rollAmp < 0
     * (FCOMP against 0.0 @0x3007bcec / TEST AH,0x5 / JP skips when the product is
     * >= 0). Otherwise the roll is 0.0. rollAmp is non-negative, so this selects
     * the negative half of sin(rollPhase). */
    long double sinRollWide = __builtin_sinl((long double)rollPhase);
    float sinRoll = (float)sinRollWide; /* 0x3001540b FST retained */
    long double roll;
    if (sinRollWide * rollAmp < 0.0L) {
        float rollApplyStance;
        if (viewHeightTarget == proneViewHeight) {
            rollApplyStance = BG_BOB_AMPLITUDE_PRONE;
        } else if (viewHeightTarget == ps->crouchViewHeight) {
            rollApplyStance = BG_BOB_AMPLITUDE_DUCKED;
        } else {
            rollApplyStance = BG_BOB_AMPLITUDE_STANDING;
        }
        long double rollApplyAmp = rollAmpBase * (long double)rollApplyStance;
        if (rollApplyAmp > (long double)BG_SWAY_MAX_BOB_AMP) {
            rollApplyAmp = (long double)BG_SWAY_MAX_BOB_AMP;
        }
        roll = rollApplyAmp * (long double)sinRoll;
    } else {
        roll = 0.0L;   /* FLD 0.0 (0x3007bcec) */
    }

    /* 0x30015465..0x300154a2: aim-down-sight attenuation. Applied only when
     * ps->adsFraction != 0.0 (FUCOMPP against 0.0 / TEST AH,0x44 / JNP skips when
     * equal). adsScale = 1 - adsFraction*(1 - wi->adsBobFactor); all three bob
     * components are scaled by it (constants 1.0 @0x3007bce0). */
    float adsFraction = ps->adsFraction;
    if (adsFraction != 0.0f) {
        long double adsScale = 1.0L - (long double)adsFraction * (1.0L - (long double)wi->adsBobFactor);
        pitch *= adsScale;
        yaw = (float)((long double)yaw * adsScale);
        roll *= adsScale;
    }

    /* 0x300154a9..0x300154bb: accumulate into the running angle vector. */
    angles[0] += pitch;
    angles[1] += yaw;
    angles[2] += roll;
#else
    playerState_t *ps = state->ps;
    const weaponInfo_t *weapon = BG_GetInfoForWeapon(ps->currentWeapon);
    float phase = BG_GetBobCycle(ps);
    float speed = state->speed * 0.16f;
    float pitch;
    float yaw;
    float roll;

    /* Linux loads these source constants as binary64 values before storing
     * the resulting phase to binary32. */
#if EMULATE_X87
    phase = x87f_store_f32(x87f_add(x87f_add(x87f_load_f32(phase), x87f_load_f64(0.7853981633974483)), x87f_load_f64(6.283185307179586)));
#else
    phase = (float)((long double)phase + 0.7853981633974483L + 6.283185307179586L);
#endif

    pitch = -BG_GetVerticalBobFactor(ps, phase, speed, (float)BG_SWAY_MAX_BOB_AMP);
    yaw = -BG_GetHorizontalBobFactor(ps, phase, speed, (float)BG_SWAY_MAX_BOB_AMP);
    roll = BG_GetHorizontalBobFactor(ps, phase - 0.47123889803846897, speed * 1.5f, (float)BG_SWAY_MAX_BOB_AMP);
    if (roll < 0.0f) {
        roll = BG_GetHorizontalBobFactor(ps, phase - 0.47123889803846897, speed * 1.5f, (float)BG_SWAY_MAX_BOB_AMP);
    } else {
        roll = 0.0f;
    }

    if (ps->adsFraction != 0.0f) {
        const float adsScale = 1.0f - (1.0f - weapon->adsBobFactor) * ps->adsFraction;

        pitch *= adsScale;
        yaw *= adsScale;
        roll *= adsScale;
    }

    angles[0] += pitch;
    angles[1] += yaw;
    angles[2] += roll;
#endif
}
