// Source: uo_cgame_mp_x86.dll 0x30021bb0..0x30021d2a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021bb0_30021d2a.mcode
//
// CG_InterpolateEntityPosition (0x30021bb0) — the frame-interpolation
// ("smoothed") branch of CG_CalcEntityLerpPositions (0x30021d30). For an entity
// whose position trajectory is TR_INTERPOLATE (or TR_LINEAR_STOP below MAX_CLIENTS),
// the sibling tail-calls this routine instead of evaluating the trajectory once at
// cg_time. This routine evaluates the entity's trajectories on BOTH the current and
// the incoming snapshot and blends the two by cg_frameInterpolation, giving a smooth
// render position/angles between the two received network states.
//
// What it interpolates (proven from the body):
//   1. Position (lerpOrigin, +0x208, a vec3):
//        posA = BG_EvaluateTrajectory(currentState.pos, cg_snap->serverTime)
//        posB = BG_EvaluateTrajectory(nextState.pos,    cg_nextSnap->serverTime)
//        lerpOrigin = posA + cg_frameInterpolation * (posB - posA)   (plain lerp)
//   2. Angles (lerpAngles, +0x214, a vec3):
//        angA = BG_EvaluateTrajectory(currentState.apos, cg_snap->serverTime)
//        angB = BG_EvaluateTrajectory(nextState.apos,    cg_nextSnap->serverTime)
//        lerpAngles[i] = LerpAngle(angA[i], angB[i], cg_frameInterpolation)
//      (short-way angle interpolation, callee 0x3004bd00 = LerpAngle).
//
// Then, ONLY when nextState.eType (+0xf8) == ET_PLAYER(1), it publishes the derived
// lean/view scalars into this client's shared angle block
// (bgs.clientinfo[nextState.clientNum].leanAmount, a
// cgLerpAngleBlock_t):
//        block->leanAmount   = LerpAngle(currentState.leanValue,         nextState.leanValue,         frac)
//        block->viewPitch    = lerpAngles[0]
//        block->viewYaw      = lerpAngles[1]
//        block->viewRoll     = lerpAngles[2]
//        block->leanFraction = LerpAngle(currentState.leanf, nextState.leanf, frac)
// and then zeroes lerpAngles[0] and [2] (NOT [1]) on the centity — the same
// post-publish clear the non-smoothed sibling performs. This mirrors the sibling's
// ET_PLAYER block write, except the block index comes from nextState.clientNum
// (+0x188) and the two lean scalars are blended (current<->next) rather than copied.
//
// Callees (both already reconstructed):
//   0x30005f30 = BG_EvaluateTrajectory(const trajectory_t* /*EBX*/,
//                int32 atTime /*EAX*/, vec3 result /*ECX*/) — register ABI.
//   0x3004bd00 = LerpAngle(float from, float to, float fraction) — cdecl, float return.
//
// Fields read from cg_snap / cg_nextSnap: only serverTime (+0x08) of each — the
// evaluation times for the current and next snapshots. (0x30021bc3 MOV EAX,[cg_snap+8];
// 0x30021bd8 MOV EAX,[cg_nextSnap+8]; same pair again for the apos evaluation.)
//
// .rdata constants: NONE referenced directly by this function. All float constants
// (180/360, TWOPI, etc.) live inside the two callees, not in this body — verified by
// the absence of any RIP/absolute float load in the disassembly.
//
// NAMING: the .mcode size-guess "BG_CalculateView_Velocity" (win size 0x17a) is
// REJECTED — there is no velocity math here, and the routine is the client-side
// snapshot-blend branch. The Mac CG_InterpolateEntityPosition has the identical
// BG_EvaluateTrajectory and LerpAngle callset and performs the same current/next
// snapshot blend, resolving the source name.
//
// ABI (proven): ESI = centity (register argument), no stack arguments, plain RET.
// The frame reserves 0x1c bytes plus EBX/EDI saves for the two 3-float scratch
// buffers the trajectory evaluations write into.

#include <stddef.h>
#include <string.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* bgs.clientinfo[] base 0x305e1f34, stride 0x4d0; the shared cgLerpAngleBlock_t
 * begins at the typed leanAmount member (+0x3e0) of each element — same overlay the
 * non-smoothed sibling CG_CalcEntityLerpPositions writes. */

