// Source: uo_cgame_mp_x86.dll 0x3003c7c0..0x3003c97f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c7c0_3003c97f.mcode
//
// CG_TransitionEntity — commit a client entity (cg_entities[] centity_t /
// centity_t) from its incoming snapshot state to its current render state and
// run the per-entityType transition work. This is the canonical Quake3/CoD cgame
// "transition" step: currentState = nextState, then evaluate the entity's motion
// trajectories at cg.time to produce the lerped render origin/angles, then reset the
// per-type render state (player anim reset, corpse anim-state copy + DObj re-parent).
//
// ABI: cent is the single cdecl stack argument (EBP = [ESP+0xc] after PUSH EBX/EBP/
// ESI/EDI). The caller (0x3003d020) memcpy's a fresh snapshot entityState into
// cent->nextState (cent+0xf4) immediately before calling, then does ADD ESP,4 —
// caller-cleaned single dword arg. No return value (RET).
//
// The .mcode size-guess name "CG_PlayerShadow" is REJECTED: it was assigned purely by
// matching the Win32 byte size (0x1c0) to a corpus symbol, with zero behavioural
// basis, and the real CG_PlayerShadow is a different function (0x30032c20). This body
// does no shadow projection — it copies snapshot->current state, evaluates two
// trajectories via BG_EvaluateTrajectory, and dispatches on nextState.eType calling
// CG_ResetPlayerEntity and a DObj-clone trap. Named CG_TransitionEntity from that
// proven behaviour (the classic id-Tech/CoD per-entity transition routine).
//
// Instruction-level proof of the shared prologue (0x3003c7c0..0x3003c828):
//   MOVSD.REP ECX=0x3d, EDI=EBP, ESI=EBP+0xf4   -> currentState = nextState (0xf4 bytes)
//   MOV [EBP+0x228]=0; [EBP+0x224]=0; [EBP+0x220]=0  -> smoothedWeaponAngles = {0,0,0}
//   MOV [EBP+0x1e8]=1                            -> dobj sentinel set to 1 ("present")
//   MOV [EBP+0x1f8]=0                            -> laserEffectStarted = 0
//   EAX=cg_time; ECX=&lerpOrigin(+0x208); EBX=&nextState.pos(+0x100)
//     CALL BG_EvaluateTrajectory                 -> lerpOrigin  = eval(nextState.pos, cg.time)
//   EAX=cg_time; ESI=&lerpAngles(+0x214); EBX=&nextState.apos(+0x124)
//     CALL BG_EvaluateTrajectory                 -> lerpAngles  = eval(nextState.apos, cg.time)
//   EAX = nextState.eType (+0xf8); CMP EAX,4; JA default; JMP [EAX*4 + table@0x3003c980]
//
// switch(nextState.eType) jump table @0x3003c980 (5 dwords, verified from .text):
//   0 -> 0x3003c83f (default-shaped: previousEvent = modelPreviousEvent = 0)
//   1 -> 0x3003c852 (ET_PLAYER: seed anim view angles, then CG_ResetPlayerEntity)
//   2 -> 0x3003c8cb (ET_PLAYER_CORPSE: copy anim-state row into the corpse slot)
//   3 -> 0x3003c968 (default-shaped: previousEvent = modelPreviousEvent = nextState+0x19c)
//   4 -> 0x3003c83f (shared with case 0)
//   default (eType > 4) -> 0x3003c968 (same as case 3)
//
// lerpOrigin (+0x208) / lerpAngles (+0x214) are this centity's interpolated
// render origin/angles here (the trajectory results); they alias the same physical
// vec3s CG_CalcEntityLerpPositions later overwrites, so the names are kept.

