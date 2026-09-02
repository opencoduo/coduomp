// Source: uo_cgame_mp_x86.dll 0x30036070..0x300361c7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30036070_300361c7.mcode
//
// CG_PredictPlayerState — public prediction wrapper and per-frame update of the
// aim-down-sight "view error" applied to the first-person view.
//
// The function does two things in sequence:
//
//  1. Unconditionally: run CG_PredictPlayerState_Internal
//     (0x30035800, return value discarded), then refresh the tracked view/camera
//     entity slot cg_entities[cgViewErrorEntityIndex]: copy the requested view
//     origin (cg_predictedPlayerState.psOrigin) into its lerpOrigin vec3 (+0x208) and
//     evaluate the slot's angle trajectory (apos, +0x30) at cg.time into its
//     lerpAngles vec3 (+0x214) via BG_EvaluateTrajectory. (The two vec3s are
//     the same fields CG_CalcEntityLerpPositions/CG_GetEntityOriginAxis use.)
//
//  2. Only while the local player is the active first-person player
//     (cg_snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) AND the ADS scope
//     overlay is active this frame (CG_CalcAdsOverlayFrac): step the ADS view-error
//     accumulators. The step fires exactly once per activation, latched by
//     cg_adsViewErrorLatched, and only when the current weapon defines a nonzero
//     error envelope (adsViewErrorMax != 0). Each step rolls a random magnitude in
//     [adsViewErrorMin, adsViewErrorMax], a random phase, and advances the x/y
//     accumulators by AngleMod(prev + trig(phase) * magnitude).
//
// NAMING: the .mcode mechanical size-guess "ScriptEnt_MoveAxis" is REJECTED — that
// is a server GSC script-entity axis mover; this is a pure cgame first-person view
// effect over cg_snap / weaponInfo / cg_entities with no script/entity-field
// handling. The same-module PPC bank identifies this wrapper as
// CG_PredictPlayerState; the inner 0x30035800 routine is its adjacent
// CG_PredictPlayerState_Internal.
//
// Register/stack facts proven against the .mcode:
//   * BG_EvaluateTrajectory uses the client register ABI result=ECX, tr=EBX,
//     atTime=EAX (LEA ECX,[cent+0x214]; LEA EBX,[cent+0x30]; MOV EAX,cg_time).
//   * cg_entities element stride is 0x288 (IMUL EAX,index,0x288; ADD EAX,base).
//   * The FUCOMPP at 0x300360f6 compares adsViewErrorMax against 0.0f and the
//     following JNP skips the step when they are equal (adsViewErrorMax == 0).
//   * Phase = (float)rand() * (1/32768) * 2*pi; the two .rdata constants are
//     [0x3007bec0] = 0x38000000 = 1/32768 (3.0517578e-05f; rand() spans
//     0..32767, so this maps it onto one full circle) and
//     [0x3007be44] = 0x40c90fdb = 2*pi (6.2831855f).
//   * FSINCOS leaves cosine in st(0) and sine in st(1): the code stores cos into
//     the +0x8 local and sin into the +0x4 local, then
//       cg_adsViewErrorAngles.x = AngleMod(cg_adsViewErrorAngles.x + sin*mag);
//       cg_adsViewErrorAngles.y = AngleMod(cg_adsViewErrorAngles.y + cos*mag);

#include "compat/coduo_native_x87.h"
#include "client/cgame/globals.h"          /* cg_snap, cg_time, cg_currentWeaponInfo,
                               * cg_predictedPlayerState.psOrigin, cg_adsViewErrorAngles,
                               * cg_adsViewErrorLatched, the cg_entities base */
#include "client/cgame/client_recovered.h" /* centity_t, weaponInfo_t, trajectory_t,
                               * BG_EvaluateTrajectory, CG_CalcAdsOverlayFrac,
                               * flrand, AngleMod, CG_PredictPlayerState_Internal */

/* cg_entities[] base: 0x3048c6e0, centity_t stride 0x288. */

/*
 * The cg_entities index this update refreshes is playerState.psClientNum
 * (0x30483298). Read as a plain array index (IMUL by the 0x288 stride).
 */

