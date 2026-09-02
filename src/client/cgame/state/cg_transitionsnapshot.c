// Source: uo_cgame_mp_x86.dll 0x3003ca30..0x3003cc0a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ca30_3003cc0a.mcode
//
// CG_TransitionSnapshot (0x3003ca30) — promote cg_nextSnap to become the current
// cg_snap and transition every entity/client/playerState it carries. This is the
// canonical Quake3/CoD CG_TransitionSnapshot, run once per snapshot step by the
// snapshot pump CG_ProcessSnapshots (0x3003d2d0), which refers to it as the no-arg
// void routine at 0x3003ca30.
//
// Name resolution: the .mcode pre-hint "script_method_scriptbuiltin_dodamage" is a
// pure size match (win 0x1da == a same-size game_mp_uo GSC builtin) with ZERO
// behavioral basis, from the WRONG DLL (the server script VM), and is REJECTED.
// The behavior — free the effect-pool slots of the outgoing snapshot's entities,
// retire departed clients' anim state + DObj, swap cg_snap = cg_nextSnap, then run
// CG_TransitionEntity's inlined body over the new snapshot's entities and finally
// the local-player transition (damage feedback / out-of-ammo / predicted events) —
// is exactly CG_TransitionSnapshot.
//
// Behaviour, proven instruction-by-instruction against the .mcode:
//
//   1. 3003ca34..ca6d: for each entity in the OUTGOING snapshot (cg_snap), clear the
//      cg_entities[entityNum].currentValid field at +0x1e8:
//        cg_entities[cg_snap->entities[i].number].currentValid = 0.
//      Loop over cg_snap->numEntities (+0x4510); entities[] at +0x4518 (stride 0xf4);
//      the entity index is entities[i].number (its leading dword), *0x288, plus
//      the cg_entities.currentValid field base 0x3048c8c8.
//
//   2. 3003ca6f..caf8: for each visible client in the OUTGOING snapshot
//      (cg_snap->numClients, +0x4514; clients[] at +0x13918, stride 0x5c), look up
//      bgs.clientinfo[client.clientNum] (base 0x305e1f34, stride 0x4d0):
//        - if its `active` flag (+0x4) is nonzero, the client is still present, so
//          just clear the flag and keep the state;
//        - otherwise the client has gone: wipe the entire 0x4d0-byte anim record
//          (preserving animTree at +0x4c4), release the client's DObj
//          registration via trap CG_SAFE_CLIENT_DOBJ_FREE(clientNum, 1), and clear its
//          parallel DObj-info key/handle (cg_dObjInfoKeys/Handles[clientNum]).
//
//   3. 3003cafa..cb00: cg_snap = cg_nextSnap (the incoming snapshot becomes current).
//
//   4. 3003cb06..cb3d: if the new snapshot's playerState first-person view mask is set
//      (ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK, tested as [snap+0x18] & 0xc0000),
//      transition the LOCAL player's own centity (cg_entities[cg_snap->ps.psClientNum]):
//      currentState = nextState (REP MOVSD 0x3d dwords = 0xf4 bytes), dobj = 1. This is
//      CG_TransitionEntity's inlined body (see FUN_3003c7c0_3003c97f.c).
//
//   5. 3003cb3d..cb99: for each entity in the NEW snapshot (cg_snap->numEntities),
//      transition cg_entities[entities[i].number] the same way (currentState = nextState;
//      dobj = 1) and fire its queued events via CG_CheckEvents (0x300238e0).
//
//   6. 3003cb99..cbf6: local player-state transition, gated on
//      (cg_demoPlayback || (ps.playerStateFlags & 0x40000) || cg_fireGateTurretA ||
//       cg_fireGateTurretB). When taken and the new player-state's damageEvent differs
//      from the old one with a nonzero damageCount, fire CG_DamageFeedback(damageYaw,
//      damagePitch, damageCount) (0x30034ac0). `ps` is the new snapshot's playerState
//      (cg_snap+0xc), `ops` the outgoing one (saved cg_snap+0xc).
//
//   7. 3003cbf6..cc01: CG_OutOfAmmoChange() (0x30034a00) then
//      CG_CheckPlayerstateEvents(ps=new ps, ops) (0x30034ec0). (Only ps is register-
//      passed here — CG_CheckPlayerstateEvents takes ps in a register and ops on the
//      stack; the inlined caller supplies the new snapshot's playerState.)
//
// Callees (all resolved in the shared header):
//   - cgame_syscall [0x30085e9c] trap CG_SAFE_CLIENT_DOBJ_FREE (0xa8): release a client's
//     DObj/model registration; args (handleKey=clientNum, flag=1), caller-cleaned.
//   - CG_CheckEvents (0x300238e0): fire an entity's queued events.
//   - CG_DamageFeedback (0x30034ac0): directional damage view-kick / blend.
//   - CG_OutOfAmmoChange (0x30034a00): low-ammo warning sound.
//   - CG_CheckPlayerstateEvents (0x30034ec0): replay client-predicted playerState events.
//
// The three transition gates: cg_demoPlayback (0x30459148), cg_fireGateTurretA
// (0x3045860c) and cg_fireGateTurretB (0x30458bac) are the same trio CG_EntityEvent
// (0x30022810) tests together; any of them nonzero forces the local-player transition.
//
// Return/ABI: no arguments, cdecl caller-cleaned (RET, no imm), void.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

