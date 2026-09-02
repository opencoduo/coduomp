#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3001c4e0..0x3001c54f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c4e0_3001c54f.mcode
//
// CG_FillRect — fill the virtual-640x480 rectangle (x, y, width, height) with a
// solid color. It sets the 2D draw color from the caller's float[4] (r,g,b,a) via
// trap_R_SetColor (cgame trap 72), draws the cgs.media hudSoftLine shader as a
// stretch-pic scaled from virtual to real screen coordinates (screenXScale on the
// x/width axis, screenYScale on the y/height axis — the CG_AdjustFrom640 transform,
// inlined here), then resets the draw color to white with trap_R_SetColor(NULL).
// Same-module PPC bank lists cgame_mp!CG_FillRect with this (x,y,w,h,color) shape;
// the .mcode's size-matched "Scr_SetObjectField" guess is rejected (this function
// draws, it does not set a script object field).
//
// Machine-code proof (all args are ESP-relative on entry; no frame pointer). Let E
// be the entry ESP, so [E+4]=x, [E+8]=y, [E+0xC]=width, [E+0x10]=height,
// [E+0x14]=color:
//   MOV EAX,[E+0x14]                       ; color pointer (5th arg)
//   PUSH EAX / PUSH 0x48 / CALL [cgame_syscall]   ; trap_R_SetColor(color)  (0x48=72)
//   FLD [screenYScale]; MOV ECX,[hudSoftLine]; FMUL [ESP+0x18]=height  ; screenYScale*height
//   ADD ESP,8                              ; pop the two trap args
//   PUSH ECX(shader) / PUSH 1.0f / PUSH 0 / PUSH 0 / PUSH 0
//   SUB ESP,0x10; FSTP [ESP+0xc]           ; store screenYScale*height into h slot
//   FLD [screenXScale]; FMUL [ESP+0x30]=width ; FSTP [ESP+0x8]  ; store screenXScale*width -> w slot
//   FLD [screenYScale]; FMUL [ESP+0x2c]=y     ; FSTP [ESP+0x4]  ; store screenYScale*y -> y slot
//   FLD [screenXScale]; FMUL [ESP+0x28]=x     ; FSTP [ESP]      ; store screenXScale*x -> x slot
//   CALL 0x3003e0f0                        ; trap_R_DrawStretchPic(x,y,w,h,s1,t1,s2,t2,hShader)
//   PUSH 0 / PUSH 0x48 / CALL [cgame_syscall]     ; trap_R_SetColor(NULL)
//   ADD ESP,0x2c; RET                      ; caller-cleaned args (cdecl); no return value
//
// The nine stretch-pic slots resolve (proven by the FSTP/PUSH displacements) to:
//   x  = screenXScale*x   y  = screenYScale*y   w  = screenXScale*width
//   h  = screenYScale*height
//   s1 = 0.0  t1 = 0.0  s2 = 0.0  t2 = 1.0      (the single 1.0f push lands in t2)
//   hShader = cgs.media hudSoftLine shader handle
// The (0,0,0,1) texcoords are a zero-width S sample of the hudSoftLine fill shader,
// distinct from CG_DrawPic's full (0,0,1,1). Each coordinate slot is a single-
// precision float bit pattern (FSTP float ptr, or the raw 1.0f/0.0f dword);
// trap_R_DrawStretchPic takes them as opaque 32-bit words, so CG_FloatBits
// reproduces the exact forwarding. The FMUL/FSTP order in the machine code is
// height, width, y, x and is retained below.

void CG_FillRect(float x, float y, float width, float height, const float *color)
{
    trap_R_SetColor(color);

    /* 0x3001c4ed..0x3001c539: load/evaluate/store in the exact height, width,
     * y, x order. The height scale is loaded before the shader handle and its
     * product remains in ST0 across that integer load. */
    long double scaledHeightRaw = (long double)cgs_screenYScale;
    qhandle_t shader = cgs_media_whiteShader;
    scaledHeightRaw *= (long double)height;
    float scaledHeight = (float)scaledHeightRaw;
    float scaledWidth = (float)((long double)cgs_screenXScale * (long double)width);
    float scaledY = (float)((long double)cgs_screenYScale * (long double)y);
    float scaledX = (float)((long double)cgs_screenXScale * (long double)x);

    trap_R_DrawStretchPic(CG_FloatBits(scaledX), CG_FloatBits(scaledY), CG_FloatBits(scaledWidth), CG_FloatBits(scaledHeight),
                          CG_FloatBits(0.0f), CG_FloatBits(0.0f), CG_FloatBits(0.0f), CG_FloatBits(1.0f), shader);

    trap_R_SetColor(NULL);
}