void CG_PredictPlayerState(void)
{
    centity_t *cent;
    float outFrac;      /* ESP+0x18 scratch handed to CG_CalcAdsOverlayFrac (ECX arg) */
    float magnitude;    /* flrand(adsViewErrorMin, adsViewErrorMax) */
    float phase;        /* rand() * (1/32768) * 2*pi */
    float sinPhase;
    float cosPhase;

    /* CALL 0x30035800 — per-frame update pass, run for side effects; its EAX
     * return is discarded (immediately overwritten by the index load below). */
    CG_PredictPlayerState_Internal();

    /* cent = &cg_entities[index]  (IMUL index,0x288 ; ADD base ; 0x30036079..0x30036090) */
    cent = cgame_compat_unchecked_cgentity(cg_predictedPlayerState.psClientNum);

    /* Copy the requested view origin into the slot's lerpOrigin vec3.
     * Store order in the machine code: +0x208 = x (0x304831d8), +0x210 = z
     * (0x304831e0), +0x20c = y (0x304831dc). */
    cent->lerpOrigin[0] = cg_predictedPlayerState.psOrigin[0];
    cent->lerpOrigin[2] = cg_predictedPlayerState.psOrigin[2];
    cent->lerpOrigin[1] = cg_predictedPlayerState.psOrigin[1];

    /* Evaluate the slot's angle trajectory at cg.time into lerpAngles.
     * Register ABI: result=&lerpAngles (ECX), tr=&cent->currentState.apos (EBX),
     * atTime=cg_time (EAX). */
    BG_EvaluateTrajectory(&cent->currentState.apos, coduo_int32_from_bits(cg_time), cent->lerpAngles);

    /* Only continue for the active local player's first-person view. */
    if ((cg_snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) {
        cg_adsViewErrorLatched = 0;   /* 0x300361b8: re-arm the one-shot step */
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (cg_currentWeaponInfo == NULL) {
        Com_Printf("WARNING: CG_PredictPlayerState: no registered current weapon\n");
        cg_adsViewErrorLatched = 0;
        return;
    }

    /* ...and only while the ADS scope overlay is active this frame. */
    if (!CG_CalcAdsOverlayFrac(&outFrac)) {
        cg_adsViewErrorLatched = 0;
        return;
    }

    /* The current weapon must define a nonzero view-error envelope, and the step
     * fires exactly once per activation (guarded by the latch). FUCOMPP at
     * 0x300360f6 skips when adsViewErrorMax == 0.0f; TEST/JNZ at 0x30036108 skips
     * when already latched. */
    if (cg_currentWeaponInfo->adsViewErrorMax == 0.0f) {
        return;
    }
    if (cg_adsViewErrorLatched != 0) {
        return;
    }
    cg_adsViewErrorLatched = 1;

    /* Random aim-error magnitude in [adsViewErrorMin, adsViewErrorMax].
     * Pushed as (adsViewErrorMax, adsViewErrorMin) -> flrand(min, max). */
    magnitude = flrand(cg_currentWeaponInfo->adsViewErrorMin, cg_currentWeaponInfo->adsViewErrorMax);

    /* Random phase over one full circle: rand() (0..32767) scaled by 1/32768,
     * then by 2*pi. (FMUL [0x3007bec0] = 0x38000000 = 3.0517578e-05f (1/32768);
     * FMUL [0x3007be44] = 0x40c90fdb = 6.2831855f.) */
    int32_t randomPhase = coduo_crt_rand();
    phase = (float)((long double)randomPhase * (long double)3.0517578e-05f * (long double)6.2831855f);

    /* FSINCOS: st(0)=cos(phase), st(1)=sin(phase). */
    coduo_x87_sincosf(phase, &sinPhase, &cosPhase);

    /* Advance the x/y accumulators, wrapping each back into [0,360) with AngleMod.
     * x uses sin, y uses cos (proven by the FSTP store order after FSINCOS). */
    float nextErrorX = (float)((long double)sinPhase * (long double)magnitude + (long double)cg_adsViewErrorAngles[0]);
    cg_adsViewErrorAngles[0] = AngleNormalize360(nextErrorX);
    float nextErrorY = (float)((long double)cosPhase * (long double)magnitude + (long double)cg_adsViewErrorAngles[1]);
    cg_adsViewErrorAngles[1] = AngleNormalize360(nextErrorY);
}
