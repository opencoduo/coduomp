// Source: uo_cgame_mp_x86.dll 0x30034d40..0x30034eb2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034d40_30034eb2.mcode
//
// CG_SnapshotTransitionStage2 (0x30034d40) — the second stage of the client's
// snapshot-transition, tail-JMP'd into from CG_InstallSnapshotResetEffects
// (0x3003c9d0) and also called directly from the initial-snapshot install path in
// CG_ProcessSnapshots (0x3003d0cd, guarded by the "not yet initialized" test on
// cg.mapRestart). It resets the locally-predicted player state to
// the freshly installed snapshot and clears the per-life transient client view /
// weapon / HUD / effect state, then re-seeds the weaponSelect / stance / run cvars.
//
// Body, in order:
//   1. Clears the "initial snapshot processed" latch (g_data_..._304831bc = 0) so a
//      later stage re-runs its one-shot init (0x30039580 sets it back to 1).
//   2. REP MOVSD copies 0x1141 dwords (0x4504 bytes) out of the newly installed
//      snapshot's embedded playerState (cg.snap + 0xc, i.e. &cg.snap->ps)
//      into cg.predictedPlayerState (base 0x304831c4). This is the whole predicted
//      playerState reset — the same 0x1141-dword playerState block CG_ProcessSnapshots
//      itself MOVSDs on the ordinary path.
//   3. Caches cg.currentWeaponInfo = bg_weaponInfos[cg.predictedPlayerState.currentWeapon]
//      and latches cg.weaponSelectTime = cg.time so the weapon-select HUD starts fresh.
//   4. Zeroes a large scatter of transient cgame globals (usable-hint, fade-overlay,
//      damage-flash/kick, view-kick vel+angles, ADS scratch, the special-tag placement
//      scratch, the 4-slot camera-shake table cg.shakeSources, the ADS-view-error latch,
//      several mechanical scratch dwords) and sets g_data_..._30489ef4 = 1.
//   5. Re-seeds three engine cvars via trap_Cvar_Set (cgame_syscall CG_CVAR_SET, id 9):
//        cg_weaponSelect = va("%i", cg.snap->ps.currentWeapon)   (the just-installed weapon)
//        cl_stance       = "0"
//        cl_run          = "1"
//
// Naming: the .mcode header carries the SIZE-GUESS name "CG_AddScaleFade" (matched
// only on byte size 0x172~0x174, zero behavioral basis) — REJECTED; this function
// scales/fades nothing. The address already has an established caller-observed name,
// CG_SnapshotTransitionStage2 (client_recovered.h, referenced by CG_ProcessSnapshots
// / CG_InstallSnapshotResetEffects reconstructions), which is retained. Many of the
// zeroed globals list this function as owner= only because it is their first toucher
// (it merely resets them here); their identities live at their own addresses.
//
// ABI: void(void). Callee-saved ESI/EDI are PUSH/POP'd around the body; the three
// trap calls and va() are all cdecl caller-cleaned (ADD ESP,0x2c at the end). No
// arguments, no return value. Plain RET.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "cg_predicted_events.h"

// Offsets this function proves against the machine code (i386, 4-byte words):
_Static_assert(offsetof(snapshot_t, ps.commandTime) == 0x0c,
               "predicted-state copy source = cg.snap + 0xc (embedded playerState)");
_Static_assert(offsetof(snapshot_t, ps.currentWeapon) == 0xe4,
               "cg.snap->ps.currentWeapon read at MOV EAX,[EDX+0xe4]");

