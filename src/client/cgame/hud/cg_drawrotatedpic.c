// Source: uo_cgame_mp_x86.dll 0x3001cb60..0x3001ccef
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001cb60_3001ccef.mcode
//
// CG_DrawRotatedPic (decl at client_recovered.h; the .mcode size-guess
// "BG_AnimationIndexForString" is REJECTED — there is no string, no index lookup,
// no anim-table walk. This is a pure x87 float rotation of a 2D quad followed by a
// single cgame 2D-poly draw syscall). The name CG_DrawRotatedPic (0x3001cb60) was
// already established in the corpus by its callers (e.g. CG_DrawSpinningPic
// 0x3002f910 feeds cg_hudSpinAngle as `angle`); this reconstruction proves the
// byte-level signature and body match that decl.
//
// Behavior (all proven against the .mcode instruction stream):
//   * Scale the four screen args by the cgs screen-scale globals, alternating X/Y:
//       X0 = cgs_screenXScale * x   ([0x30447aa4], [ESP+0x48])
//       Y1 = cgs_screenYScale * y   ([0x30447aa8], [ESP+0x4c])
//       X2 = cgs_screenXScale * w   ([0x30447aa4], [ESP+0x50])
//       Y3 = cgs_screenYScale * h   ([0x30447aa8], [ESP+0x54])
//     (this binary defines screenXScale = vidWidth/480, screenYScale = vidHeight/640,
//      swapped vs stock CoD/Q3 — see globals.h; this code just consumes them.)
//   * Convert `angle` (degrees, [ESP+0x58]) to radians as two separate single-
//     precision multiplies, matching the machine code's split of M_PI/180:
//       (angle * M_PI)  [FMUL 0x3007bd88 = 3.14159274f]
//       then * (1/180)  [FMUL 0x3007bed4 = 0.0055555557f].
//     Both FMULs chain in st(0); the ONLY rounding is FSTP DWORD [ESP+0xc] at
//     0x3001cbba, so this is a single rvalue with one round-to-float.
//   * FSINCOS the radians -> {sin, cos}.
//   * Center of the quad: cx = X0 + 0.5*X2 ; cy = Y1 + 0.5*Y3 (top-left + half-dim).
//     Half-extents: hw = 0.5*X2 (used with the x-basis), hh = 0.5*Y3 (y-basis).
//     The rotation applies cos/sin to hw for the x components and to hh for the y
//     components independently, exactly as the FMUL 0.5 [0x3007bce8] / FMUL -1.0
//     [0x3007bdb0] / FMUL 1.0 [0x3007bce0] sequence dictates:
//       v0 = (cx - cos*hw + sin*hw,  cy - sin*hh - cos*hh)   -> [0x24],[0x28]
//       v1 = (cx + cos*hw + sin*hw,  cy + sin*hh - cos*hh)   -> [0x2c],[0x30]
//       v2 = (cx + cos*hw - sin*hw,  cy + sin*hh + cos*hh)   -> [0x34],[0x38]
//       v3 = (cx - cos*hw - sin*hw,  cy - sin*hh + cos*hh)   -> [0x3c],[0x40]
//     (Eight consecutive output floats at frame [0x24..0x43], i.e. verts[0..7].)
//   * Submit via cgame_syscall(CG_R_DRAW_ROTATED_QUAD, &verts[0], cg_rotatedPicShaderParams,
//     hShader). The middle arg is the fixed static param block at 0x30071874
//     ({0,0,0,1, 0,1,1,0, 1,-1,-1,1, -1,1,1,-1}); the last is arg5 [ESP+0x5c]
//     (the shader handle), pushed as a raw dword. Push order at 0x3001ccba..ccd1
//     proves the arg order: PUSH hShader; PUSH 0x30071874; PUSH &verts; PUSH 0x4c.
//
// ABI: caller-cleaned cdecl, six 32-bit stack args (x,y,w,h,angle floats; hShader
// int). Verified at call sites 0x30016bb6 (six PUSHes then ADD ESP,0x18) and
// 0x30031d2e (six PUSHes). The function cleans its own frame + syscall pushes
// (ADD ESP,0x54) and does a bare RET; void return.

#include "client/cgame/globals.h"          /* cgs_screenXScale/YScale, cgame_syscall, cg_rotatedPicShaderParams */
#include "client/cgame/client_recovered.h" /* CG_R_DRAW_ROTATED_QUAD, CG_DrawRotatedPic decl */

#include "compat/coduo_native_x87.h"

/*
 * angle->radians as two separate single-precision multiplies by these .rdata
 * constants, matching the machine code's split of M_PI/180 into (* M_PI) then
 * (* 1/180). Named to preserve the exact two-step float rounding the DLL performs.
 */
#define CG_PI_F 3.1415927410125732f /* 0x3007bd88 = 0x40490fdb */
#define CG_INV_180_F 0.0055555556900799274f /* 0x3007bed4 = 0x3bb60b61; 1/180 */

