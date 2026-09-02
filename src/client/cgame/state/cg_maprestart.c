// Source: uo_cgame_mp_x86.dll 0x30039500..0x300396e1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30039500_300396e1.mcode
//
// CG_MapRestart(qboolean restart) — reset the cgame client state for a map
// restart / re-init. The .mcode's mechanical pre-hint "CG_DrawSoundOverlay" is
// REJECTED: it is a broad corpus name attached by a size match (win 0x1e1 vs
// 0x1e0) with no supporting behavior, and this function draws nothing. The body
// is self-identifying: when the developer/debug-print flag is set it emits the
// literal "CG_MapRestart\n" (0x3007a324) via Com_PrintMessage, then tears down and
// re-initialises the cgame local-entity / mark-poly / weapon subsystems and forces
// a handful of cvars back to their restart defaults. The name CG_MapRestart is
// taken from that print string.
//
// The mechanical `owner=cg_drawsoundoverlay` label on several .data globals this
// function zeroes (0x3048ae50..0x3048ae70, 0x3048b5c4) is the SAME rejected
// pre-hint leaking into the globals exporter (first-touch = this function); those
// symbols are pure restart-reset targets, not sound-overlay state.
//
// ABI: one argument (qboolean, read at [ESP+0x3c] after the batched pushes ==
// caller's first arg; callers at 0x3003af7b push 1 and at 0x3003b12e push 0),
// no source-level return (the function tail-calls cgame_syscall(0xf8); see below).
// Only ESI/EDI/EBX are saved. cgame_syscall args are pushed right-to-left with the
// trap id last (lowest address), then the batch is caller-cleaned.

#include <string.h>

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/* rep-stosd count from the machine code: MOV ECX,0x18 at 0x300395a3 clears 24
 * dwords starting at 0x3048ae74 — i.e. the whole cg_damageDirIndicators[8] ring
 * (8 slots * 3 dwords = 24 dwords = 0x60 bytes). The array is typed in globals.h;
 * the raw 24-dword count is retained here only as a cross-check on the STOSD. */

