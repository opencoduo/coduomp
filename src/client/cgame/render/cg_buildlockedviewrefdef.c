#include "../globals.h"
#include "../client_recovered.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30040360..0x30040568
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30040360_30040568.mcode
//
// CG_BuildLockedViewRefdef (name provisional): build cg.refdef for the special
// "locked" square-viewport render used when cg_lockedViewFace != 0. The caller,
// the refdef dispatcher at 0x30041a30, zeroes the whole cg.refdef block, then
// calls this routine on the cg_lockedViewFace != 0 branch (and returns straight
// after); the normal-view refdef setup is the other branch. cg_lockedViewFace
// (1..6) selects one of six axis-aligned camera orientations via a jump table --
// the six cardinal basis matrices of a cube face -- so this is a cube-face /
// environment-style fixed-axis view builder.
//
// NAMING NOTE: the .mcode mechanical name "CG_SpawnTracer" is a pure size match
// (win 0x208 == corpus cgame_mp CG_SpawnTracer 0x208) and is REJECTED. This body
// contains no tracer trajectory, no VectorMA, no entity spawn, and no call to
// trap_R_AddRefEntityToScene. It exclusively writes the cg.refdef static block
// (0x30487a78..0x30487abc: view rect, projection-scale/fov pair, vieworg vec3,
// and the three viewaxis rows) -- proven by the neighbouring, independently
// resolved cg_refdefView* globals (see globals.h) and by the caller's rep-stos
// of the same 20-dword block before it dispatches here. Exact original CoD name
// unresolved; the cg.refdef-builder role is proven.
//
// Calling convention: RET (no imm) with a single PUSH ECX / POP ECX bracketing
// the body -- ECX is spilled only to give FILD a memory operand at [ESP]. No
// stack arguments; all inputs/outputs are file-scope cg globals.

// cg_lockedViewFace values: 1..6 index one of six axis-aligned view orientations
// (the six cube-map faces). Exact source enum names unresolved; named by role.
enum {
    LOCKED_VIEW_FACE_1 = 1,
    LOCKED_VIEW_FACE_2 = 2,
    LOCKED_VIEW_FACE_3 = 3,
    LOCKED_VIEW_FACE_4 = 4,
    LOCKED_VIEW_FACE_5 = 5,
    LOCKED_VIEW_FACE_6 = 6,
};

// FMUL constant at .rdata 0x3007bd98 (double): 114.5915590261646 == 360/pi ==
// 2*(180/pi). Converts the atan2 radian result to degrees and doubles it, so the
// square fov spans the full [-fov/2, +fov/2] across the (size+2)-wide viewport.
#define RAD_TO_DEG_TIMES_TWO 114.59155902616465 /* 0x405ca5dc_1a63c1f8, == 360/pi */

