#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3001caa0..0x3001caf6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001caa0_3001caf6.mcode
//
// CG_DrawPic — draw shader hShader as a 2D stretch-pic in the virtual 640x480 UI
// coordinate space. The four position/size arguments are scaled from virtual to
// real screen coordinates (the CG_AdjustFrom640 transform, inlined here) and the
// texture coordinates are pinned to the full image (0,0)-(1,1) before the draw is
// forwarded to trap_R_DrawStretchPic (cgame trap 73).
//
// Machine-code proof (all args are ESP-relative; no frame pointer):
//   MOV EAX,[ESP+0x14]                 ; arg5 = hShader (kept as an int handle)
//   FLD [0x30447aa8]; FMUL [ESP+0x10]  ; cgs.screenYScale * arg4 (height)
//   PUSH EAX / PUSH 1.0f / PUSH 1.0f   ; hShader, s2=1, t2=1  (0x3f800000 = 1.0f)
//   PUSH 0 / PUSH 0                    ; s1=0, t1=0           (0x0        = 0.0f)
//   SUB ESP,0x10; FSTP [ESP+0xc]       ; store screenYScale*height
//   FLD [0x30447aa4]; FMUL [ESP+0x30]  ; cgs.screenXScale * arg1 (x)
//   FSTP [ESP+0x8]                     ; store screenXScale*x
//   FLD [0x30447aa8]; FMUL [ESP+0x2c]  ; cgs.screenYScale * arg2 (y)
//   FSTP [ESP+0x4]                     ; store screenYScale*y
//   FLD [0x30447aa4]; FMUL [ESP+0x28]  ; cgs.screenXScale * arg3 (width)
//   FSTP [ESP]                         ; store screenXScale*width
//   CALL 0x3003e0f0                    ; trap_R_DrawStretchPic(x,y,w,h,s1,t1,s2,t2,hShader)
//   ADD ESP,0x24; RET                  ; caller-cleaned args (cdecl); no return value
//
// The eight coordinate slots are single-precision float bit patterns (each stored
// with FSTP float ptr, or pushed as the raw 1.0f/0.0f dword); trap_R_DrawStretchPic
// takes them as opaque 32-bit words, so CG_FloatBits reproduces the exact forwarding.
// The multiply order in the machine code is height, x, y, width.

void CG_DrawPic(float x, float y, float width, float height, qhandle_t hShader)
{
    float scaledHeight = (float)((long double)cgs_screenYScale * (long double)height);
    float scaledX = (float)((long double)cgs_screenXScale * (long double)x);
    float scaledY = (float)((long double)cgs_screenYScale * (long double)y);
    float scaledWidth = (float)((long double)cgs_screenXScale * (long double)width);

    trap_R_DrawStretchPic(CG_FloatBits(scaledX), CG_FloatBits(scaledY), CG_FloatBits(scaledWidth), CG_FloatBits(scaledHeight),
                          CG_FloatBits(0.0f), CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), hShader);
}