void CG_DrawRotatedPic(float x, float y, float w, float h, float angle, int32_t hShader)
{
    /* Screen-scaled top-left (X0,Y1) and dimensions (X2,Y3). */
    float X0 = cgs_screenXScale * x;
    float Y1 = cgs_screenYScale * y;
    float X2 = cgs_screenXScale * w;
    float Y3 = cgs_screenYScale * h;

    /* angle (degrees) -> radians, split as the machine code does. */
    float angleRad = (float)((long double)angle * (long double)CG_PI_F * (long double)CG_INV_180_F);

    /* One hardware FSINCOS on Intel/AMD. The adapter commits cosine first and
     * sine second, matching 0x3001cbc4..0x3001cbd0; ARM64 emulation is outside
     * the current client scope. */
    float s;
    float c;
    coduo_x87_sincosf(angleRad, &s, &c);

    /*
     * Quad center and per-axis half extents.  Rounding here is ASYMMETRIC and is
     * dictated by the instruction stream, not by symmetry of the formula:
     *   hw (0x3001cbd4 FLD [0x50]; FMUL 0.5) is NEVER stored to a float slot --
     *     it stays live in st(0) from 0x3001cbd8 until 0x3001cc0b consumes it in
     *     place (FMUL s), so it must NOT be rounded here (long double).
     *   hh (0x3001cbde FMUL 0.5; 0x3001cbe8 FSTP DWORD [0x50]) IS rounded.
     */
    long double hw = 0.5f * (long double)X2;   /* 0x3007bce8 = 0.5f; kept in st(0) */
    float hh = 0.5f * Y3;                      /* 0x3001cbe8 FSTP DWORD [0x50] */
    float cx = X0 + hw;                        /* 0x3001cbf2 FSTP DWORD [0xc]  */
    float cy = hh + Y1;                        /* 0x3001cbfe FSTP DWORD [0x10] */

    /*
     * cos/sin scaled by each axis half-extent.  Again asymmetric: the two FMULs
     * that feed a DWORD store are rounded; the other two live in st(0..1) across
     * every vertex below and stay 80-bit.
     */
    float cx_hw = c * hw;              /* 0x3001cc07 FSTP DWORD [0x14] */
    long double sx_hw = hw * s;        /* 0x3001cc0b FMUL [0x58] -- no store */
    long double sx_hh = hh * s;        /* 0x3001cc13 FMUL [0x58] -- no store */
    float cx_hh = hh * c;              /* 0x3001cc1e FSTP DWORD [0x20] */

    /*
     * Rounded intermediates the DLL spills and reloads (Class-1 sites):
     *   t0 = cx - cx_hw*1.0f  -> FSTP DWORD [0x48] @0x3001cc34, reloaded @cc40/ccc2
     *   t1 = cx_hw + cx       -> FST  DWORD [0x54] @0x3001cc78 (FST keep: the
     *        chain for v1.x continues on the UNROUNDED st(0); [0x54] is the
     *        rounded copy that v2.x reloads @0x3001cc98)
     * The sign constants are genuine negated-constant multiplies by
     * 0x3007bdb0 = -1.0f, not source-level negations.
     */
    float verts[8];
    float t0 = cx - cx_hw * 1.0f;      /* 0x3007bce0 = 1.0f  */
    float negCxHh = -1.0f * cx_hh;          /* 0x3001cc54 FSTP DWORD [0x50] */
    long double t1raw = cx_hw + cx;
    float t1 = (float)t1raw;           /* 0x3001cc78 FST DWORD [0x54] */
    long double negSxHhRaw = -1.0f * sx_hh;
    float negSxHh = (float)negSxHhRaw;      /* 0x3001cc60 FST DWORD [0x4c] */
    /* sx_hw is rounded into slot [0x50] only at 0x3001cc94 (FXCH; FSTP DWORD),
     * i.e. AFTER v0.x/v1.x have consumed it unrounded; v2.x/v3.x reload it. */
    float sxHwR = (float)sx_hw;           /* 0x3001cc94 FSTP DWORD [0x50] */

    verts[0] = t0 - -1.0f * sx_hw;                  /* v0.x  [0x24] 0x3001cc46 */
    verts[1] = (negSxHhRaw + negCxHh) + cy;         /* v0.y  [0x28] 0x3001cc6c */
    verts[2] = t1raw - -1.0f * sx_hw;               /* v1.x  [0x2c] 0x3001cc7e */
    verts[3] = (negCxHh + sx_hh) + cy;              /* v1.y  [0x30] 0x3001cc8e */
    verts[4] = t1 - sxHwR;                          /* v2.x  [0x34] 0x3001cca4 */
    verts[5] = (cx_hh + sx_hh) + cy;                /* v2.y  [0x38] 0x3001ccb6 */
    verts[6] = t0 - sxHwR;                          /* v3.x  [0x3c] 0x3001ccd1 */
    verts[7] = (cx_hh + negSxHh) + cy;              /* v3.y  [0x40] 0x3001cce1 */

    /* cgame_syscall(76, &verts[0], cg_rotatedPicShaderParams, hShader). */
    cgame_syscall(CG_R_DRAW_ROTATED_QUAD, (intptr_t)&verts[0], (intptr_t)cg_rotatedPicShaderParams, hShader);
}
