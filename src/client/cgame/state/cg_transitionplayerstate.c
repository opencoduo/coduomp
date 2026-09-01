// Source: uo_cgame_mp_x86.dll 0x30034fe0..0x30035024
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034fe0_30035024.mcode
//
// CG_TransitionPlayerState — per-frame reconciliation of the local player's new
// playerState `ps` against the previous frame's `ops`: fire the directional damage
// feedback whenever a new damage event has arrived, run the local-sound checks, and
// replay the client-predicted playerState events.
//
// Name resolution: the .mcode header's guess "Item_GetModelDef" is a pure size
// match (win size 0x44) with no behavioral basis and is REJECTED — this function
// contains no item or model logic. The behavior is the canonical Quake3/CoD
// CG_TransitionPlayerState, and the same-module cgame_mp.dll PPC symbol table lists
// CG_TransitionPlayerState immediately before CG_CheckPlayerstateEvents (PPC
// 0x41ac0 -> 0x41b60), exactly the call this function makes. The three callees the
// machine code proves are:
//   0x30034ac0 = CG_DamageFeedback(yaw, pitch, damage)  (PPC CG_DamageFeedback)
//   0x30034a00 = CG_CheckAmmo()                         (the local-sound check;
//                this build reduced CG_CheckLocalSounds to just the out-of-ammo path,
//                a void global-state helper — see functions/FUN_30034a00_30034abf.c)
//   0x30034ec0 = CG_CheckPlayerstateEvents(ps, ops)     (PPC CG_CheckPlayerstateEvents)
//
// Damage fields (playerState_t +0x10c..+0x118, newly named in client_recovered.h):
//   +0x10c damageEvent, +0x110 damageYaw, +0x114 damagePitch, +0x118 damageCount.
// The guard `ps->damageEvent != ops->damageEvent && ps->damageCount` is the classic
// "a new damage event arrived and it carried real magnitude" latch; the three
// CG_DamageFeedback arguments are pushed damageCount/damagePitch/damageYaw (reverse
// push order), i.e. the call is CG_DamageFeedback(damageYaw, damagePitch, damageCount).
//
// Register ABI (compiler-chosen for this small helper): `ps` arrives in EAX (moved
// straight into EBX at the top and used as the new-state pointer everywhere), and
// `ops` arrives in ESI (used only to read ops->damageEvent and then handed to
// CG_CheckPlayerstateEvents). Neither pointer is set up in the prologue, so both are
// register-passed. CG_OutOfAmmoChange takes no arguments (it reads cg_snap globally).
// Expressed below as a normal two-parameter C function; the EAX/ESI split is an ABI
// detail, not source-level behavior.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops)
{
    // 0x30034fe0 ECX = ops->damageEvent; 0x30034fe9 EAX = ps->damageEvent;
    // 0x30034fef CMP / JZ skip: only when the new damage event differs from the old.
    // 0x30034ff3 EAX = ps->damageCount; 0x30034ff9 TEST / JZ skip: and only when the
    // new state carries real damage magnitude.
    if (ps->damageEvent != ops->damageEvent && ps->damageCount != 0) {
        // 0x30034ffd ECX = ps->damagePitch; 0x30035003 EDX = ps->damageYaw;
        // 0x30035009..0x3003500c PUSH damageCount, damagePitch, damageYaw; CALL.
        CG_DamageFeedback(ps->damageYaw, ps->damagePitch, ps->damageCount);
    }

    // 0x30035014 CALL 0x30034a00 — the local-sound check (out-of-ammo warning).
    CG_CheckAmmo();

    // 0x30035019 PUSH ESI (ops); 0x3003501a CALL 0x30034ec0 with ps live in EBX.
    CG_CheckPlayerstateEvents(ps, ops);
}
