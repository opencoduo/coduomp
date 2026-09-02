// Source: uo_cgame_mp_x86.dll 0x3003c750..0x3003c798
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c750_3003c798.mcode
//
// CG_UpdateShellShock — advance the whole cgame "shellshock" post-effect (the
// disorienting camera sway + full-screen blend flash + screen blur that follows a
// nearby explosion) for the current frame, or end it if its window has not started
// or has expired.
//
// Called once per frame from the scene reader CG_ShellShockScene (0x30042160, the
// leg at 0x300424e0) with the register ABI proven at the sole call site 0x30042541:
//   EAX = startTime  (cg.time captured when the shellshock began; either
//                     cg_shellShockStartTime for the manual "cg_shellshock" console
//                     command, or the active snapshot's shellshock start field)
//   EBX = params     (the active shellshock_t*: cg_consoleShellShock 0x30448624 for
//                     the console command, or &cg_shellShocks[index] for a
//                     config-string shellshock)
//   EDI = duration   (the shellshock duration in ms: cg_shellShockDuration, or the
//                     active snapshot's shellshock duration field)
// No stack arguments; the callee is __usercall over EAX/EBX/EDI. Expressed here as a
// normal 3-argument C function; the register binding is the ABI note above.
//
// Behavior, proven instruction by instruction:
//
//   elapsed = cg_time - startTime;          // ESI = [0x304831b0] - EAX (signed ms)
//   if (startTime == 0 || elapsed < 0)      // TEST EAX,EAX / JZ ; TEST ESI,ESI / JL
//       return CG_EndShellShock();          // tail JMP 0x3003c1d0 — not yet started
//                                           //   (startTime==0) or clock ran backwards
//   CG_UpdateShellShockSound(params, elapsed, duration);         // 0x3003c230
//   CG_UpdateShellShockMouse(params, elapsed, duration);         // 0x3003c530
//   CG_ShellShockCalcVibrate(duration, params, elapsed);         // 0x3003c630
//   cgame_syscall(CG_SET_SHELLSHOCK_SCREEN_BLUR, elapsed < duration);               // trap 0x57
//
// The final trap pushes a boolean: SETL AL after CMP ESI,EDI is the SIGNED
// (elapsed < duration) test, i.e. qtrue while the effect is still inside its active
// window, qfalse on the frame it expires. trap 0x57 is the same screen-state trap
// CG_EndShellShock (0x3003c1d0) uses to push the screen-blur amount; here it hands
// the engine the "shellshock still active" flag. The exact engine service name is
// unresolved (no cgame syscall-id table recovered), so it is named by proven trap id
// CG_SET_SHELLSHOCK_SCREEN_BLUR = 0x57.
//
// Argument order to the three sub-updates is proven from each callee's own prologue:
//   * Camera 0x3003c230: EAX=params (MOV ESI,EAX), stack arg0=elapsed, arg1=duration
//     (the caller does PUSH EDI; PUSH ESI => ESI is the lower/first stack slot).
//   * Mouse  0x3003c530: ECX=params, stack elapsed/duration.
//   * Vibrate 0x3003c630: EAX=duration, EDX=params, stack=elapsed.
// ESP is cleaned by ADD ESP,0x1c after the trap: 3 pushed pairs from the two-arg
// updates plus the one-arg blur (2+2+1 = 5 dwords cleaned across the sub-calls the
// caller cdecl-cleans here) and the trap's own 2 dwords => 0x1c total. Cdecl cleanup.
//
// Naming: the .mcode header's size-matched "Item_GetEditFieldDef" guess is REJECTED
// (no edit-field / item work — this fans out the three shellshock sub-updates and
// issues the shellshock screen-state trap). The two already-reconstructed sibling
// updates (CG_UpdateShellShockMouse 0x3003c530, CG_ShellShockCalcVibrate
// 0x3003c630) both name THIS function CG_UpdateShellShock, so that binding is adopted.
// NAMING CONFLICT: globals.c notes also call the *caller* leg 0x300424e0
// CG_UpdateShellShock; the two are distinct functions (0x300424e0 selects the active
// shellshock params/times, 0x3003c750 runs the per-frame effect). This function keeps
// the name the reconstructed siblings bound to its address; the 0x300424e0 note is
// the one that should be re-scoped (e.g. CG_ShellShockScene) when that caller is
// reconstructed.

#include "client/cgame/client_recovered.h"

void CG_UpdateShellShock(int32_t startTime, shellshock_t *params, int32_t duration)
{
    int32_t elapsed = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)startTime);

    /* Not started yet (no start time), or the client clock ran backwards past the
     * start: tear the effect down instead of advancing it. */
    if (startTime == 0 || elapsed < 0) {
        CG_EndShellShock();
        return;
    }

    CG_UpdateShellShockSound(params, elapsed, duration);
    CG_UpdateShellShockMouse(params, elapsed, duration);
    /* The blur update reads the same params base held in EBX at the call site. */
    CG_ShellShockCalcVibrate(duration, params, elapsed);

    /* qtrue while still inside the active window, qfalse the frame it expires. */
    cgame_syscall(CG_SET_SHELLSHOCK_SCREEN_BLUR, (elapsed < duration) ? qtrue : qfalse);
}
