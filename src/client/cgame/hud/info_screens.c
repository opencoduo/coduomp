// Source: uo_cgame_mp_x86.dll 0x3001b360..0x3001b38b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b360_3001b38b.mcode
//
// CG_DrawInfoScreens — per-frame developer info-overlay dispatcher.
//
// Naming: the mechanical pre-hint "Cmd_Fogswitch_f" is a pure size match
// (win size 0x2b == corpus size 0x2b) and is REJECTED: this is not a console
// command handler (no trap_Argc/trap_Argv, no command/format string, no cvar
// read of an argument), and it is not registered as a command. It is invoked
// unconditionally, once per frame, from the CG_Draw2D HUD chain at 0x3001bfe0
// (call site 0x3001c0b8, sitting between CG_DrawTimerHud and CG_DrawWeaponStance).
// An earlier provisional decl "CG_DrawScoresHud" is also superseded: this function
// draws nothing itself and reads no scores.
//
// Behavior (proven from the bytes): three cvar integer enable-flags are tested in
// a fixed priority order; the FIRST one that is nonzero selects a single developer
// info overlay, which is entered by an unconditional tail-jump (JMP, not CALL).
// If none is set, the function returns. Only one overlay is drawn per frame.
//
//   3001b360  a1 0c 6b 45 30   MOV EAX,[0x30456b0c]   ; flag A
//   3001b365  85 c0            TEST EAX,EAX
//   3001b367  74 05            JZ  0x3001b36e         ; A==0 -> test B
//   3001b369  e9 ..            JMP 0x3001acc0         ; A!=0 -> overlay A (tail)
//   3001b36e  a1 cc 71 45 30   MOV EAX,[0x304571cc]   ; flag B
//   3001b373  85 c0            TEST EAX,EAX
//   3001b375  74 05            JZ  0x3001b37c         ; B==0 -> test C
//   3001b377  e9 ..            JMP 0x30017e90         ; B!=0 -> overlay B (tail)
//   3001b37c  a1 2c 90 45 30   MOV EAX,[0x3045902c]   ; flag C
//   3001b381  85 c0            TEST EAX,EAX
//   3001b383  74 05            JZ  0x3001b38a         ; C==0 -> return
//   3001b385  e9 ..            JMP 0x3001b2b0         ; C!=0 -> overlay C (tail)
//   3001b38a  c3               RET
//
// The tail-jumps forward this function's (empty) frame directly to each target;
// each target is a zero-argument void draw routine, so this is a plain call-then-
// return in source form. The flags are read-only here (never written in .text);
// the cvar subsystem owns their storage. Their exact cvar names are unresolved,
// so their globals keep a role-based, address-suffixed name.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_DrawInfoScreens(void)
{
    /* Priority-ordered: first enabled overlay wins; only one is drawn. */
    if (cg_drawSoundOverlay_vmCvar.integer != 0) {
        CG_DrawSoundOverlay();                  /* JMP 0x3001acc0 */
        return;
    }
    if (cg_drawScriptUsage_vmCvar.integer != 0) {
        CG_DrawScriptUsage();              /* JMP 0x30017e90 */
        return;
    }
    if (cg_drawShader_vmCvar.integer != 0) {
        CG_DrawViewInfoOverlay();            /* JMP 0x3001b2b0 */
        return;
    }
    /* RET: no overlay enabled. */
}
