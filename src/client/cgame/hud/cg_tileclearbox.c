#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3001d0d0..0x3001d15c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d0d0_3001d15c.mcode
//
// CG_TileClearBox — emit one axis-aligned 2D stretch-pic
// whose texture coordinates are derived from integer pixel positions divided by
// 64, i.e. a segment of a tiled-texture border/edge. It converts four integer
// coordinates (arg0..arg3) to float, scales the texcoord-carrying values by the
// .rdata constant at 0x3007befc (= 0x3c800000 = 1.0f/64.0f, a 64-texel tiling
// step), and forwards a 9-dword frame to trap_R_DrawStretchPic (0x3003e0f0,
// cgame trap 73). The shader handle arrives in EAX (see __usercall note below).
//
// Callers: 0x3001d160 issues four of these back-to-back (0x3001d1b5, 0x3001d1c6,
// 0x3001d1dc, 0x3001d1f0), one per side of a rectangle, each time reloading the
// shader handle from a cgame global (0x3044b6f0) into EAX just before the call —
// the classic "draw the four edges of a tiled border" pattern.
//
// The .mcode header name "PM_ShouldMakeFootsteps" is REJECTED: that name is a
// size guess (win size 0x8c matched). PM_ShouldMakeFootsteps is a bg_pmove
// predicate returning a qboolean from playerState velocity/ground state; this
// function performs no player-move logic, reads no playerState, converts ints to
// floats via FILD, multiplies by a tiling constant, and issues the 2D stretch-pic
// trap. Size matching is explicitly forbidden by the contract.
//
// CALLING CONVENTION (__usercall): the shader handle is passed in EAX, not on the
// stack. Proof: the entry `PUSH EAX` (0x3001d0f0) spills EAX into the outgoing
// frame, and every one of the four call sites executes
// `MOV EAX, ds:0x3044b6f0` immediately before the call. The four ordinary
// arguments (arg0..arg3) are the cdecl stack slots [ESP+4..+0x10]; the caller
// cleans them (ADD ESP,0x40 across its four calls). In this portable
// reconstruction the register argument is modeled as a leading parameter
// `hShader`; on the real i386 build it is EAX.
//
// OUTGOING FRAME (byte-exact; simulated from the FILD/FMUL/FSTP/PUSH stream).
// K = 1.0f/64.0f. trap_R_DrawStretchPic receives, in positional order:
//     x       = (float)arg0
//     y       = (float)arg1
//     w       = (float)arg2
//     h       = (float)arg3
//     s1      = (float)arg0 * K
//     t1      = (float)arg1 * K
//     s2      = (float)(arg0+arg2) * K   (dword ADD, then direct FILD)
//     t2      = (float)(arg1+arg3) * K   (dword ADD, then direct FILD)
//     hShader = the EAX register argument, forwarded as-is
// The prior reconstruction incorrectly inserted the register-passed shader into
// the portable C stack-argument sequence and shifted every one of these slots.
//
// The two integer sums are computed with EDX/ECX before the FILD that reloads
// them (0x3001d0df ADD EDX,ECX for arg1+arg3 -> written back to [ESP+0x10];
// 0x3001d100 ADD ECX,EAX for arg0+arg2 -> written back to [ESP+0x2c]), so the
// FILD at 0x3001d0fc/0x3001d118 converts the 32-bit integer sum, not a float sum.

/* .rdata float constant at 0x3007befc = 0x3c800000 = 1.0f/64.0f (tiling step). */
#define CG_TILE_TEXCOORD_STEP (1.0f / 64.0f)

// Name resolved during reconstruction of the sole caller CG_TileClear (0x3001d160):
// the caller matches Quake3 CG_TileClear and the same-module PPC bank lists
// cgame_mp!CG_TileClearBox as its helper. The prior role name
// "CG_DrawTiledPicSegment" is superseded.
int32_t CG_TileClearBox(int32_t hShader, int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3)
{
    /* 0x3001d0d3..0x3001d0f5: round arg1, form its wrapped sum with arg3,
     * then round arg0. The remaining declarations follow the target's x87
     * load/store sequence rather than merely reproducing the final formulas. */
    long double yValue = (long double)arg1;
    int32_t sum_bd = coduo_int32_from_bits((uint32_t)arg1 + (uint32_t)arg3);
    float y = (float)yValue;
    long double xValue = (long double)arg0;
    float x = (float)xValue;

    /* t2's direct FILD is live while the other wrapped dword sum is formed. */
    long double t2Value = (long double)sum_bd;
    int32_t sum_ac = coduo_int32_from_bits((uint32_t)arg0 + (uint32_t)arg2);
    t2Value *= (long double)CG_TILE_TEXCOORD_STEP;
    float t2 = (float)t2Value;

    float s2 = (float)((long double)sum_ac * (long double)CG_TILE_TEXCOORD_STEP);
    float t1 = (float)((long double)y * (long double)CG_TILE_TEXCOORD_STEP);
    float s1 = (float)((long double)x * (long double)CG_TILE_TEXCOORD_STEP);
    float h = (float)arg3;
    float w = (float)arg2;

    /* Every non-integer slot is forwarded as its raw 32-bit float bit pattern. */
    return trap_R_DrawStretchPic(CG_FloatBits(x), CG_FloatBits(y), CG_FloatBits(w), CG_FloatBits(h), CG_FloatBits(s1), CG_FloatBits(t1),
                                 CG_FloatBits(s2), CG_FloatBits(t2), hShader);
}
