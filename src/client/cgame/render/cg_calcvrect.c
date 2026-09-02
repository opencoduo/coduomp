// Source: uo_cgame_mp_x86.dll 0x3003f510..0x3003f601
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f510_3003f601.mcode
//
// CG_CalcVrect — compute the cropped 3D-view rectangle (cg.refdef view region:
// x, y, width, height, in real backbuffer pixels) from the cg_viewsize percentage
// and the current backbuffer dimensions, centering it on screen. This is the
// canonical Quake3/CoD CG_CalcVrect.
//
// NAME: resolved by behavior + call graph, and rejecting the .mcode size-guess.
//   * It reads cgs.glconfig.vidWidth/vidHeight (0x30447a88/0x30447a8c) and writes
//     the four cg.refdef view-rect words at 0x30487a78..0x30487a84 — the exact
//     rectangle that its sibling CG_TileClear (0x3001d160, already reconstructed)
//     reads back and tests for full-screen coverage.
//   * It clamps a "cg_viewsize" cvar to [30,100] and centers width/height by
//     (dim - size*dim/100)/2 — the textbook CG_CalcVrect body.
//   * The same-module PPC bank (cgame_mp/inputs/ppc_function_names.tsv) lists
//     cgame_mp!CG_CalcVrect.
//   The .mcode-assigned "BG_AnimScriptEvent" is REJECTED: that name came from a
//   pure win-size==0xf1 match (forbidden by the contract), and BG_AnimScriptEvent
//   queues animation-script events — it does not read glconfig, touch the refdef
//   view rect, or set the cg_viewsize cvar. Zero behavioral overlap.
//
// GLOBALS consumed (resolved and renamed at their definitions):
//   cg_nextSnap            [0x30459164]  snapshot_t* : ps.pmType at +0x10
//   cg_viewSizeCvar        [0x3052eec0]  vmCvar_t      : .integer at +0xc
//   cg_letterbox_vmCvar.integer   [0x304567ac]  qboolean gate : scale height to 85%
//   cgs_glconfig.vidWidth  [0x30447a88]  backbuffer width
//   cgs_glconfig.vidHeight [0x30447a8c]  backbuffer height
//   cg_refdef.x/Y/Width/Height [0x30487a78..0x30487a84]  output rectangle
//
// INTERMISSION GATE (0x3003f511..0x3003f526): reads cg_nextSnap, then
//   CMP [snap+0x10],5 ; JNZ use-cvar. Field +0x10 is the embedded playerState
//   pmType (snapshot_t.psPmType); value 5 is PM_TYPE_INTERMISSION. So during
//   intermission the view is forced full-size (viewsize 100); otherwise the
//   cg_viewsize cvar is used. Matches Quake3: `if (pm_type == PM_INTERMISSION)`.
//
// CLAMP (0x3003f528..0x3003f574): viewsize = cg_viewSizeCvar.integer.
//   if (viewsize < 30)  { trap_Cvar_SetValue(&cg_viewSizeCvar,"30");  viewsize=30;  }
//   else if (viewsize > 100){ trap_Cvar_SetValue(&cg_viewSizeCvar,"100"); viewsize=100; }
//   Instruction proof: CMP ESI,0x1e ; JGE (signed >=30) ; else the <30 branch sets
//   "30". CMP ESI,0x64 ; JLE (signed <=100) ; else the >100 branch sets "100". Each
//   set is trap(0xa, &cg_viewSizeCvar, string) with ADD ESP,0xc (three dwords,
//   caller-cleaned) — the vmCvar-handle cvar-set (trap_Cvar_SetValue, id 10).
//
// LETTERBOX (0x3003f576..0x3003f592): heightSize = viewsize; if (cg_letterbox_vmCvar.integer)
//   heightSize = Q_rint((float)viewsize * 0.85f). The float const at 0x3007be84 is
//   0.85000002 (== 0.85f). Q_rint (0x3006be3c) rounds ST0 to int in EAX; the
//   FILD [ESP+0xc]/FMUL/CALL sequence pushes viewsize as the x87 argument.
//   widthSize keeps the raw clamped viewsize (EBX is never rescaled).
//
// RECT MATH (0x3003f594..0x3003f5f9):
//   width  = vidWidth  * widthSize  / 100;   height = vidHeight * heightSize / 100;
//   The /100 is the emitted signed-divide-by-100 idiom (IMUL by 0x51eb851f;
//   SAR EDX,5; add sign bit). Both products are non-negative here (positive
//   dimensions, size in [30,100]).
//   width  &= ~1;  height &= ~1;            (AND with 0xfffffffe: force even)
//   viewX  = (vidWidth  - width ) / 2;      (CDQ ; SUB ; SAR 1: signed /2)
//   viewY  = (vidHeight - height) / 2;
//   Stores: refdef.x=viewX(0x78), refdef.y=viewY(0x7c), refdef.width=width(0x80),
//           refdef.height=height(0x84).
//
// i386 ABI: cdecl, no args (RET with no immediate; the entry PUSH ECX reserves the
// [ESP+0xc] scratch slot holding `viewsize` for the FILD). EBX/ESI/EDI/ECX
// save/restore is calling convention, not source behavior.

#include "../client_recovered.h"
#include "../globals.h"

/* cg_viewsize clamp range. Written as plain decimals: they are percentages, the
 * exact literals the original clamp used (compared as signed ints; set as the
 * strings "30"/"100"). */
enum {
    CG_VIEWSIZE_MIN = 30,
    CG_VIEWSIZE_MAX = 100
};

void CG_CalcVrect(void)
{
    int32_t widthSize;
    int32_t heightSize;

    if (cg_nextSnap->ps.pmType == PM_TYPE_INTERMISSION) {
        /* Intermission: force a full-size view. */
        widthSize = CG_VIEWSIZE_MAX;
    } else {
        int32_t viewsize = cg_viewSizeCvar.integer;

        if (viewsize < CG_VIEWSIZE_MIN) {
            trap_Cvar_SetValue(&cg_viewSizeCvar, "30");
            viewsize = CG_VIEWSIZE_MIN;
        } else if (viewsize > CG_VIEWSIZE_MAX) {
            trap_Cvar_SetValue(&cg_viewSizeCvar, "100");
            viewsize = CG_VIEWSIZE_MAX;
        }
        widthSize = viewsize;
    }

    /* Vertical size follows the horizontal size, optionally letterboxed to 85%. */
    heightSize = widthSize;
    if (cg_letterbox_vmCvar.integer) {
        /* 0x3003f583: FILD widthSize; FMUL 0.85f with no intervening FSTP -- the
         * integer feeds the multiply directly (Class 4), so no (float) cast. */
        heightSize = coduo_fp_to_i32_extended(widthSize * 0.85f);
    }

    {
        int32_t vidWidth  = cgs_glconfig.vidWidth;
        int32_t vidHeight = cgs_glconfig.vidHeight;
        /* size percentage of the backbuffer, forced to an even pixel count. */
        int32_t width  = (vidWidth  * widthSize  / 100) & ~1;
        int32_t height = (vidHeight * heightSize / 100) & ~1;

        /* Center the view rect on screen (signed halving rounds toward zero). */
        int32_t viewX = (vidWidth  - width ) / 2;
        int32_t viewY = (vidHeight - height) / 2;

        cg_refdef.x      = (uint32_t)viewX;
        cg_refdef.height = (uint32_t)height;
        cg_refdef.width  = (uint32_t)width;
        cg_refdef.y      = (uint32_t)viewY;
    }
}
