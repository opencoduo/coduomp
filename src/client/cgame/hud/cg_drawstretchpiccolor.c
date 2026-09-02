#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30032050..0x300320d8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032050_300320d8.mcode
//
// CG_DrawStretchPicColor — draw shader `hShader` as a 2D stretch-pic filling the
// virtual-640x480 rectangle `rect` (a float[4] = {x, y, width, height}) in the
// given `color`. It sets the 2D draw color from the caller's float[4] (r,g,b,a)
// via trap_R_SetColor (cgame trap 72), draws the shader as a stretch-pic scaled
// from virtual to real screen coordinates (screenXScale on the x/width axis,
// screenYScale on the y/height axis — the CG_AdjustFrom640 transform, inlined
// here) with the full-image texcoords (0,0,1,1), then resets the draw color to
// white with trap_R_SetColor(NULL).
//
// This is the rect-pointer + explicit-color sibling of CG_FillRect (0x3001c4e0,
// which reads the four coordinates from stack args and uses a fixed hudSoftLine
// shader) and of CG_DrawPic (0x3001caa0, which takes the four coordinates as
// stack args and does not touch trap_R_SetColor). The (0,0,1,1) full-image
// texcoords match CG_DrawPic (not CG_FillRect's zero-width (0,0,0,1) sample).
//
// Register-arg ABI (Ghidra __usercall): the rect pointer arrives in EAX; the
// two remaining arguments are on the stack. Proven by the sole caller at
// 0x30032639, one arm of a rect-render switch:
//   MOV EAX,[ESP+0x50] ; PUSH EAX           ; color pointer  -> [E+8]
//   MOV ECX,[ESP+0x54] ; PUSH ECX           ; shader handle  -> [E+4]
//   LEA EAX,[ESP+0x14]                       ; EAX = &local float[4] rect
//   CALL 0x30032050
//   ADD ESP,8                                ; caller cleans the two stack args
//
// Machine-code proof (E = entry ESP; after SUB ESP,0x10 the frame base is E-0x10):
//   FLD [screenXScale]; FMUL [EAX+0x0]; FSTP [ESP+0xc]  ; screenXScale*rect[0] (x)
//   FLD [screenYScale]; FMUL [EAX+0x4]; FSTP [ESP+0x8]  ; screenYScale*rect[1] (y)
//   FLD [screenXScale]; FMUL [EAX+0x8]; FSTP [ESP+0x4]  ; screenXScale*rect[2] (w)
//   FLD [screenYScale]; FMUL [EAX+0xc]                  ; screenYScale*rect[3] (h)
//   MOV EAX,[ESP+0x18]=[E+0x8]=color ; PUSH EAX ; PUSH 0x48
//   FSTP [ESP+0x8]                                      ; store h product
//   CALL [cgame_syscall]                                ; trap_R_SetColor(color) (0x48=72)
//   ... gather the four products + hShader ([E+0x4]) and issue:
//   PUSH hShader / PUSH 1.0f / PUSH 1.0f / PUSH 0 / PUSH 0 / PUSH h / PUSH w /
//   PUSH y / PUSH x
//   CALL 0x3003e0f0                     ; trap_R_DrawStretchPic(x,y,w,h,0,0,1,1,hShader)
//   ADD ESP,0x3c                        ; caller-cleaned stretch-pic args (cdecl)
//   MOV [ESP+0x8],0 ; MOV [ESP+0x4],0x48 ; JMP [cgame_syscall]
//                                       ; tail call: trap_R_SetColor(NULL) reusing
//                                       ; the caller's return address
//
// The nine stretch-pic slots (proven by the push order — last-pushed is arg1):
//   x=screenXScale*rect[0]  y=screenYScale*rect[1]  w=screenXScale*rect[2]
//   h=screenYScale*rect[3]  s1=0.0 t1=0.0 s2=1.0 t2=1.0  hShader
// Each coordinate slot is a single-precision float bit pattern (FSTP float ptr, or
// the raw 1.0f/0.0f dword); trap_R_DrawStretchPic takes them as opaque 32-bit
// words, so CG_FloatBits reproduces the exact forwarding. The FMUL order in the
// machine code is x, y, w, h; written here in the same order since each product is
// independent.
//
// The .mcode's size-matched guess "BG_GetHorizontalBobFactor" is rejected: that BG
// helper computes a bob factor and returns a float, whereas this function issues
// two set-color traps and a stretch-pic draw. Behavior/call-graph name is used.
// Alternative candidate: same-module PPC bank lists cgame_mp!Script_SetColor near
// this cluster; not adopted because the proven behavior is a colored rect draw,
// not a script field set. Exact CoD symbol unproven; role name used.

void CG_DrawStretchPicColor(const rectDef_t *rect, qhandle_t hShader,
                            const float *color)
{
    /* 0x30032053..0x30032089 computes and spills all four products before
     * trap_R_SetColor. Keep that callback boundary and the x/y/w/h store order
     * explicit; evaluating these expressions after the trap changes the retail
     * operation order. */
    const float realX = cgs_screenXScale * rect->x;
    const float realY = cgs_screenYScale * rect->y;
    const float realW = cgs_screenXScale * rect->w;
    const float realH = cgs_screenYScale * rect->h;

    trap_R_SetColor(color);

    trap_R_DrawStretchPic(CG_FloatBits(realX),
                          CG_FloatBits(realY),
                          CG_FloatBits(realW),
                          CG_FloatBits(realH),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(1.0f),
                          CG_FloatBits(1.0f),
                          hShader);

    trap_R_SetColor(NULL);
}
