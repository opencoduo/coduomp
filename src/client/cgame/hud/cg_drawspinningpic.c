#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002f910..0x3002f9c1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002f910_3002f9c1.mcode
//
// CG_DrawSpinningPic — HUD draw-command handler that draws one HUD-scaled 2D pic
// (`rect` = {x,y,w,h}, shader `hShader`) rotated by a time-animated spin angle and
// modulated by `color`. It is a member of the cgame HUD-command dispatcher family at
// 0x300320e0 (sole call site 0x3003249f); the sibling CG_DrawPlayerAmmoBackdrop
// (0x3002eb90) uses the same trap_R_SetColor / draw / trap_R_SetColor(NULL) bracket.
//
// Behavior:
//   1. Transform `rect` from virtual-640 layout coordinates into scaled screen space
//      using cg_hudCompassSize_vmCvar.value (0x3048c4a8, the HUD slide/scale factor) and fixed
//      anchor constants (-25, 25, 345, 160). The x/y anchors keep the element pinned
//      while w/h scale directly.
//   2. Advance and read the rotating element's spin angle (cg_hudSpinAngle,
//      0x3048b5cc) via CG_UpdateHudSpinAngle (0x3001d3a0), which derives it from
//      cg_time. The four scaled coordinates (h, w, y, x) are its cdecl float args;
//      its return value is discarded.
//   3. trap_R_SetColor(color) -> CG_DrawRotatedPic(x, y, w, h, spinAngle, hShader) ->
//      trap_R_SetColor(NULL) (tail-call resetting the 2D draw color to opaque white).
//
// ABI (proven from the sole caller 0x30032491): EAX = pointer to a local rect
// float[4]; two caller-cleaned cdecl stack args: hShader then color (push edx=hShader;
// push eax=color; lea eax,[rect]; call; add esp,8). Register-passed EAX pointer is
// written here as an ordinary parameter, per the codebase convention.
//
// The .mcode size-matched candidate "PlayerCmd_isOnGround" is REJECTED by behavior:
// this reads no usercmd/ground state and issues 2D-draw cgame traps (R_SetColor,
// rotated-pic draw). Size is not evidence. The exact original cgame symbol is
// unproven (no cgame syscall/symbol table recovered), so CG_DrawSpinningPic is a
// proven-role provisional name.

// HUD virtual-screen anchor constants for this element (from .rdata float table).
enum {
    HUD_SPIN_ANCHOR_UNUSED = 0
};
#define HUD_SPIN_X_PRE (-25.0f)   /* 0x3007be9c: subtracted from rect.x before scale */
#define HUD_SPIN_X_POST (25.0f)    /* 0x3007be98: subtracted after scale */
#define HUD_SPIN_Y_ANCHOR (345.0f)  /* 0x3007be94: rect.y pivot, restored after scale */
#define HUD_SPIN_Y_SLIDE (160.0f)  /* 0x3007be90: extra y offset scaled by (scale-1) */
#define HUD_SCALE_ONE (1.0f)    /* 0x3007bce0: 1.0 baseline for the (scale-1) term */

void CG_DrawSpinningPic(const rectDef_t *rect, qhandle_t hShader, const float *color)
{
    // 0x3002f913..0x3002f927  FLD [EAX]; FSUB -25; FMUL scale; FSUB 25; FSTP [ESP+0xc]
    float x = (float)((((long double)rect->x - (long double)HUD_SPIN_X_PRE) * (long double)cg_hudCompassSize_vmCvar.value) -
                      (long double)HUD_SPIN_X_POST);

    // 0x3002f92b..0x3002f954  ((rect.y-345)*scale + 345) - (scale-1)*160
    long double yAnchor = (((long double)rect->y - (long double)HUD_SPIN_Y_ANCHOR) * (long double)cg_hudCompassSize_vmCvar.value) +
                          (long double)HUD_SPIN_Y_ANCHOR;
    long double ySlide = ((long double)cg_hudCompassSize_vmCvar.value - (long double)HUD_SCALE_ONE) * (long double)HUD_SPIN_Y_SLIDE;
    float y = (float)(yAnchor - ySlide);

    // 0x3002f958..0x3002f961  FLD scale; FMUL [EAX+8]; FSTP [ESP+4]
    float w = cg_hudCompassSize_vmCvar.value * rect->w;

    // 0x3002f965..0x3002f96e  FLD scale; FMUL [EAX+0xc]; FSTP [ESP]
    float h = cg_hudCompassSize_vmCvar.value * rect->h;

    // 0x3002f971  CALL 0x3001d3a0 — advance the spinning HUD element's angle from
    // cg_time toward its target. The four scaled floats (h, w, y, x) sit in the
    // outgoing stack slots because the compiler reused this frame, but the callee
    // reads NONE of them (proven from its own .mcode): CG_UpdateHudSpinAngle takes
    // no arguments and its return value is ignored.
    CG_UpdateHudSpinAngle();

    // 0x3002f976..0x3002f97d  PUSH color; PUSH 0x48; CALL [cgame_syscall]:
    //   trap(72, color) -> set the 2D draw color modulation. Return value ignored.
    trap_R_SetColor(color);

    // 0x3002f983..0x3002fa3  read back the freshly-updated spin angle and draw:
    //   CG_DrawRotatedPic(x, y, w, h, cg_hudSpinAngle, hShader).
    // Push order (right-to-left): hShader, cg_hudSpinAngle, h, w, y, x.
    CG_DrawRotatedPic(x, y, w, h, cg_hudSpinAngle, hShader);

    // 0x3002f9ab..0x3002f9bb  MOV [ESP+8],0; MOV [ESP+4],0x48; JMP [cgame_syscall]:
    //   tail-call trap(72, NULL) -> reset the draw color to opaque white.
    trap_R_SetColor(NULL);
}