void CG_InterpolateEntityPosition(centity_t *entity /* ESI */)
{
    /* Same physical centity, currentState/nextState trajectory view. */
    centity_t *state = (centity_t *)entity;
    float frac = cg_frameInterpolation;

    /* 0x30021bc7..0x30021c2d: blend the position. Evaluate currentState.pos at the
     * current snapshot's serverTime and nextState.pos at the next snapshot's
     * serverTime into two scratch vec3s, then plain-lerp component-wise. */
    vec3_t posA;
    vec3_t posB;
    BG_EvaluateTrajectory(&state->currentState.pos, cg_snap->serverTime, posA);          /* 0x30021bce */
    BG_EvaluateTrajectory(&state->nextState.pos, cg_nextSnap->serverTime, posB);  /* 0x30021be5 */
    entity->lerpOrigin[0] = (float)(((long double)posB[0] - (long double)posA[0]) * (long double)frac + (long double)posA[0]); /* +0x208 */
    entity->lerpOrigin[1] = (float)(((long double)posB[1] - (long double)posA[1]) * (long double)frac + (long double)posA[1]); /* +0x20c */
    entity->lerpOrigin[2] = (float)(((long double)posB[2] - (long double)posA[2]) * (long double)frac + (long double)posA[2]); /* +0x210 */

    /* 0x30021c33..0x30021c98: blend the angles via short-way LerpAngle. Evaluate
     * currentState.apos and nextState.apos at the same two snapshot times, reusing
     * the same scratch buffers. */
    vec3_t angA;
    vec3_t angB;
    BG_EvaluateTrajectory(&state->currentState.apos, cg_snap->serverTime, angA); /* 0x30021c3c */
    BG_EvaluateTrajectory(&state->nextState.apos, cg_nextSnap->serverTime, angB); /* 0x30021c53 */
    entity->lerpAngles[0] = LerpAngle(angA[0], angB[0], frac); /* 0x30021c67 -> +0x214 */
    entity->lerpAngles[1] = LerpAngle(angA[1], angB[1], frac); /* 0x30021c7d -> +0x218 */
    entity->lerpAngles[2] = LerpAngle(angA[2], angB[2], frac); /* 0x30021c93 -> +0x21c */

    /* 0x30021c9e/0x30021ca7: only player entities publish the shared view-angle block.
     * The gate reads nextState.eType (+0xf8), the incoming-snapshot entity type. */
    if (state->nextState.eType == ET_PLAYER) {
        /* 0x30021cac..0x30021cc4: index bgs.clientinfo[nextState.clientNum]
         * (stride 0x4d0), then step to its cgLerpAngleBlock_t at +0x3e0. */
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        int32_t clientNum = state->nextState.clientNum;
        if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "CG_InterpolateEntityPosition: "
                      "invalid client number %i",
                      clientNum);
            return;
        }
        clientInfo_t *clientInfo = &bgs.clientinfo[clientNum];
        cgLerpAngleBlock_t *block = (cgLerpAngleBlock_t *)(void *)&clientInfo->leanAmount;

        /* 0x30021cca: leanAmount blends the current/next lean scalars short-way. */
        block->leanAmount = LerpAngle(state->currentState.leanValue, state->nextState.leanAmount, frac); /* +0x3e0 */
        /* 0x30021cd6..0x30021cf4: publish the just-computed axis angles. */
        memcpy(&block->viewPitch, entity->lerpAngles, sizeof(entity->lerpAngles));

        /* 0x30021d02/0x30021d08: zero lerpAngles[0] and [2] on the centity
         * (leaving [1] intact), matching the non-smoothed sibling's post-publish clear. */
        /* The current leanf dword is loaded before the two zero stores; the next
         * leanf dword is loaded after them. */
        float currentLeanFraction;
        memcpy(&currentLeanFraction, &state->currentState.leanf, sizeof(currentLeanFraction));
        entity->lerpAngles[0] = 0.0f;
        entity->lerpAngles[2] = 0.0f;
        float nextLeanFraction;
        memcpy(&nextLeanFraction, &state->nextState.leanf, sizeof(nextLeanFraction));

        /* 0x30021d16: leanFraction blends the current/next lean-fraction scalars. */
        block->leanFraction = LerpAngle(currentLeanFraction, nextLeanFraction, frac); /* +0x3e4 */
    }
}