#include <string.h>   /* memcpy models the REP MOVSD block copies */

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_TransitionEntity(centity_t *cent /* [ESP+0xc] */)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((cent->nextState.eType == ET_PLAYER ||
         cent->nextState.eType == ET_PLAYER_CORPSE) &&
        (uint32_t)cent->nextState.clientNum >=
            (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_TransitionEntity: invalid client number %i",
                  cent->nextState.clientNum);
        return;
    }

    /* currentState = nextState. The REP MOVSD copies the full 0xf4-byte state record
     * from cent+0xf4 (nextState) down over cent+0x0 (currentState). The currentState
     * region is the centity_t header fields (entityNum..hudTagMask); model it as
     * the same 0xf4-byte snapshot-state shape for the copy. */
    memcpy(cent, &cent->nextState, sizeof(entityState_t));

    /* Reset the smoothed view-weapon angles and the per-entity latches. */
    cent->smoothedWeaponAngles[0] = 0.0f;
    cent->smoothedWeaponAngles[1] = 0.0f;
    cent->smoothedWeaponAngles[2] = 0.0f;
    cent->currentValid = 1;                 /* +0x1e8 seeded to the "present" sentinel at transition */
    cent->laserEffectStarted = 0;   /* +0x1f8 one-shot laser latch cleared */

    /* Evaluate this entity's motion trajectories at cg.time to produce the current
     * interpolated render origin and angles. */
    BG_EvaluateTrajectory(&cent->nextState.pos, (int32_t)cg_time,
                          cent->lerpOrigin);
    BG_EvaluateTrajectory(&cent->nextState.apos, (int32_t)cg_time,
                          cent->lerpAngles);

    switch (cent->nextState.eType) {
    case ET_PLAYER: {
        /* Seed previousEvent/modelPreviousEvent from the incoming event sequence. */
        cent->previousEvent = cent->nextState.eventSequence;
        cent->modelPreviousEvent = cent->nextState.eventSequence;

        /* Push the freshly-lerped view angles (lerpAngles) and the two lean
         * payloads into this client's clientInfo_t, which CG_ResetPlayerEntity
         * consumes (leanAmount +0x3e0, leanFraction +0x3e4, viewPitch/Yaw/Roll
         * +0x3e8/+0x3ec/+0x3f0). The machine code seeds viewPitch/viewRoll from the
         * angle result and then zeroes lerpAngles[0]/[2] again below. */
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        clientInfo_t *anim =
            &bgs.clientinfo[cent->currentState.clientNum];
        anim->leanAmount   = cent->nextState.leanAmount;   /* +0x3e0 <- nextState+0x6c */
        anim->leanFraction = cent->nextState.leanf;        /* +0x3e4 <- nextState+0xd8 */
        anim->viewPitch    = cent->lerpAngles[0];    /* +0x3e8 */
        anim->viewYaw      = cent->lerpAngles[1];    /* +0x3ec */
        anim->viewRoll     = cent->lerpAngles[2];    /* +0x3f0 */

        /* MOV [ESI],0 and MOV [EBP+0x21c],0: clear lerpAngles[0] and [2]. */
        cent->lerpAngles[0] = 0.0f;
        cent->lerpAngles[2] = 0.0f;

        /* Re-initialise the player DObj's per-bone tags / swing-angle state. The
         * cdecl arg is the centity itself: CG_ResetPlayerEntity reads +0x8 (flags) and
         * +0x94 (animStateIndex), which alias cent->currentState.eFlags / cent->currentState.clientNum. */
        CG_ResetPlayerEntity((entityState_t *)cent);
        break;
    }

    case ET_PLAYER_CORPSE: {
        /* Corpse client-info row, indexed by (currentState.number - MAX_CLIENTS)
         * (EBX = [EBP+0x0], the just-copied nextState.number). */
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        uint32_t corpseIndex =
            (uint32_t)cent->currentState.number -
            (uint32_t)PLAYER_CLONE_ENTITYNUM_BASE;
        if (corpseIndex >= (uint32_t)PLAYER_CLONE_COUNT) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_TransitionEntity: "
                      "invalid player clone entity %i",
                      cent->currentState.number);
            return;
        }
        clientInfo_t *corpse = &cg_corpseInfo[corpseIndex];

        /* The live player's 0x4d0-byte clientInfo_t row for this client. */
        /* Retail used the same unchecked wire clientNum for the live row copied
         * into this corpse. The common entry guard establishes the bound. */
        clientInfo_t *playerAnim =
            &bgs.clientinfo[cent->currentState.clientNum];

        /* animTree (+0x4c4) is preserved across every full-row copy below. */
        XAnimTree *corpseAnimTree = corpse->animTree;

        int32_t eventValue;
        if ((cent->currentState.eFlags & EF_TRANSITION_CLONE_DOBJ) != 0) {
            /* Clone path: copy the whole player anim-state row over the corpse slot,
             * restore the corpse's own animTree, then hand the freshly-copied
             * player tree and the old corpse tree to the clone trap. */
            memcpy(corpse, playerAnim, sizeof(clientInfo_t));
            corpse->animTree = corpseAnimTree;
            cgame_syscall(CG_XANIM_CLONE_ANIM_TREE,
                          (intptr_t)playerAnim->animTree,
                          (intptr_t)corpseAnimTree);
            /* 0x3003c923 xor eax,eax: the clone path zeroes the event fields -- it jmps
             * straight to the common store tail (0x3003c94d), skipping the eventSequence
             * load at 0x3003c947. */
            eventValue = 0;
        } else {
            if (corpse->modelName[0] == '\0' ||
                corpse->clientNum != playerAnim->clientNum) {
                /* Fresh path: (re)copy the player anim-state row over the corpse slot when
                 * the corpse has no live tag byte (+0x40 == 0) or its anim field diverges
                 * from the player's. animTree is preserved. */
                memcpy(corpse, playerAnim, sizeof(clientInfo_t));
                corpse->animTree = corpseAnimTree;
            }
            /* else: the corpse already mirrors the player row -> no copy. */
            /* 0x3003c947 mov eax,[ebp+0x19c]: both non-clone sub-cases use eventSequence. */
            eventValue = cent->nextState.eventSequence;
        }

        /* 0x3003c94f/0x3003c955: the common tail stores eventValue (0 on the clone path,
         * nextState.eventSequence otherwise) to previousEvent (+0x1f0) and
         * modelPreviousEvent (+0x1f4); then dobjNeedsUpdate (+0x404) = 1. A prior pass
         * assigned eventSequence UNCONDITIONALLY, so the clone path wrongly seeded the
         * event fields with eventSequence instead of 0. */
        cent->previousEvent = eventValue;
        cent->modelPreviousEvent = eventValue;
        corpse->dobjNeedsUpdate = 1;   /* +0x404 */
        break;
    }

    case ET_GENERAL:
    case ET_MISSILE:
        /* Jump-table slots 0 and 4 (0x3003c83f): clear the event-dispatch latches. */
        cent->previousEvent = 0;
        cent->modelPreviousEvent = 0;
        break;

    default:
        /* Jump-table slot 3 (ET_ITEM) and the unsigned-range default (eType > 4, the
         * CMP EAX,4 / JA at 0x3003c832) share the 0x3003c968 tail: seed both event
         * latches from nextState.eventSequence. */
        cent->previousEvent = cent->nextState.eventSequence;
        cent->modelPreviousEvent = cent->nextState.eventSequence;
        break;
    }
}
