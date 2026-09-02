#include "../client_recovered.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3001c860..0x3001c8da
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c860_3001c8da.mcode
//
// CG_DrawFilledBarStyled — thin wrapper that draws one styled filled progress bar
// via CG_FilledBar (0x3001c5d0). It fixes the bar's fill color to gray (0.5,0.5,0.5)
// at alpha 0.3 and its background/border color to white (1.0,1.0,1.0) at alpha 0.3,
// selects bar flags 0x18 (fill-color-from-ECX + skip-alpha-fade), passes no third
// color (EDX = NULL), and forwards the caller's rect (x, y, width, height) and
// completion fraction unchanged. The one observed caller (0x3001c120, a
// loading/download progress screen that computes fraction = done/total clamped to
// 1.0 and passes the fixed rect 200,468,240,10) makes this the loading progress
// bar; the exact original symbol is unresolved, so the name is provisional by role.
// The .mcode's size-matched "BG_GetSpeed" guess is rejected: this function draws a
// 2D UI bar, it does not compute a movement speed.
//
// Machine-code proof (args are ESP-relative on entry; no frame pointer). Let E be
// the entry ESP: [E+4]=x, [E+8]=y, [E+0xC]=width, [E+0x10]=height, [E+0x14]=frac.
//   SUB ESP,0x20                              ; reserve the two color[4] locals
//   MOV EAX,[E+0x14]=frac ; MOV EDX,[E+0x10]=height
//   PUSH EBX                                  ; save EBX (restored after the call)
//   ; --- build the 6 stack args for CG_FilledBar (pushed high->low) ---
//   PUSH frac                                 ; stack arg6 = frac
//   LEA ECX,[borderColor]; PUSH ECX           ; stack arg5 = &borderColor (white)
//   PUSH height ; PUSH width ; PUSH y ; PUSH x ; stack args 4,3,2,1
//   ; --- build the register args ---
//   MOV EBX,0x18                              ; flags = FILLCOLOR | NO_ALPHA_FADE
//   LEA ECX,[fillColor]                       ; ECX = &fillColor (gray)
//   XOR EDX,EDX                               ; EDX = NULL (no third color)
//   ; --- initialize the two color[4] locals in place ---
//   fillColor  = {0.5, 0.5, 0.5, 0.3}         ; [ESP+0x2c..0x38] = 0.5,0.5,0.5,0.3
//   borderColor= {1.0, 1.0, 1.0, 0.3}         ; [ESP+0x1c..0x28] = 1.0,1.0,1.0,0.3
//   CALL 0x3001c5d0                           ; CG_FilledBar(flags,fill,NULL,x,y,w,h,border,frac)
//   ADD ESP,0x18                              ; pop the 6 stack args (caller-cleaned)
//   POP EBX ; ADD ESP,0x20 ; RET              ; restore, drop locals, no return value
//
// The float bit patterns resolve to: 0x3f000000=0.5f, 0x3e99999a=0.3f,
// 0x3f800000=1.0f. Both colors carry alpha 0.3. CG_FilledBar receives flags,
// fillColor, and the (unused here) third color in EBX/ECX/EDX (register-arg ABI),
// and the rect/border-color/fraction on the stack.

void CG_DrawFilledBarStyled(float x, float y, float width, float height, float frac)
{
    // Fill color (passed to CG_FilledBar in ECX; selected by flag CG_FILLEDBAR_FILLCOLOR).
    vec4_t fillColor = {0.5f, 0.5f, 0.5f, 0.3f};
    // Background/border color (stack arg, copied first inside CG_FilledBar).
    vec4_t borderColor = {1.0f, 1.0f, 1.0f, 0.3f};

    CG_FilledBar(CG_FILLEDBAR_FILLCOLOR | CG_FILLEDBAR_NO_ALPHA_FADE, fillColor, NULL, x, y, width, height, borderColor, frac);
}
