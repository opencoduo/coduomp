// Source: uo_cgame_mp_x86.dll 0x3001ccf0..0x3001ce35
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001ccf0_3001ce35.mcode
//
// CG_DrawTurretTagQuad (role name; the .mcode size-guess "G_BackupSpawnVars" is
// REJECTED — this is not a spawn-var backup: there is no varinfo table, no field
// copy loop, and no string work. It is a pure float 2D-quad rotate + a cgame 2D
// draw syscall). The name is adopted from the turret-tag HUD caller (0x30031e20);
// see the reconstructed decl in client_recovered.h. The exact original CoD symbol
// is unproven (no cgame syscall-id table recovered), so the name is a role name.
//
// Behavior (all proven against the .mcode instruction stream):
//   * Convert the caller's angle (arg `angleDegrees`, [ESP+0x54]) to radians by two
//     x87 multiplies: (angle * M_PI) [FMUL 0x3007bd88 = 3.14159274f]
//     then * (1/180) [FMUL 0x3007bed4 = 0.0055555557f]. Kept as two separate
//     multiplies in that order to preserve the exact single-precision result.
//   * FSINCOS the radian angle -> {sin, cos}.
//   * Screen-scale the rotation basis and the center by the cgs screen-scale
//     globals (cgs_screenXScale @0x30447aa4, cgs_screenYScale @0x30447aa8). NOTE:
//     this binary computes screenXScale = vidWidth/480 and screenYScale =
//     vidHeight/640 (swapped from stock CoD/Q3 X/640, Y/480 — see globals.c/.h);
//     the code here simply consumes them and does not care which is which.
//   * Transform each of the four {x,y} corner offsets in `cornerOffsets` (passed in
//     EDX; eight consecutive floats = M[0..7]) through the rotation, adding the
//     scaled center (tx = screenXScale*x, ty = screenYScale*y), producing eight
//     output floats (four screen-space {x,y} vertices).
//   * Submit the quad via cgame_syscall(CG_R_DRAW_ROTATED_QUAD, &verts[0], shaderParams,
//     hShader). CG_R_DRAW_ROTATED_QUAD (0x4c) is a rotated 2D-pic / 2D-poly draw trap.
//
// Register-passing ABI (caller-observed at both call sites 0x3001aae0 and
// 0x30031f55, which each `LEA EDX,[esp+...]` immediately before the CALL): the
// corner-offset array is passed in EDX; x,y,shaderParams,angleDegrees,hShader are
// caller-cleaned cdecl stack args. The function cleans its own frame + syscall
// pushes (ADD ESP,0x54) and does a bare RET; the caller cleans the five stack args.
// This is not standard cdecl/fastcall, so EDX is modeled as an explicit leading
// parameter rather than a calling-convention attribute (no build target requires
// the attribute; see WORKFLOW.md).

#include "client/cgame/globals.h"          /* cgs_screenXScale / cgs_screenYScale, cgame_syscall */
#include "client/cgame/client_recovered.h" /* CG_R_DRAW_ROTATED_QUAD, CG_DrawTurretTagQuad decl */
#include "compat/coduo_native_x87.h"

/*
 * The angle->radians conversion is done as two separate single-precision multiplies
 * by these two .rdata constants, matching the machine code's split of M_PI/180 into
 * (* M_PI) then (* 1/180). Written as named constants rather than one fused
 * 0.017453f to preserve the exact two-step float rounding the DLL performs.
 */
#define CG_PI_F 3.1415927410125732f /* 0x3007bd88 = 0x40490fdb */
#define CG_INV_180_F 0.0055555556900799274f /* 0x3007bed4 = 0x3bb60b61; 1/180 */

