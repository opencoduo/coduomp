// Source: uo_cgame_mp_x86.dll 0x3003d2d0..0x3003d46b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003d2d0_3003d46b.mcode
//
// CG_ProcessSnapshots (0x3003d2d0) — the per-frame client snapshot pump.
//
// Naming: the mechanical .mcode pre-hint "script_func_playfxontag" is REJECTED. It
// came from a size-match (win 0x19b == a server GSC builtin of the same size) and is
// wrong: this is a cgame client routine, not a server GSC "play fx on tag" builtin.
// The identity is proven directly from the two Com_ErrorMessage format strings this
// function references — "CG_ProcessSnapshots: n < cg.latestSnapshotNum" (0x3007a454)
// and "CG_ProcessSnapshots: Server time went backwards" (0x3007a424) — which name it
// outright, and from its call graph (CG_ReadNextSnapshot, CG_SetNextSnap,
// CG_InstallSnapshotResetEffects, CG_SetFrameInterpolation) and its cg_snap /
// cg_nextSnap / cg.time state machine. This is the stock id-Tech/CoD CG_ProcessSnapshots.
//
// Behavior (all proven against the .mcode):
//   1. Ask the engine for the latest snapshot number and its server time via
//      cgame_syscall(CG_GET_CURRENT_SNAPSHOT_NUMBER, &n, &cg_latestSnapshotServerTime).
//      Mirror that server time into cg_effectAnimTime.
//   2. Advance cg_latestSnapshotNum toward n, complaining via Com_ErrorMessage if n
//      regressed below it.
//   3. If cg_snap is not yet set, spin CG_ReadNextSnapshot until one arrives, install
//      the first usable snapshot (skipping SNAPFLAG_NOT_ACTIVE ones), reset the client
//      time bases + effect pool, and prime the DObj/effect subsystem.
//   4. Once cg_snap exists, run the transition loop: pull the next snapshot, detect a
//      server restart via the SNAPFLAG_SERVERCOUNT toggle, complain about backwards
//      server time, and step cg_snap/cg_nextSnap forward until cg.time lies within the
//      [cg_snap.serverTime, cg_nextSnap.serverTime) window.
//   5. On the way out, if cg.time is still behind cg_snap.serverTime, clamp cg.time
//      (and cg_effectTime) forward to cg_snap.serverTime.
//
// Callee/return conventions proven at the call sites:
//   - cgame_syscall (0x30085e9c): variadic int trap; args pushed right-to-left, id
//     last, caller-cleaned.
//   - Com_ErrorMessage (0x3002b300): one format-string dword, caller-cleaned (ADD ESP,4).
//   - CG_ReadNextSnapshot (0x3003d220): no args, returns snapshot_t * in EAX.
//   - CG_SnapshotTransitionStage2 (0x30034d40), CG_SetNextSnap (0x3003cc10, one arg),
//     CG_InstallSnapshotResetEffects (0x3003c9d0, arg in EAX), CG_SetFrameInterpolation
//     (0x3001f710, no args), CG_ResetSnapshotEntityEffects (0x3003ca30, no args): all
//     return void.
//   - RET with no imm: cdecl, caller-cleaned. The SUB ESP,8 / ADD ESP,8 frame holds
//     the two scratch dwords `n` and the 1.0f trap argument.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_ProcessSnapshots(void)
{
    int32_t n;

    /* 3003d2d0..3003d2e6: cgame_syscall(CG_GET_CURRENT_SNAPSHOT_NUMBER, &n,
     * &cg_latestSnapshotServerTime). &n is a stack local (ESP+8); the second out is
     * the .data global. */
    cgame_syscall(CG_GET_CURRENT_SNAPSHOT_NUMBER, &n, &cg_latestSnapshotServerTime);

    /* 3003d2ea..3003d30a: advance cg_latestSnapshotNum toward n.
     *   n == latestSnapshotNum -> nothing;
     *   n  > latestSnapshotNum -> latestSnapshotNum = n (JGE skips the warning);
     *   n  < latestSnapshotNum -> Com_ErrorMessage, then reload n and set it. */
    if (n != cg_latestSnapshotNum) {
        if (n < cg_latestSnapshotNum) {
            Com_ErrorMessage("CG_ProcessSnapshots: n < cg.latestSnapshotNum");
            /* 3003d303: n is reloaded from the stack local before the store. */
        }
        cg_latestSnapshotNum = n;
    }

    /* 3003d316..3003d31c: mirror the latest snapshot's server time into the effect
     * animation clock, regardless of which branch below is taken. */
    cg_effectAnimTime = cg_latestSnapshotServerTime;

    /* 3003d30f..3003d322: if we have no current snapshot yet, run the initial-install
     * loop; otherwise jump straight to the transition loop below. */
    if (cg_snap == NULL) {
        /* 3003d328..3003d3a9: keep reading snapshots until one installs as cg_snap. */
        do {
            snapshot_t *snap = CG_ReadNextSnapshot();  /* 3003d328 */
            if (snap == NULL) {
                /* 3003d331: no snapshot available at all — done for this frame. */
                return;
            }

            /* 3003d337: an inactive snapshot cannot become the initial cg_snap;
             * fall through to the "still no cg_snap?" retest and loop again. */
            if ((snap->snapFlags & SNAPFLAG_NOT_ACTIVE) == 0) {
                /* 3003d33c: install this snapshot as the current one. */
                cg_snap = snap;

                /* 3003d342..3003d34f: seed the client time bases from its serverTime. */
                cg_time = (uint32_t)snap->serverTime;
                cg_effectTime = (uint32_t)snap->serverTime;
                cg_physicsTime = snap->serverTime;

                /* 3003d354..3003d370: clear cg_entities[i]+0x1e8. */
                for (int32_t i = 0; i < MAX_GENTITIES; i++) {
                    cg_entities[i].currentValid = 0;
                }

                /* 3003d372..3003d38c: restore full master sound volume
                 * immediately. The float is materialized on the stack and pushed
                 * as its raw bit pattern, matching the i386 code exactly. */
                cgame_syscall(CG_MSS_FADE_ALL_SOUNDS,
                              CG_FloatBits(1.0f), 0);

                /* 3003d38f..3003d39d: finish the transition for this first snapshot. */
                CG_SnapshotTransitionStage2();
                CG_SetNextSnap(snap);
                CG_TransitionSnapshot();
            }

            /* 3003d3a2..3003d3a9: retry until a snapshot actually became cg_snap. */
        } while (cg_snap == NULL);

    }

    /* 3003d322 jumps here when cg_snap was already installed, and the initial-
     * snapshot path falls through here after its install loop.  The original
     * therefore recomputes the fraction on every frame, before entering the
     * transition loop. */
    CG_SetFrameInterpolation();

    /* 3003d3c0..3003d443: transition loop. Step cg_snap/cg_nextSnap forward until
     * cg.time falls inside the current snapshot pair's time window. */
    for (;;) {
        /* 3003d3c0..3003d3cb: if next != current we already have a pending next
         * snapshot; skip straight to the window check. */
        if (cg_nextSnap == cg_snap) {
            /* 3003d3cd: pull a fresh next snapshot. */
            snapshot_t *snap = CG_ReadNextSnapshot();
            if (snap == NULL) {
                /* 3003d3d6 -> 3003d448: no newer snapshot; finalize with a clamp. */
                break;
            }

            /* 3003d3dd..3003d3e4: a toggle of SNAPFLAG_SERVERCOUNT between the current
             * snapshot and the new one marks a server restart. */
            if (((snap->snapFlags ^ cg_snap->snapFlags) & SNAPFLAG_SERVERCOUNT) != 0) {
                /* 3003d3e6..3003d3fb: full reset — reinstall from scratch and restart
                 * the loop. CG_InstallSnapshotResetEffects receives `snap` in EAX. */
                CG_InstallSnapshotResetEffects(snap);
                CG_SetNextSnap(snap);
                CG_TransitionSnapshot();
                continue;  /* 3003d3fb: JMP back to loop top */
            }

            /* 3003d3fd..3003d40f: same server session — guard against backwards time. */
            if (coduo_int32_from_bits((uint32_t)snap->serverTime -
                                 (uint32_t)cg_snap->serverTime) < 0) {
                Com_ErrorMessage("CG_ProcessSnapshots: Server time went backwards");
            }

            /* 3003d412: adopt the new snapshot as cg_nextSnap. */
            CG_SetNextSnap(snap);
        }

        /* 3003d41b..3003d43c: exit the loop once cg.time sits within the window,
         * i.e. cg.time >= cg_snap.serverTime AND cg.time < cg_nextSnap.serverTime.
         * The i386 forms both as signed subtractions and tests the sign bit (JS). */
        if (coduo_int32_from_bits(cg_time -
                             (uint32_t)cg_snap->serverTime) >= 0) {
            if (coduo_int32_from_bits(cg_time -
                                 (uint32_t)cg_nextSnap->serverTime) < 0) {
                /* 3003d43c: cg.time is before the next snapshot — window found. */
                break;
            }
        }

        /* 3003d43e..3003d443: cg.time is at/after cg_nextSnap.serverTime (or before
         * cg_snap.serverTime); reset entities and step to the next pair. */
        CG_TransitionSnapshot();
    }

    /* 3003d448..3003d466: final clamp. If cg.time is still behind cg_snap.serverTime,
     * snap cg.time (and cg_effectTime) forward to it so nothing runs before the
     * installed snapshot's time. Reached both from the break above (with EAX = cg.time)
     * and from the CG_ReadNextSnapshot==NULL exit (which loads cg.time at 0x3003d448). */
    {
        int32_t snapServerTime = cg_snap->serverTime;
        /* 3003d456..3003d458: JNS -> skip when cg.time >= cg_snap.serverTime. */
        if (coduo_int32_from_bits(cg_time - (uint32_t)snapServerTime) < 0) {
            cg_time = (uint32_t)snapServerTime;
            cg_effectTime = (uint32_t)snapServerTime;
        }
    }
    /* 3003d466..3003d46a: POP ESI; ADD ESP,8; RET. */
}