void CG_MapRestart(qboolean restart)
{
    /* if (cg_showmiss_vmCvar.integer) Com_PrintMessage("CG_MapRestart\n");
     *   MOV EAX,[cg_showmiss_vmCvar.integer]; CMP EAX,0(ESI); JZ; PUSH str; CALL Com_PrintMessage; ADD ESP,4. */
    if (cg_showmiss_vmCvar.integer != 0) {
        Com_PrintMessage("CG_MapRestart\n");
    }

    /* if (cgs_localServer) { cgame_syscall(0xcc, 1); InitWeaponInfo(); }
     *   CMP [cgs_localServer],0; JZ; PUSH 1; PUSH 0xcc; CALL *cgame_syscall; ADD ESP,8;
     *   CALL 0x30010df0. */
    if (cgs_localServer != 0) {
        cgame_syscall(CG_FREE_WEAPON_INFO_MEMORY, 1);
        InitWeaponInfo();
    }

    /* Reset the center-print / weapon-select / fade-overlay timers and the
     * tracer-state pair to their idle values (ESI == 0 throughout; the -1 is the
     * explicit 0xffffffff immediate at 0x30039549). */
    cg_centerPrintTime = 0;                     /* MOV [0x3048a9ac],ESI */
    cg_weaponSelectTimeA = 0;          /* MOV [0x3048ae3c],ESI */
    cg_overlayFadeDuration = 0;                 /* MOV [0x3048ae10],ESI */
    cg_complaintClientNum = -1;                 /* MOV [0x3044caf4],-1 */
    cg_complaintEndTime = 0;                    /* MOV [0x3044caf8],ESI */

    /* Re-initialise the local-entity and mark-poly (decal) pools, then issue the
     * two post-init engine traps that always follow this pair in the reset paths. */
    CG_InitLocalEntities();                     /* CALL 0x3002a9e0 */
    CG_InitMarkPolys();                         /* CALL 0x3002e400 */
    cgame_syscall(CG_FX_FREE_SYSTEM);                 /* PUSH 0xed; CALL *cgame_syscall */

    /* Clear the vote-time / timeout-HUD-end-time state and re-arm the
     * add-scale-fade guard (0x304831bc <- 1) before the next batch of traps.
     * The two zeros go in via ESI; the whole [0xed .. 0xd7] push batch is cleaned
     * together (there is no ADD ESP between them — cgame_syscall is caller-cleaned
     * and the id-0xd7 site pushes ESI==0 as its single argument). */
    cg_voteTime = 0;                            /* MOV [0x30447ca0],ESI */
    cg_timeoutEndTime = 0;             /* MOV [0x30447fd0],ESI */
    cg_initialSnapshotPending = 1;        /* MOV [0x304831bc],1 */
    cgame_syscall(CG_MSS_STOP_SOUNDS, 0);              /* PUSH 0(ESI); PUSH 0xd7; CALL *cgame_syscall */
    /* 0x30039590: re-register config-string 3's sound alias immediately after
     * the engine reset and submit its nonnegative time delta. */
    CG_ConfigString3Modified();

    /* Reset the sound-overlay pre-hint block (all refs=1, touched only here) and
     * zero the ejected-brass buffer array. EAX is 0 for all of these (XOR EAX,EAX
     * at 0x30039595); ESI is also 0. Order matches the machine code: two scalar
     * zeros, then the 24-dword rep-stosd of the brass buffer, then the trailing
     * scalar zeros of the same restart block. */
    cg_shakeRestartLatch = 0;                   /* MOV [0x3048b5c4],ESI */
    cg_damageFlashEndTime = 0;                  /* MOV [0x3048af0c],ESI */
    /* REP STOSD: EAX=0, ECX=0x18, EDI=0x3048ae74 -> zero the whole
     * cg_damageDirIndicators[8] ring (24 dwords == sizeof the array). */
    memset(cg_damageDirIndicators, 0, sizeof(cg_damageDirIndicators));
    memset(cg_weaponSelectSlotScale, 0, sizeof(cg_weaponSelectSlotScale));
    cg_weaponSelectLastTime = 0;                /* MOV [0x3048ae70],ESI */

    /* Force cg_thirdPerson=0 and cl_stance=0 (both value string "0" at 0x30076cc8)
     * via trap_Cvar_Set(name, value); the id-9 sites push (value, name, 9). */
    trap_Cvar_Set("cg_thirdPerson", "0");       /* PUSH "0"; PUSH "cg_thirdPerson"; PUSH 9 */
    trap_Cvar_Set("cl_stance", "0");            /* PUSH "0"; PUSH "cl_stance"; PUSH 9 */
    trap_Cvar_Set("cl_run", "1");               /* PUSH "1"(0x30077398); PUSH "cl_run"; PUSH 9 */

    /* Only on a full re-init (restart argument == 0) do we tear down any open
     * script menu: force ui_scriptMenuAllowResponse="0", close the script menu
     * twice (CG_CloseScriptMenu), restore ui_scriptMenuAllowResponse="1", then
     * fire trap 0x7e once. The other branch (restart != 0) skips straight to the
     * trailing trap 0x7e. Evidence: MOV EAX,[ESP+0x3c]; CMP EAX,ESI; JNZ 0x30039658. */
    if (restart == 0) {
        trap_Cvar_Set(ui_scriptMenuAllowResponseCvarName, "0"); /* value "0" @0x30076cc8 */
        CG_CloseScriptMenu();                   /* CALL 0x3003a950 */
        CG_CloseScriptMenu();                   /* CALL 0x3003a950 (issued twice) */
        trap_Cvar_Set(ui_scriptMenuAllowResponseCvarName, "1"); /* value "1" @0x30077398 */
        cgame_syscall(CG_MAP_RESTART_RESET_RENDERER);             /* PUSH 0x7e; CALL *cgame_syscall */
    }
    cgame_syscall(CG_MAP_RESTART_RESET_RENDERER);                 /* PUSH 0x7e; CALL *cgame_syscall (always) */

    /* If the scoreboard is up, clear it and latch cg_time as the fade start.
     *   EAX = cg_scoreboardShowing; if (EAX) { ECX = cg_time;
     *         cg_scoreboardShowing = 0; cg_scoreboardShowTime = ECX; } */
    if (cg_scoreboardShowing != 0) {
        int32_t now = coduo_int32_from_bits((uint32_t)cg_time);
        cg_scoreboardShowing = qfalse;          /* MOV [0x3048a554],ESI */
        cg_scoreboardShowTime = now;            /* MOV [0x3048a55c],ECX */
    }

    /* Clear the objective text: trap_Cvar_Set("cg_objectiveText", "") — value is
     * the empty string at 0x30074a0c. */
    trap_Cvar_Set("cg_objectiveText", "");      /* PUSH ""; PUSH "cg_objectiveText"; PUSH 9 */

    /* If the predicted player currently holds a weapon, re-seed its XAnim tree.
     *   EAX = cg_snap->ps.currentWeapon (+0xe4); if (EAX > 0 [signed, JLE]) {
     *       EBX = cgame_syscall(0xb5, cg_weaponInfos[EAX].viewDObjSelf);  // runtime tree
     *       EAX = cg_snap->ps.currentWeapon;                                 // reloaded
     *       CG_StopAllWeaponAnims(weaponIndex=EAX, animTree=EBX);  }
     * 0x30042a30 is CG_StopAllWeaponAnims (reconstructed body: IMUL 0x1c4 stride +
     * per-node trap-0x8f loop), NOT a sound refresh — it consumes the runtime
     * XAnim tree returned by trap 0xb5 in EBX. */
    if (cg_snap->ps.currentWeapon > 0) {
        int weapon = cg_snap->ps.currentWeapon;
        /* IMUL EAX,0x1c4; MOV EAX,[EAX+0x30413580] -> cg_weaponInfos[weapon].viewDObjSelf. */
        intptr_t animTree = cgame_syscall(CG_DOBJ_GET_TREE, (intptr_t)cg_weaponInfos[weapon].viewDObjSelf);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (animTree != 0) {
            CG_StopAllWeaponAnims(cg_snap->ps.currentWeapon, animTree);
        }
    }

    /* Tail call: the compiler unwinds the frame, writes the id 0xf8 into the
     * outgoing argument slot ([ESP+4]) and JMPs into *cgame_syscall so the trap's
     * return becomes this function's return. Modeled as the equivalent tail call. */
    cgame_syscall(CG_SYNC_TIMES); /* MOV [ESP+4],0xf8; JMP *cgame_syscall */
}