void CG_SnapshotTransitionStage2(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: clear predicted event history across different lives or sessions. */
    coduomp_predicted_events_reset();

    // 0x30034d4a: clear the initial-snapshot-processed latch.
    cg.mapRestart = 0;

    // 0x30034d43..0x30034d5d: copy the installed snapshot's playerState into
    // cg.predictedPlayerState. Source is the embedded playerState (cg.snap + 0xc).
    memcpy(&cg.predictedPlayerState, &cg.snap->ps,
           sizeof(cg.predictedPlayerState));

    // 0x30034d5f..0x30034d79: cache the current predicted weapon's weaponInfo_t and
    // restart the weapon-select timer at cg.time. The pointer is loaded first, then
    // stock publishes the timestamp before the pointer.
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    weaponInfo_t *selectedWeapon =
        bg_weaponInfos[cg.predictedPlayerState.currentWeapon];
    int32_t weaponSelectTime = cg.time;
    cg.weaponSelectTime = weaponSelectTime;
    cg.currentWeaponInfo = selectedWeapon;

    // 0x30034d80..0x30034dec: zero the scatter of transient view/HUD/weapon/effect
    // globals (all stores of EDX==0 or EAX==0).
    cg.usableHintKind = CURSOR_HINT_OFF; // 0x3048ae08
    cg.overlayFadeStartTime = 0;        // 0x3048ae0c
    cg.screenReadyState = 0;  // 0x3048b114

    cg.weaponSwayOffset[2] = 0.0f;
    cg.weaponSwayOffset[1] = 0.0f;
    cg.weaponSwayOffset[0] = 0.0f;
    cg.weaponSwayAngles[2] = 0.0f;
    cg.weaponSwayAngles[1] = 0.0f;
    cg.weaponSwayAngles[0] = 0.0f;
    cg.weaponSwayViewAngles[2] = 0.0f;
    cg.weaponSwayViewAngles[1] = 0.0f;
    cg.weaponSwayViewAngles[0] = 0.0f;

    // 0x30034dc8..0x30034de6: cg.viewKickAngles (.z,.y,.x @6c,68,64) and
    // cg.viewKickVel (.z,.y,.x @60,5c,58) both cleared to the zero vector.
    cg.viewKickAngles[2] = 0.0f;
    cg.viewKickAngles[1] = 0.0f;
    cg.viewKickAngles[0] = 0.0f;
    cg.viewKickVel[2] = 0.0f;
    cg.viewKickVel[1] = 0.0f;
    cg.viewKickVel[0] = 0.0f;

    cg.xyspeed = 0;

    // 0x30034df2: one-shot vehicle-view reset latched to 1 (read/cleared by
    // CG_CalcVehicleViewValues at 0x30040675).
    cg.vehicleViewReset = 1;

    // 0x30034dfc..0x30034e06: STOSD zero-fill of the 12-dword block at 0x30487950.
    // The original monolithic cg_t storage makes these fields contiguous. They are
    // separate native globals during recovery, so clear the six proven fields rather
    // than relying on linker placement or writing beyond the first scalar object.
    cg.weaponPosition.prevAdsFraction = 0.0f;
    cg.weaponPosition.adsZoomingIn = qfalse;
    memset(cg.weaponPosition.weaponMovementAngles, 0, sizeof(cg.weaponPosition.weaponMovementAngles));
    cg.weaponPosition.weaponPositionMoveScale = 0.0f;
    memset(cg.weaponPosition.weaponMoveAngles, 0, sizeof(cg.weaponPosition.weaponMoveAngles));
    memset(cg.weaponPosition.weaponPositionPrevAngles, 0, sizeof(cg.weaponPosition.weaponPositionPrevAngles));

    cg.damageDirLatestServerTime = 0;          // 0x3048aed4
    cg.v_dmg_pitch = 0.0f;                 // 0x3048af10 (EAX==0 store)
    cg.v_dmg_roll = 0.0f;                     // 0x3048af14

    // 0x30034e18..0x30034e31: the cg.specialTagPlacement scratch orientation words at
    // 0x3048b0cc..0x3048b0e0 (EAX==0 stores).
    cg.weaponPositionBaseAngles[2] = 0.0f;
    cg.weaponPositionBaseAngles[1] = 0.0f;
    cg.weaponPositionBaseAngles[0] = 0.0f;
    cg.weaponRecoilAngles[2] = 0.0f;
    cg.weaponRecoilAngles[1] = 0.0f;
    cg.weaponRecoilAngles[0] = 0.0f;

    // 0x30034e36..0x30034e40: STOSD zero-fill of the 24-dword block at 0x3048ae74 —
    // the whole cg.damageDirIndicators[8] damage-direction arrow ring (8 * 3 dwords).
    memset(cg.damageDirIndicators, 0, sizeof(cg.damageDirIndicators));

    // 0x30034e42..0x30034e4c: STOSD zero-fill of the 4-slot camera-shake table
    // cg.shakeSources[4] (0x3048b52c, 4 * 9 dwords == 36 dwords).
    memset(cg.shakeSources, 0, sizeof(cg.shakeSources));

    cg.adsViewErrorLatched = 0;   // 0x3048c004

    /* 0x30034e54 snapshots cg.snap before the final three zero stores and
     * retains that pointer for the currentWeapon read below. */
    snapshot_t *snap = cg.snap;

    // 0x30034e5a..0x30034e64: three more scratch dwords cleared (EAX==0 stores).
    cg.predictedError[2] = 0.0f;
    cg.predictedError[1] = 0.0f;
    cg.predictedError[0] = 0.0f;

    // 0x30034e69..0x30034ea6: re-seed the three engine cvars via trap_Cvar_Set (id 9).
    // trap_Cvar_Set("cg_weaponSelect", va("%i", cg.snap->ps.currentWeapon))
    cgame_syscall(CG_CVAR_SET, (intptr_t)cg_weaponSelectCvarName,
                  (intptr_t)va("%i", snap->ps.currentWeapon));
    // trap_Cvar_Set("cl_stance", "0")
    cgame_syscall(CG_CVAR_SET, (intptr_t)cl_stanceCvarName,
                  (intptr_t)g_str_zero);
    // trap_Cvar_Set("cl_run", "1")
    cgame_syscall(CG_CVAR_SET, (intptr_t)cl_runCvarName,
                  (intptr_t)cvarEnabledValue);
}
