#include "../client_recovered.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30021a30..0x30021ba5
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021a30_30021ba5.mcode
//
// CG_AddCEntity_LoopedFx (eType 10 handler; EAX=cent, proven by the CG_AddCEntity
// dispatcher 0x30022170). The .mcode's size-matched "AngleVectors" guess is
// REJECTED: the body contains no sin/cos, no FSINCOS and no euler->basis math. It
// is the looped-effect emitter tail that shares cg_effectDefs, the 0xe7/0xe8 play
// traps and the "ERROR: CG_PlayFx called with invalid effect id %i\n" (0x30077150)
// diagnostic with CG_PlayFx (0x30022720) — this is the culling/cadence variant the
// CG_PlayFx reconstruction already names.
//
// Three phases:
//   1. Cadence (0x30021a36..0x30021a82): keep loopedFxNextTime marching with cg.time
//      at a fixed loopedFxInterval. If cg.time <= loopedFxNextTime (time reset / not
//      yet stamped) clamp loopedFxNextTime up to cg.time and emit this frame. Else if
//      less than one interval has elapsed, return without emitting; otherwise advance
//      loopedFxNextTime by whole intervals until it is within one interval of cg.time,
//      then emit this frame.
//   2. Distance cull (0x30021a82..0x30021afd): if loopedFxCullRadius (+0x68) != 0,
//      emit only when the effect origin (+0x208) lies within that radius of the view
//      origin cg_predictedPlayerState.psOrigin (squared compare dist2 < radius*radius). A radius
//      of 0.0f disables the cull.
//   3. Play (0x30021afd..end): range-check loopedFxId (+0xdc) in (0,80), look up
//      cg_effectDefs[id], and fire the play trap — oriented (0xe8, passing
//      &cent->currentState.effectEndOrigin as dir) when that emission-direction vector is nonzero,
//      otherwise origin-only (0xe7). Out-of-range id prints the shared error string.
//
// Register-argument ABI: cent in EAX; no stack args; returns void (every path is a
// bare RET that sets no return value).
//
// x87 compare idioms:
//   30021a95 FUCOMPP loopedFxCullRadius vs 0.0f; TEST AH,0x44; JNP -> radius==0 skips
//            the cull (fall straight to phase 3).
//   30021af0 FCOMPP  radius*radius vs dist2;      TEST AH,0x41; JNP -> return unless
//            radius*radius > dist2 (i.e. play only when dist2 < radius*radius).
//   30021b5b FUCOMPP 0.0f vs |dir|^2;             TEST AH,0x44; JNP -> |dir|^2==0 takes
//            the origin-only play path.

void CG_AddCEntity_LoopedFx(centity_t *cent)
{
    // Phase 1: advance the emission cadence toward cg.time.
    int32_t entryTime = coduo_int32_from_bits(cg_time);
    if (entryTime <= cent->loopedFxNextTime) {
        cent->loopedFxNextTime = entryTime;
    } else {
        int32_t interval = coduo_fp_to_i32_extended(cent->currentState.loopedFxInterval);

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (interval <= 0) {
            Com_Error(
                ERR_DROP,
                "\x15" "CG_AddCEntity_LoopedFx: invalid interval %i",
                interval);
            return;
        }

        int32_t elapsed = coduo_int32_from_bits(
            (uint32_t)entryTime - (uint32_t)cent->loopedFxNextTime);

        if (elapsed < interval) {
            return; // less than one interval elapsed: not time to emit yet
        }

        do {
            int32_t nextTime = coduo_int32_from_bits(
                (uint32_t)cent->loopedFxNextTime + (uint32_t)interval);
            cent->loopedFxNextTime = nextTime;
            /* 0x30021a6e reloads cg_time after every publication. */
            int32_t currentTime = coduo_int32_from_bits(cg_time);
            elapsed = coduo_int32_from_bits(
                (uint32_t)currentTime - (uint32_t)nextTime);
        } while (elapsed > interval);
    }

    // Phase 2: distance cull against the view origin (0 radius disables it).
    if (cent->currentState.loopedFxCullRadius != 0.0f) {
        float dx = cent->lerpOrigin[0] - cg_predictedPlayerState.psOrigin[0];
        float dy = cent->lerpOrigin[1] - cg_predictedPlayerState.psOrigin[1];
        float dz = cent->lerpOrigin[2] - cg_predictedPlayerState.psOrigin[2];
        /* long double: the sum is never stored — it rides st1 straight into the
         * FCOMPP against radius*radius (0x30021acc..0x30021af0). */
        long double dist2 =
            ((long double)dz * (long double)dz +
             (long double)dy * (long double)dy) +
            (long double)dx * (long double)dx;
        long double radius2 =
            (long double)cent->currentState.loopedFxCullRadius *
            (long double)cent->currentState.loopedFxCullRadius;

        /* FCOMPP / TEST AH,0x41 / JNP returns only for ordered <=; unordered
         * values continue to the play path. */
        if (radius2 <= dist2) {
            return; // effect origin is at/beyond the cull radius: skip
        }
    }

    // Phase 3: play the looped effect (shared with CG_PlayFx).
    int32_t fxId = cent->currentState.loopedFxId;

    if (fxId <= 0 || fxId >= 80) {
        Com_PrintMessage("ERROR: CG_PlayFx called with invalid effect id %i\n", fxId);
        return;
    }

    uint32_t handle = cg_effectDefs[fxId];

    // The per-entity emission direction reuses the effectEndOrigin vec3 (+0x5c).
    float dirX = cent->currentState.effectEndOrigin[0];
    float dirY = cent->currentState.effectEndOrigin[1];
    float dirZ = cent->currentState.effectEndOrigin[2];
    /* long double: never stored — compared against 0.0f straight in st
     * (0x30021b33..0x30021b5b FUCOMPP). */
    long double dirLen2 =
        ((long double)dirX * (long double)dirX +
         (long double)dirY * (long double)dirY) +
        (long double)dirZ * (long double)dirZ;

    if (dirLen2 != 0.0f) {
        cgame_syscall(CG_PLAY_EFFECT_ORIENTED,
                      coduo_int32_from_bits(handle),
                      (intptr_t)cent->lerpOrigin,
                      (intptr_t)cent->currentState.effectEndOrigin);
    } else {
        cgame_syscall(CG_PLAY_EFFECT_ORIGIN,
                      coduo_int32_from_bits(handle),
                      (intptr_t)cent->lerpOrigin);
    }
}
