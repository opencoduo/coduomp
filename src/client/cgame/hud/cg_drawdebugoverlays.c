#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30018730..0x30018768
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018730_30018768.mcode
//
// CG_DrawDebugOverlays — the small debug-HUD dispatcher that draws the optional
// snapshot-timing line and the renderer-stats overlay, one above the other, in a
// shared right-aligned text column. Called with no arguments and its result
// discarded by the cgame 2D-draw dispatcher at 0x3001bfe0 (CALL 0x30018730 at
// 0x3001c0bd, among a long run of no-arg CG_Draw* overlay calls), so it is
// void(void).
//
// Behavior. A local vertical cursor `y` starts at the top of the column (0.0f).
// If cg_drawSnapshot_vmCvar.integer is enabled, CG_DrawSnapshot draws the "time/snap/cmd" line at
// `y` and returns the next line's Y, which is threaded into `y`. If
// cg_drawFPS_vmCvar.integer is enabled, CG_DrawFPS draws the fps/renderer-stats block
// starting at that same `y`. Its returned next-Y is discarded (nothing is drawn
// below), matching the FSTP ST0 at 0x30018761.
//
// Naming. The .mcode size-guess "PM_Weapon_ReloadDelayedAction" is REJECTED: this
// routine touches no weapon/rechamber state — it only reads two draw-enable gates
// and calls the two debug-HUD line drawers. The symbolized Mac cgame names the
// corresponding dispatcher CG_DrawDebugOverlays.
//
// Machine-code proof (ESP tracked from entry; entry has no stack args):
//   30018730 PUSH ECX                  reserve one 4-byte stack local `y` at [ESP].
//   30018731 MOV EAX,[0x30450b6c]      EAX = cg_drawSnapshot_vmCvar.integer.
//   30018736 TEST EAX,EAX
//   30018738 MOV dword [ESP],0x0       y = 0.0f  (0x00000000 == 0.0f as a float).
//   3001873f JZ 0x3001874f             if cg_drawSnapshot_vmCvar.integer == 0, skip the snap line.
//     30018741 PUSH 0x0                float arg 0.0f -> CG_DrawSnapshot(y).
//     30018743 CALL 0x30018020         st0 = CG_DrawSnapshot(0.0f).
//     30018748 FSTP float [ESP+0x4]    store returned next-Y back into `y` (the
//                                      local is now at ESP+4 after the PUSH).
//     3001874c ADD ESP,0x4             pop the float arg.
//   3001874f MOV EAX,[0x3044f18c]      EAX = cg_drawFPS_vmCvar.integer.
//   30018754 TEST EAX,EAX
//   30018756 JZ 0x30018766            if cg_drawFPS_vmCvar.integer == 0, skip the stats block.
//     30018758 MOV EAX,[ESP]           EAX = y (the current cursor).
//     3001875b PUSH EAX                float arg y -> CG_DrawFPS(y).
//     3001875c CALL 0x30018090         st0 = CG_DrawFPS(y).
//     30018761 FSTP ST0                discard the returned next-Y.
//     30018763 ADD ESP,0x4             pop the float arg.
//   30018766 POP ECX                   release the stack local.
//   30018767 RET
//
// Both drawers take/return a float `y` by value on the stack (see their .c files);
// modeled here as ordinary float pass/return. The entry PUSH 0x0 passes the same
// 0.0f the local was initialized to, so `y` is 0.0f on the CG_DrawSnapshot call.

void CG_DrawDebugOverlays(void)
{
    float y = 0.0f;

    if (cg_drawSnapshot_vmCvar.integer) {
        y = CG_DrawSnapshot(0.0f);
    }
    if (cg_drawFPS_vmCvar.integer) {
        CG_DrawFPS(y);
    }
}
