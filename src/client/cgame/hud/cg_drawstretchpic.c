#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3001cb00..0x3001cb58
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001cb00_3001cb58.mcode
//
// CG_DrawStretchPic — draw shader hShader as a 2D stretch-pic in the virtual
// 640x480 UI coordinate space, using caller-supplied texture coordinates. This is
// the general sibling of CG_DrawPic (0x3001caa0): CG_DrawPic pins the texcoords to
// the full image (0,0)-(1,1); this routine forwards the four texcoords passed in.
// The position/size (x,y,w,h) are scaled from virtual to real screen coordinates
// (the CG_AdjustFrom640 transform, inlined here) and the whole 9-dword frame is
// forwarded to trap_R_DrawStretchPic (cgame trap 73).
//
// Naming: the `.mcode` header's size-matched hint `PM_GetSlowdownFriction` is
// rejected. This function performs no player-move math; it multiplies four
// coordinates by cgs.screenXScale (0x30447aa4) / cgs.screenYScale (0x30447aa8) and
// issues the 2D stretch-pic draw trap. Those two globals are the precomputed screen
// scales (written once in the cgs-init path at 0x3002e016/0x3002e028 as
// vidWidth*(1/640) and vidHeight*(1/480)); they are runtime .data, NOT the .rdata
// float constants — the multiplicands here are the scales themselves. Verified the
// sole caller (0x3002a2e5) pushes 8 stack dwords and loads hShader into EAX
// (`mov eax,ebx`) before the CALL, then cleans exactly 8 dwords (ADD ESP,0x20),
// proving the register-passed shader handle (non-cdecl / __usercall).
//
// Machine-code proof (all args are ESP-relative at entry ESP = E; no frame pointer;
// stack args a1..a8 at E+4..E+0x20, hShader in EAX):
//   MOV ECX,[E+0x20]                    ; ECX = a8 (t2)
//   FLD [screenYScale]; FMUL [E+0x10]   ; screenYScale * a4 (height)
//   MOV EDX,[E+0x1c]                    ; EDX = a7 (s2)
//   PUSH EAX                            ; -> callee arg9 slot = hShader (register)
//   MOV EAX,[E+0x18]                    ; EAX = a6 (t1)
//   PUSH ECX                            ; -> callee arg8 = a8 (t2), raw dword
//   MOV ECX,[E+0x18]... (reloaded a5)   ; ECX = a5 (s1)
//   PUSH EDX / PUSH EAX / PUSH ECX      ; -> callee arg7/6/5 = a7,a6,a5 raw dwords
//   SUB ESP,0x10
//   FSTP [ESP+0xc]                      ; callee arg4 (h) = screenYScale*a4
//   FLD [screenXScale]; FMUL [E+0x30(=a1)]; FSTP [ESP+0x8] ; arg1 (x) = screenXScale*a1
//   FLD [screenYScale]; FMUL [E+0x2c(=a2)]; FSTP [ESP+0x4] ; arg2 (y) = screenYScale*a2
//   FLD [screenXScale]; FMUL [E+0x28(=a3)]; FSTP [ESP]     ; arg3 (w) = screenXScale*a3
//   CALL 0x3003e0f0                     ; trap_R_DrawStretchPic(x,y,w,h,s1,t1,s2,t2,hShader)
//   ADD ESP,0x24; RET                   ; caller-cleaned stack; no return value
//
// The x/y/w/h products are single-precision (FSTP float ptr); s1..t2 are forwarded
// as raw 32-bit words (plain PUSH of the incoming dwords, no x87 touch). trap_R_
// DrawStretchPic takes all nine as opaque 32-bit words, so CG_FloatBits reproduces
// the exact forwarding. The multiply order in the machine code is h, x, y, w.

void CG_DrawStretchPic(float x, float y, float width, float height,
                       float s1, float t1, float s2, float t2,
                       qhandle_t hShader)
{
    float scaledHeight = (float)((long double)cgs_screenYScale *
                                 (long double)height);
    float scaledX = (float)((long double)cgs_screenXScale * (long double)x);
    float scaledY = (float)((long double)cgs_screenYScale * (long double)y);
    float scaledWidth = (float)((long double)cgs_screenXScale *
                                (long double)width);

    trap_R_DrawStretchPic(CG_FloatBits(scaledX),
                          CG_FloatBits(scaledY),
                          CG_FloatBits(scaledWidth),
                          CG_FloatBits(scaledHeight),
                          CG_FloatBits(s1),
                          CG_FloatBits(t1),
                          CG_FloatBits(s2),
                          CG_FloatBits(t2),
                          hShader);
}
