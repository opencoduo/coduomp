#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3001c8e0..0x3001c97d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c8e0_3001c97d.mcode
//
// CG_DrawSides — draw the left and right vertical bars of a HUD rectangle border.
// Takes (x, y, width, height, size) in virtual 640x480 space, applies the
// CG_AdjustFrom640 transform (screenXScale on the x/width axis, screenYScale on the
// y/height axis) inline, and issues two hudSoftLine stretch-pics through
// trap_R_DrawStretchPic (cgame trap 73): the left bar at the scaled left edge and
// the right bar at the scaled right edge, each `size` wide and spanning the full
// scaled height. Texcoords are all zero (a degenerate sample of the solid
// hudSoftLine fill shader) and the current draw color must already be set by the
// caller (this function does not touch trap_R_SetColor). It is the sibling of
// CG_DrawTopBottom (0x3001c980); both are called by CG_DrawRect (0x3001ca20), whose
// reconstructed body already calls CG_DrawSides(x, y, width, height, size) — that
// caller fixes the argument order and the void/cdecl ABI.
//
// The .mcode's size-matched guess "PM_Weapon_SetFPSFireAnim" is REJECTED: this
// function draws two 2D stretch-pics scaled by cgs.screenXScale/screenYScale using
// the hudSoftLine shader and takes no player/weapon state — it is a HUD border
// drawer, not a playermove weapon-animation setter. Same-module PPC bank lists
// cgame_mp!CG_DrawSides, and the split into top/bottom vs. left/right bars matches
// stock Q3 CG_DrawRect.
//
// Machine-code proof (no frame pointer; args are ESP-relative on entry, tracked
// across the interleaved pushes). Let E be entry ESP: [E+4]=x, [E+8]=y, [E+0xC]=w,
// [E+0x10]=h, [E+0x14]=size. The x87/stack shuffle computes and forwards:
//   xs   = x * screenXScale   (FSTP float -> single, reloaded for both bars)
//   ys   = y * screenYScale   (single)
//   ws   = w * screenXScale   (kept in ST as the extended intermediate for the
//                              right edge; never itself a draw argument)
//   hs   = h * screenYScale   (single)
//   ss   = size * screenXScale
//   right = (xs + ws) - ss    (FADD ST0,ST1 then FSUB float ptr [ss slot])
// Call 1 (left bar):  trap_R_DrawStretchPic(xs,    ys, ss, hs, 0,0,0,0, hShader)
// Call 2 (right bar): trap_R_DrawStretchPic(right, ys, ss, hs, 0,0,0,0, hShader)
// hShader = cgs.media hudSoftLine shader handle ([0x3044b6ac], loaded once into EAX
// for call 1 and re-read into EDX for call 2). Each coordinate slot is a single-
// precision float bit pattern (FSTP float ptr / raw 0 dword); trap_R_DrawStretchPic
// takes them as opaque 32-bit words, so CG_FloatBits reproduces the exact
// forwarding. Both trap calls are caller-cleaned (ADD ESP,0x24 after each).

void CG_DrawSides(float x, float y, float width, float height, float size)
{
    /* 0x3001c8e0 loads X scale, then snapshots the first shader handle before
     * completing and storing the first product. */
    long double xScaleForX = (long double)cgs_screenXScale;
    qhandle_t firstShader = cgs_media_whiteShader;
    float xs = (float)(xScaleForX * (long double)x);
    float ys = (float)((long double)cgs_screenYScale * (long double)y);
    /* ws is the one scaled term the DLL never stores to a float slot: it is kept
     * in st(0) as the extended intermediate and consumed by (xs + ws) - ss at
     * 0x3001c956 (FADD ST0,ST1). A float local would round it where the DLL does
     * not, so ws is long double. xs/ys/hs/ss ARE spilled to float
     * (0x3001c8f2/0x3001c90b/0x3001c929/0x3001c93c) and stay float. */
    long double ws = (long double)cgs_screenXScale * (long double)width;
    float hs = (float)((long double)cgs_screenYScale * (long double)height);
    float ss = (float)((long double)cgs_screenXScale * (long double)size);

    /* left vertical bar */
    trap_R_DrawStretchPic(CG_FloatBits(xs), CG_FloatBits(ys), CG_FloatBits(ss), CG_FloatBits(hs), CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f), CG_FloatBits(0.0f), firstShader);

    /* right vertical bar, at the scaled right edge minus one bar width */
    {
        long double rightBase = (long double)xs;
        qhandle_t secondShader = cgs_media_whiteShader;
        float right = (float)((rightBase + ws) - (long double)ss);
        trap_R_DrawStretchPic(CG_FloatBits(right), CG_FloatBits(ys), CG_FloatBits(ss), CG_FloatBits(hs), CG_FloatBits(0.0f),
                              CG_FloatBits(0.0f), CG_FloatBits(0.0f), CG_FloatBits(0.0f), secondShader);
    }
}
