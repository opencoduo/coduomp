// Source: uo_cgame_mp_x86.dll 0x3001bd20..0x3001bd44
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001bd20_3001bd44.mcode
//
// CG_DrawIntermission — the intermission-screen draw. Called by CG_Draw2D
// (0x3001bfe0) on the PM_TYPE_INTERMISSION player-move state. During intermission the
// game forces the multiplayer scoreboard on: it issues the zero-argument engine trap
// CG_MAP_RESTART_RESET_RENDERER (0x7e), latches the current client game time (cg_time) into
// cg_scoreboardShowTime, raises the cg_scoreboardShowing state flag, and then draws
// the scoreboard.
//
// Machine shape (all four items proven by the bytes):
//   3001bd20  PUSH 0x7e                      ; trap id CG_MAP_RESTART_RESET_RENDERER
//   3001bd22  CALL [0x30085e9c]              ; *cgame_syscall (zero-arg trap)
//   3001bd28  MOV  EAX,[0x304831b0]          ; EAX = cg_time  (read AFTER the trap)
//   3001bd2d  ADD  ESP,0x4                   ; caller-cleaned cdecl (one pushed dword)
//   3001bd30  MOV  [0x3048a55c],EAX          ; cg_scoreboardShowTime = cg_time
//   3001bd35  MOV  [0x3048a554],0x1          ; cg_scoreboardShowing  = qtrue
//   3001bd3f  JMP  0x30037d90                ; TAIL CALL CG_DrawScoreboard()
//
// The terminating instruction is a JMP (not CALL/RET), so this is a tail call: this
// function's return value IS whatever CG_DrawScoreboard() returns (qboolean in EAX).
// No frame is set up (no PUSH EBP / no locals); ESP is balanced by the ADD ESP,4
// before the jump, so CG_DrawScoreboard runs on this function's caller's return
// address and returns directly to it. Takes no arguments.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

qboolean CG_DrawIntermission(void)
{
    /* Force the scoreboard on for the intermission screen, timestamped at cg_time.
     * cg_time is read only after the trap returns (MOV EAX,[cg_time] follows the
     * CALL), matching the instruction order. */
    cgame_syscall(CG_MAP_RESTART_RESET_RENDERER);          /* PUSH 0x7e; CALL *cgame_syscall */
    cg_scoreboardShowTime = cg_time;     /* MOV [0x3048a55c],EAX (EAX = cg_time) */
    cg_scoreboardShowing = qtrue;        /* MOV dword [0x3048a554],1 */

    /* Tail call: JMP 0x30037d90 — the scoreboard draw becomes this function's
     * return, propagating CG_DrawScoreboard's qboolean result to the caller. */
    return CG_DrawScoreboard();
}