void CG_BuildLockedViewRefdef(void)
{
    // 0x30040361..0x30040368: cg.refdef view rect origin x = y = 0.
    cg_refdef.x = 0;
    cg_refdef.y = 0;

    // 0x3004036d..0x30040372: size = cg_lockedViewSize + 2 (square viewport edge).
    int32_t viewSize = coduo_int32_from_bits((uint32_t)cg_lockedViewSize + 2u);

    // 0x30040375..0x30040381 + 0x30040396: fov angle = atan2(size, cg_lockedViewSize)
    //   * (360/pi). The FILDs push (double)size then (double)cg_lockedViewSize;
    //   FPATAN computes atan2(ST1, ST0) = atan2(size, cg_lockedViewSize).
    long double fovDeg = coduo_x87_atan2l((long double)viewSize, (long double)cg_lockedViewSize) * (long double)RAD_TO_DEG_TIMES_TWO;

    // 0x30040387..0x3004038c: cg.refdef width = height = size+2 (square).
    cg_refdef.width = (uint32_t)viewSize;
    cg_refdef.height = (uint32_t)viewSize;

    // 0x300403b2 FST / 0x300403b8 FSTP: store the same fov value to both the
    // projection-scale pair slots (fov_x and fov_y are equal for the square view).
    cg_refdef.fov_x = (float)fovDeg;
    cg_refdef.fov_y = (float)fovDeg;

    // 0x30040391 + 0x30040398 / 0x3004037b + 0x300403a6: copy vieworg .x and .y
    // from the requested source origin (int-width dword copies).
    cg_refdef.vieworg[0] = cg_predictedPlayerState.psOrigin[0];
    cg_refdef.vieworg[1] = cg_predictedPlayerState.psOrigin[1];

    // 0x300403be..0x300403cc: vieworg .z = source .z + cg_predictedPlayerState.viewHeightCurrent.
    //   FLD [0x304831e0] (source.z), FLD [0x304832bc] (offset), FADD, FSTP.
    cg_refdef.vieworg[2] = cg_predictedPlayerState.psOrigin[2] + cg_predictedPlayerState.viewHeightCurrent;

    // 0x3004039d..0x300403da: switch on (cg_lockedViewFace - 1), unsigned; any
    // value with (face-1) > 5 (i.e. face < 1 or face > 6) skips the axis fill and
    // returns. The jump table at 0x30040568 selects the viewaxis basis.
    switch (cg_lockedViewFace) {
    case LOCKED_VIEW_FACE_1: /* 0x300403e1 */
        // forward=(0,0,1), axis1=(0,1,0), axis2=(-1,0,0)
        cg_refdef.viewaxis[0][0] = 0.0f;
        cg_refdef.viewaxis[0][1] = 0.0f;
        cg_refdef.viewaxis[0][2] = 1.0f;
        cg_refdef.viewaxis[1][0] = 0.0f;
        cg_refdef.viewaxis[1][1] = 1.0f;
        cg_refdef.viewaxis[1][2] = 0.0f;
        cg_refdef.viewaxis[2][0] = -1.0f;
        cg_refdef.viewaxis[2][1] = 0.0f;
        cg_refdef.viewaxis[2][2] = 0.0f;
        break;
    case LOCKED_VIEW_FACE_2: /* 0x30040422 */
        // forward=(0,0,-1), axis1=(0,1,0), axis2=(1,0,0)
        cg_refdef.viewaxis[0][0] = 0.0f;
        cg_refdef.viewaxis[0][1] = 0.0f;
        cg_refdef.viewaxis[0][2] = -1.0f;
        cg_refdef.viewaxis[1][0] = 0.0f;
        cg_refdef.viewaxis[1][1] = 1.0f;
        cg_refdef.viewaxis[1][2] = 0.0f;
        cg_refdef.viewaxis[2][0] = 1.0f;
        cg_refdef.viewaxis[2][1] = 0.0f;
        cg_refdef.viewaxis[2][2] = 0.0f;
        break;
    case LOCKED_VIEW_FACE_3: /* 0x30040463 */
        // forward=(-1,0,0), axis1=(0,-1,0), axis2=(0,0,1)
        cg_refdef.viewaxis[0][0] = -1.0f;
        cg_refdef.viewaxis[0][1] = 0.0f;
        cg_refdef.viewaxis[0][2] = 0.0f;
        cg_refdef.viewaxis[1][0] = 0.0f;
        cg_refdef.viewaxis[1][1] = -1.0f;
        cg_refdef.viewaxis[1][2] = 0.0f;
        cg_refdef.viewaxis[2][0] = 0.0f;
        cg_refdef.viewaxis[2][1] = 0.0f;
        cg_refdef.viewaxis[2][2] = 1.0f;
        break;
    case LOCKED_VIEW_FACE_4: /* 0x300404ae */
        // forward=(1,0,0), axis1=(0,1,0), axis2=(0,0,1)
        cg_refdef.viewaxis[0][0] = 1.0f;
        cg_refdef.viewaxis[0][1] = 0.0f;
        cg_refdef.viewaxis[0][2] = 0.0f;
        cg_refdef.viewaxis[1][0] = 0.0f;
        cg_refdef.viewaxis[1][1] = 1.0f;
        cg_refdef.viewaxis[1][2] = 0.0f;
        cg_refdef.viewaxis[2][0] = 0.0f;
        cg_refdef.viewaxis[2][1] = 0.0f;
        cg_refdef.viewaxis[2][2] = 1.0f;
        break;
    case LOCKED_VIEW_FACE_5: /* 0x300404f6 */
        // forward=(0,-1,0), axis1=(1,0,0), axis2=(0,0,1)
        cg_refdef.viewaxis[0][0] = 0.0f;
        cg_refdef.viewaxis[0][1] = -1.0f;
        cg_refdef.viewaxis[0][2] = 0.0f;
        cg_refdef.viewaxis[1][0] = 1.0f;
        cg_refdef.viewaxis[1][1] = 0.0f;
        cg_refdef.viewaxis[1][2] = 0.0f;
        cg_refdef.viewaxis[2][0] = 0.0f;
        cg_refdef.viewaxis[2][1] = 0.0f;
        cg_refdef.viewaxis[2][2] = 1.0f;
        break;
    case LOCKED_VIEW_FACE_6: /* 0x3004050c */
        // forward=(0,1,0), axis1=(-1,0,0), axis2=(0,0,1)
        cg_refdef.viewaxis[0][0] = 0.0f;
        cg_refdef.viewaxis[0][1] = 1.0f;
        cg_refdef.viewaxis[0][2] = 0.0f;
        cg_refdef.viewaxis[1][0] = -1.0f;
        cg_refdef.viewaxis[1][1] = 0.0f;
        cg_refdef.viewaxis[1][2] = 0.0f;
        cg_refdef.viewaxis[2][0] = 0.0f;
        cg_refdef.viewaxis[2][1] = 0.0f;
        cg_refdef.viewaxis[2][2] = 1.0f;
        break;
    default: /* 0x30040566: (cg_lockedViewFace - 1) unsigned > 5 -> no axis fill */
        break;
    }
}
