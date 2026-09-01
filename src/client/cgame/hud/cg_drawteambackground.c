#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30017dd0..0x30017e84
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30017dd0_30017e84.mcode
//
// CG_DrawTeamBackground — draw a team-colored 2D background bar. Picks a solid team
// color (team 1 = red (1,0,0), team 2 = blue (0,0,1)) with the caller's alpha,
// sets the 2D draw color via trap_R_SetColor (cgame trap 72), draws the
// cgs.media hudColorBar shader as a full-image stretch-pic scaled from the virtual
// 640x480 UI space to real screen coordinates (the CG_AdjustFrom640 transform,
// inlined exactly as CG_DrawPic does), then resets the draw color to white
// (trap_R_SetColor(NULL)). For any team other than 1 or 2 the function does nothing.
//
// Name resolved by behavior + call graph: it registers no state and issues the
// HUD draw traps 72/73 around the "hudColorBar" shader handle at 0x3044b6c0
// (registered from the "hudColorBar" string 0x30078048 at 0x3002c4b4). The
// same-module (cgame_mp) PPC name bank lists CG_DrawTeamBackground, whose Quake3
// signature (x, y, w, h, alpha, team) matches this argument shape exactly. The
// .mcode's size-matched guess "CG_InitVote" is rejected: this is a 2D HUD draw with
// no vote state, named by behavior and call graph, not by its 0xb4 byte size.
//
// Calling convention (i386 __usercall): `team` arrives in EAX; x, y, w, h, alpha
// arrive on the caller's stack (slots +0x04..+0x14 above the return address). The
// callee is caller-cleaned (plain RET); modeled here in source-natural argument
// order with `team` first.
//
// Machine-code proof of the color select (local float[4] at [ESP..ESP+0xc]):
//   CMP EAX,1; JNZ ...                 ; team == 1 ?
//     MOV [ESP],    0x3f800000         ; color[0] (r) = 1.0f
//     MOV [ESP+0x8],0x0                ; color[2] (b) = 0.0f
//   else CMP EAX,2; JNZ exit           ; team == 2 ? (else draw nothing)
//     MOV [ESP],    0x0                ; color[0] (r) = 0.0f
//     MOV [ESP+0x8],0x3f800000         ; color[2] (b) = 1.0f
//   MOV [ESP+0xc(pre-push)],ECX        ; color[3] (a) = alpha (caller arg +0x14), raw dword
//   MOV [ESP+0xc(post-2-push)],0x0     ; color[1] (g) = 0.0f
//   PUSH &color; PUSH 0x48; CALL [cgame_syscall]   ; trap_R_SetColor(color)   (0x48=72)
//
// Machine-code proof of the stretch-pic frame (built by 5 PUSH + SUB ESP,0x10 + 4 FSTP):
//   FLD [0x30447aa8]; FMUL [x-arg+0x10] ; cgs.screenYScale * h  -> FSTP [ESP+0xc] (arg h)
//   MOV EAX,[0x3044b6c0]                ; hShader = cgs.media hudColorBar
//   PUSH EAX / PUSH 1.0f / PUSH 1.0f    ; hShader, t2=1, s2=1  (0x3f800000)
//   PUSH 0 / PUSH 0                     ; t1=0, s1=0
//   FLD [0x30447aa4]; FMUL [+0xc]       ; cgs.screenXScale * w  -> FSTP [ESP+0x8] (arg w)
//   FLD [0x30447aa8]; FMUL [+0x8]       ; cgs.screenYScale * y  -> FSTP [ESP+0x4] (arg y)
//   FLD [0x30447aa4]; FMUL [+0x4]       ; cgs.screenXScale * x  -> FSTP [ESP]     (arg x)
//   CALL 0x3003e0f0                     ; trap_R_DrawStretchPic(x,y,w,h,0,0,1,1,hShader)
//   PUSH 0; PUSH 0x48; CALL [cgame_syscall]        ; trap_R_SetColor(NULL)    (0x48=72)
//
// screenXScale (0x30447aa4) scales x and width; screenYScale (0x30447aa8) scales y
// and height. The eight coordinate/texcoord slots are single-precision float bit
// patterns forwarded to trap_R_DrawStretchPic as opaque 32-bit words, so
// CG_FloatBits reproduces the exact FSTP-to-dword forwarding. The FMUL evaluation
// order in the machine code is h, w, y, x; written here in argument order since each
// product is independent.

void CG_DrawTeamBackground(int32_t team, float x, float y, float width, float height,
                          float alpha)
{
    /* Local RGBA color: 0x3f800000 == 1.0f, 0x0 == 0.0f. */
    float color[4];

    if (team == 1) {
        /* Red team. */
        color[0] = 1.0f;
        color[2] = 0.0f;
    } else if (team == 2) {
        /* Blue team. */
        color[0] = 0.0f;
        color[2] = 1.0f;
    } else {
        /* No team background for any other value. */
        return;
    }
    color[3] = alpha; /* caller alpha, stored as the raw float dword */
    color[1] = 0.0f;

    trap_R_SetColor(color);

    /* Preserve the retail evaluation order: height, width, y, then x. Each
     * x87 product is rounded once by its FSTP float argument slot. */
    float scaledHeight = (float)(
        (long double)cgs_screenYScale * (long double)height);
    float scaledWidth = (float)(
        (long double)cgs_screenXScale * (long double)width);
    float scaledY = (float)(
        (long double)cgs_screenYScale * (long double)y);
    float scaledX = (float)(
        (long double)cgs_screenXScale * (long double)x);

    trap_R_DrawStretchPic(CG_FloatBits(scaledX),
                          CG_FloatBits(scaledY),
                          CG_FloatBits(scaledWidth),
                          CG_FloatBits(scaledHeight),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(1.0f),
                          CG_FloatBits(1.0f),
                          cgs_media_hudColorBar);

    trap_R_SetColor((const float *)0);
}