void CG_DrawTurretTagQuad(float *cornerOffsets, float x, float y, const float *shaderParams, float angleDegrees, int32_t hShader)
{
    const float *m = cornerOffsets; /* EDX: four {x,y} corner offsets, M[0..7] */

    /* angleDegrees -> radians remains one x87 chain through both m32 multiplies,
     * with its only binary32 store after the second product. */
    float angleRad = (float)(((long double)angleDegrees * (long double)CG_PI_F) * (long double)CG_INV_180_F);

    /* One hardware FSINCOS, with cosine committed before sine. */
    float s;
    float c;
    coduo_x87_sincosf(angleRad, &s, &c);

    /* Screen-scaled rotation basis (0x3001cd2c..0x3001cd56).
     *   xs = cgs_screenXScale, ys = cgs_screenYScale.
     *   S = xs*sin, C = xs*cos   (the persistent x87 ST1/ST2 pair through the block)
     *   ysSin = ys*sin  -> local [ESP+0x1c]
     *   ysCos = ys*cos  -> local [ESP+0x20]
     *
     * ASYMMETRIC SPILL (proven from the bytes): C (0x3001cd2c/cd32) and S
     * (0x3001cd35/cd3b) are NEVER stored to a float slot -- they live in st(2)/
     * st(1) unrounded for the whole vertex block -- while ysSin (FSTP 0x3001cd49)
     * and ysCos (FSTP 0x3001cd56) ARE rounded to float. Hence C/S are long double
     * and the ys* pair stays float; a float C/S would add two roundings the DLL
     * does not perform.
     */
    long double C = (long double)cgs_screenXScale * (long double)c;
    long double S = (long double)cgs_screenXScale * (long double)s;
    float ysSin = (float)((long double)cgs_screenYScale * (long double)s);
    float ysCos = (float)((long double)cgs_screenYScale * (long double)c);

    /* Scaled center offsets (0x3001cd5a..0x3001cd6e):
     *   tx = xs * x   (arg0, [ESP+0x48]; stays as x87 ST0 across the vertex block,
     *                  never rounded -> long double)
     *   ty = ys * y   (arg1, [ESP+0x4c]; rounded by FSTP 0x3001cd6e to [ESP+0x10])
     */
    long double tx = (long double)cgs_screenXScale * (long double)x;
    float ty = (float)((long double)cgs_screenYScale * (long double)y);

    /* Four output vertices. For corner i (matrix pair m[2i], m[2i+1]):
     *   vert.x = C*m[2i]     + tx - S*m[2i+1]        (FLD C; FMUL m; FADD tx; FLD S; FMUL m1; FSUBP)
     *   vert.y = ysSin*m[2i] + ysCos*m[2i+1] + ty    (accumulated then FADD ty)
     * Corners 0..2 use FLD-ST-relative reloads of C and S; corner 3 uses FXCH to
     * consume the persistent basis, but the arithmetic is identical. Each vertex
     * is rounded exactly once, at its own FSTP; the C/S/tx operands enter every
     * chain unrounded (see the long double decls above).
     */
    float verts[8];

    verts[0] = (float)((C * (long double)m[0] + tx) - S * (long double)m[1]);
    verts[1] = (float)(((long double)ysSin * (long double)m[0] + (long double)ysCos * (long double)m[1]) + (long double)ty);

    verts[2] = (float)((C * (long double)m[2] + tx) - S * (long double)m[3]);
    verts[3] = (float)(((long double)ysCos * (long double)m[3] + (long double)ysSin * (long double)m[2]) + (long double)ty);

    verts[4] = (float)((C * (long double)m[4] + tx) - S * (long double)m[5]);
    verts[5] = (float)(((long double)ysCos * (long double)m[5] + (long double)ysSin * (long double)m[4]) + (long double)ty);

    verts[6] = (float)((C * (long double)m[6] + tx) - S * (long double)m[7]);
    long double lastY = ((long double)ysCos * (long double)m[7] + (long double)ysSin * (long double)m[6]) + (long double)ty;

    /* The last y result remains in ST0 while the final scalar arguments are
     * loaded and pushed, then is rounded into the vertex array. */
    int32_t drawShader = hShader;
    const float *drawShaderParams = shaderParams;
    verts[7] = (float)lastY;

    /* 0x3001ce12/ce1a: reload hShader ([ESP+0x58]) and shaderParams ([ESP+0x50]).
     * 0x3001ce1e..ce2b: cgame_syscall(76, &verts[0], shaderParams, hShader).
     * Push order proves the argument order: PUSH hShader; PUSH shaderParams;
     * PUSH &verts; PUSH 0x4c; CALL. */
    cgame_syscall(CG_R_DRAW_ROTATED_QUAD, (intptr_t)&verts[0], (intptr_t)drawShaderParams, drawShader);
    /* 0x3001ce31 ADD ESP,0x54 ; 0x3001ce34 RET (bare) — void return. */
}
