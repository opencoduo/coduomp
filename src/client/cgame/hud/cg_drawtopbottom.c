#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3001c980..0x3001ca1d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c980_3001ca1d.mcode
//
// CG_DrawTopBottom — draw the top and bottom horizontal bars of a HUD rectangle
// border. The sibling of CG_DrawSides (0x3001c8e0), and like it called by
// CG_DrawRect (0x3001ca20). Takes (x, y, width, height, size) in virtual 640x480
// space, applies the CG_AdjustFrom640 transform inline (screenXScale on the
// x/width axis, screenYScale on the y/height axis), and issues two hudSoftLine
// stretch-pics through trap_R_DrawStretchPic (cgame trap 73): the top bar along
// the scaled top edge and the bottom bar along the scaled bottom edge. Each bar
// spans the full scaled width and is `size` tall (the thickness, being a vertical
// dimension, is scaled by screenYScale here — the mirror of CG_DrawSides, where
// the bar width is scaled by screenXScale). Texcoords are all zero (a degenerate
// sample of the solid hudSoftLine fill shader) and the current draw color must
// already be set by the caller (this function does not touch trap_R_SetColor).
//
// The .mcode's size-matched guess "G_VehicleClientThink" is REJECTED: this
// function draws two 2D stretch-pics scaled by cgs.screenXScale/screenYScale using
// the hudSoftLine shader and takes no entity/vehicle state — it is a HUD border
// drawer, not a server-side vehicle think. Same-module PPC bank lists
// cgame_mp!CG_DrawSides; the top/bottom vs. left/right split matches stock Q3
// CG_DrawRect, so the sibling of CG_DrawSides is CG_DrawTopBottom.
//
// Machine-code proof (no frame pointer; args are ESP-relative on entry, tracked
// across the interleaved pushes/FSTPs). Let E be entry ESP: [E+4]=x, [E+8]=y,
// [E+0xC]=width, [E+0x10]=height, [E+0x14]=size. The x87/stack shuffle computes:
//   xs   = x      * screenXScale   (FSTP float -> single)
//   ys   = y      * screenYScale   (single)
//   ws   = width  * screenXScale   (single)
//   hs   = height * screenYScale   (kept in ST0 across call 1 as the extended
//                                   intermediate; never itself a draw argument)
//   ss   = size   * screenYScale   (single; the bar thickness, Y-axis scaled)
//   bottom = (ys + hs) - ss        (FADD ST0,ST1 then FSUB float ptr [ss slot])
// Call 1 (top bar):    trap_R_DrawStretchPic(xs,     ys, ws, ss, 0,0,0,0, hShader)
// Call 2 (bottom bar): trap_R_DrawStretchPic(xs, bottom, ws, ss, 0,0,0,0, hShader)
// hShader = cgs.media hudSoftLine shader handle ([0x3044b6ac], loaded once into EAX
// for call 1 and re-read into EDX for call 2). screenXScale=[0x30447aa4],
// screenYScale=[0x30447aa8]. Each coordinate slot is a single-precision float bit
// pattern (FSTP float ptr / raw 0 dword); trap_R_DrawStretchPic takes them as
// opaque 32-bit words, so CG_FloatBits reproduces the exact forwarding. Both trap
// calls are caller-cleaned (ADD ESP,0x24 after each). void/cdecl (RET, no imm).

void CG_DrawTopBottom(float x, float y, float width, float height, float size)
{
    long double xScaleForX = (long double)cgs_screenXScale;
    qhandle_t firstShader = cgs_media_whiteShader;
    float xs = (float)(xScaleForX * (long double)x);
    float ys = (float)((long double)cgs_screenYScale * (long double)y);
    float ws = (float)((long double)cgs_screenXScale * (long double)width);
    /* hs stays 80-bit: the DLL keeps screenYScale*height in ST0 across draw call 1
     * and feeds it UNSTORED into (ys + hs) - ss (FMUL @0x3001c9cd; FADD ST0,ST1
     * @0x3001c9f6, never an FSTP DWORD). A float local would round it before that
     * add, which the DLL does not (Class 8/1). ys and ss ARE stored+reloaded floats
     * (FSTP @0x3001c9ab reloaded @0x3001c9ec; FSTP @0x3001c9db), so they stay float. */
    long double hs = (long double)cgs_screenYScale * (long double)height;
    float ss = (float)((long double)cgs_screenYScale * (long double)size);

    /* top horizontal bar */
    trap_R_DrawStretchPic(CG_FloatBits(xs), CG_FloatBits(ys), CG_FloatBits(ws), CG_FloatBits(ss), CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f), CG_FloatBits(0.0f), firstShader);

    /* bottom horizontal bar, at the scaled bottom edge minus one bar height */
    {
        long double bottomBase = (long double)ys;
        qhandle_t secondShader = cgs_media_whiteShader;
        float bottom = (float)((bottomBase + hs) - (long double)ss);
        trap_R_DrawStretchPic(CG_FloatBits(xs), CG_FloatBits(bottom), CG_FloatBits(ws), CG_FloatBits(ss), CG_FloatBits(0.0f),
                              CG_FloatBits(0.0f), CG_FloatBits(0.0f), CG_FloatBits(0.0f), secondShader);
    }
}