#include <string.h>   /* memcpy models the REP MOVSD state-copy blocks */

/* The three transition flags (same values CG_EntityEvent uses, 0x30022810). Any one
 * nonzero forces the local player-state transition path even without the first-person
 * view bit set. */
#define cg_fireGateTurretA cg_nopredict_vmCvar.integer      /* 0x3045860c */
#define cg_fireGateTurretB g_synchronousClients_vmCvar.integer      /* 0x30458bac */

/* cg_entities[] view over the .data base at 0x3048c6e0 (stride 0x288 ==
 * sizeof(centity_t)); the established access used across the corpus. */

void CG_TransitionSnapshot(void)
{
    snapshot_t *oldSnap;
    snapshot_t *snap;
    playerState_t *ps;
    playerState_t *ops;
    int i;

    /* 1. Clear current state for every outgoing-snapshot entity. */
    for (i = 0; i < cg_snap->numEntities; i++) {
        int32_t entityNum = cg_snap->entities[i].number;
        cg_entities[entityNum].currentValid = 0;
    }

    /* 2. Retire departed clients. */
    for (i = 0; i < cg_snap->numClients; i++) {
        int32_t clientNum = cg_snap->clients[i].clientNum;
        clientInfo_t *anim = &bgs.clientinfo[clientNum];

        if (anim->moduleState.active != 0) {
            /* Still present: consume the flag, keep the state. */
            anim->moduleState.active = 0;
        } else {
            /* Gone: wipe the record but preserve its runtime animation tree. */
            XAnimTree *savedAnimTree = anim->animTree;
            memset(anim, 0, sizeof(clientInfo_t));
            anim->animTree = savedAnimTree;

            /* Release this client's DObj registration and clear its cached slot. */
            cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, clientNum, 1);
            cg_dObjInfoKeys[clientNum] = 0;
            cg_dObjInfoHandles[clientNum] = 0;
        }
    }

    /* 3. Promote the incoming snapshot. `oldSnap` keeps the outgoing pointer for the
     *    player-state diff at the end. */
    oldSnap = cg_snap;
    snap = cg_nextSnap;
    cg_snap = snap;

    /* 4. Local player's own entity, when this is a first-person view snapshot. */
    if ((snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0) {
        centity_t *cent = &cg_entities[snap->ps.psClientNum];
        memcpy(cent, &cent->nextState, sizeof(entityState_t));
        cent->currentValid = 1;
    }

    /* 5. Every entity in the new snapshot. */
    for (i = 0; i < snap->numEntities; i++) {
        centity_t *cent = &cg_entities[snap->entities[i].number];
        memcpy(cent, &cent->nextState, sizeof(entityState_t));
        cent->currentValid = 1;
        CG_CheckEvents(cent);
    }

    /* 6/7. Local player-state transition (damage feedback, ammo check, predicted
     *      events). Gated on the fire-gate flags / the half first-person bit. */
    ps = &snap->ps;
    ops = &oldSnap->ps;

    if (cg_demoPlayback != 0 || (snap->ps.playerStateFlags & PSF_FOLLOWING) != 0 || cg_fireGateTurretA != 0 || cg_fireGateTurretB != 0) {
        if (ps->damageEvent != ops->damageEvent && ps->damageCount != 0) {
            CG_DamageFeedback(ps->damageYaw, ps->damagePitch, ps->damageCount);
        }
        CG_CheckAmmo();
        CG_CheckPlayerstateEvents(ps, ops);
    }
}
