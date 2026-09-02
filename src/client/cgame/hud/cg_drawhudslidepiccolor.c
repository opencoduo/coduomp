#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x300302d0..0x3003039c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300302d0_3003039c.mcode
//
// CG_DrawHudSlidePicColor — draw shader `hShader` as a 2D stretch-pic in `color`,
// with the rect `rect` (a float[4] = {x, y, width, height}) transformed by the HUD
// "slide" animation fraction cg_hudCompassSize_vmCvar.value (0x3048c4a8) before the usual
// virtual-640x480 -> real-screen scaling. It is a near-identical sibling of
// CG_DrawStretchPicColor (0x30032050): both set the 2D draw color from a float[4]
// (r,g,b,a) via trap_R_SetColor (cgame trap 72), draw the shader as a stretch-pic
// with full-image texcoords (0,0,1,1), and reset the draw color to nothing with
// trap_R_SetColor(NULL) via a tail call. The only difference is the coordinate
// transform: instead of a plain screenXScale/screenYScale scale, each of x/y/w/h is
// first blended by cg_hudCompassSize_vmCvar.value to produce a slide-in animation, then scaled.
//
// Register/stack ABI (Ghidra __usercall): the rect pointer arrives in EAX; the two
// remaining arguments are on the stack. Proven by the sole caller at 0x300324ae
// (one arm of a rect-render dispatch), which mirrors the CG_DrawStretchPicColor
// call shape at 0x30032639:
//   MOV ECX,[ESP+0x50] ; MOV EDX,[ESP+0x54]
//   PUSH ECX                                  ; color pointer -> [E+0x8]
//   PUSH EDX                                  ; shader handle -> [E+0x4]
//   LEA EAX,[ESP+0x14]                        ; EAX = &local float[4] rect
//   CALL 0x300302d0
//   ADD ESP,8                                 ; caller cleans the two stack args
//
// Machine-code proof (E = entry ESP; after SUB ESP,0x10 the frame base is E-0x10;
// slide = cg_hudCompassSize_vmCvar.value). Four products are stored into the local frame:
//   x  @E-0x4 : FLD [rect+0]; FSUB [-25.0]; FMUL [slide]; FSUB [25.0]; FMUL [Xscale]
//               = (((rect[0] - (-25.0)) * slide) - 25.0) * screenXScale
//   y  @E-0x8 : FLD [rect+4]; FSUB [345.0]; FMUL [slide]; FADD [345.0];
//               FLD [slide]; FSUB [1.0]; FMUL [160.0]; FSUBP; FMUL [Yscale]
//               = (((rect[1]-345.0)*slide + 345.0) - (slide-1.0)*160.0) * screenYScale
//   w  @E-0xc : FLD [slide]; FMUL [rect+8]; FMUL [Xscale]
//               = slide * rect[2] * screenXScale
//   h  @E-0x10: FLD [slide]; FMUL [rect+12]; FMUL [Yscale]
//               = slide * rect[3] * screenYScale
// The FSUBP (de e9) computes ST(1)-ST(0): the (rect[1]...) accumulator minus the
// (slide-1.0)*160.0 term, as written above.
//
// Around the h store the code sets the color: MOV EAX,[ESP+0x18]=[E+0x8]=color;
// PUSH EAX; PUSH 0x48; CALL [cgame_syscall] => trap_R_SetColor(color) (0x48=72).
// cgame_syscall is cdecl (its two args stay on the stack and are folded into the
// final ADD ESP,0x3c), so the subsequent [ESP+...] reads count them.
//
// Stretch-pic gather (push order; last-pushed is arg1). Reads: [E+0x4]=hShader,
// products x/y/w/h from the frame, then:
//   PUSH hShader / PUSH 1.0f / PUSH 1.0f / PUSH 0 / PUSH 0 / PUSH h / PUSH w /
//   PUSH y / PUSH x
//   CALL 0x3003e0f0    ; trap_R_DrawStretchPic(x,y,w,h,0,0,1,1,hShader)
//   ADD ESP,0x3c       ; cdecl cleanup of the syscall(2) + stretch-pic(9) words
//   MOV [ESP+0x8],0 ; MOV [ESP+0x4],0x48 ; JMP [cgame_syscall]
//                      ; tail call: trap_R_SetColor(NULL) reusing the return address
//
// Each coordinate slot is a single-precision float bit pattern (FSTP float ptr, or
// the raw 1.0f/0.0f dword); trap_R_DrawStretchPic takes them as opaque 32-bit words,
// so CG_FloatBits reproduces the exact forwarding.
//
// Naming: the .mcode's size-matched guess "BG_GetVerticalBobFactor" is rejected —
// that BG helper computes and returns a float bob factor, whereas this function sets
// a draw color, draws a stretch-pic, and resets the color (two trap-72 calls plus a
// trap-73 draw). Name resolved by behavior/call-graph as a HUD-slide colored-pic
// drawer, the cg_hudCompassSize_vmCvar.value-animated sibling of CG_DrawStretchPicColor. Exact
// CoD source symbol unproven (same-module PPC bank exposes no matching entry at this
// address); role name used with an uncertainty note.

void CG_DrawHudSlidePicColor(const rectDef_t *rect, qhandle_t hShader,
                             const float *color)
{
    /* Constants from .rdata (natural-form): -25.0 (0x3007be9c), 25.0 (0x3007be98),
     * 345.0 (0x3007be94), 160.0 (0x3007be90), 1.0 (0x3007bce0). */
    const float x = (float)(((((long double)rect->x - (-25.0L)) *
                              (long double)cg_hudCompassSize_vmCvar.value) -
                             25.0L) * (long double)cgs_screenXScale);
    const long double yAnchor =
        (((long double)rect->y - 345.0L) *
         (long double)cg_hudCompassSize_vmCvar.value) + 345.0L;
    const long double ySlide =
        ((long double)cg_hudCompassSize_vmCvar.value - 1.0L) * 160.0L;
    const float y = (float)((yAnchor - ySlide) *
                            (long double)cgs_screenYScale);
    const float w = (float)((long double)cg_hudCompassSize_vmCvar.value *
                            (long double)rect->w *
                            (long double)cgs_screenXScale);
    const float h = (float)((long double)cg_hudCompassSize_vmCvar.value *
                            (long double)rect->h *
                            (long double)cgs_screenYScale);

    trap_R_SetColor(color);

    trap_R_DrawStretchPic(CG_FloatBits(x),
                          CG_FloatBits(y),
                          CG_FloatBits(w),
                          CG_FloatBits(h),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(1.0f),
                          CG_FloatBits(1.0f),
                          hShader);

    trap_R_SetColor(NULL);
}
