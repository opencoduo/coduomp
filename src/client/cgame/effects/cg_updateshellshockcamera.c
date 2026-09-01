#include "../globals.h"
#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3003b670..0x3003b7d4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b670_3003b7d4.mcode
//
// CG_UpdateShellShockCamera — per-frame view-axis perturbation for the
// shellshock screen-blur effect. Runs during the refdef/view-setup pass (sole
// caller 0x300425de, immediately after the viewaxis is built at 0x3004c200) and
// rotates the current view axis (cg.refdef.viewaxis, stored as the three
// consecutive vec3 rows cg_refdef.viewaxis[0] / cg_refdef.viewaxis[1] /
// cg_refdef.viewaxis[2] at 0x30487a9c..0x30487bc) by a small orthonormal basis
// derived from the two screen-blur amounts cg_shellshockScreenBlurX/Y.
//
// NAME: the exact CoD source symbol is unproven. The role is proven from the
// machine code (reads cg_shellshockScreenBlurX/Y, builds a blur-tilt basis,
// MatrixMultiplies it into cg.refdef.viewaxis). The same-module PPC bank lists a
// CG_UpdateShellShockCamera in the shellshock family. Its cluster position and
// matching camera-axis behavior identify this Windows body with that symbol.
// The .mcode size-guess "PM_Jump" is REJECTED: this function does no jump-input
// check, no jump velocity, and touches no playerState/pml — it is a pure x87
// view-axis rotation for the screen-blur effect (matched to PM_Jump only by
// byte size 0x164 == 0x164, which the naming rules forbid).
//
// Return: void. Both paths (the early-out and the work path) simply restore ESP
// (ADD ESP,0x48) and RET with no value set; no register carries a result. The
// prompt's "returns qboolean likely" is disproven by the bytes.
//
// Callee ABIs are re-derived from THIS call site:
//   - VectorNormalize(vec3_t v): v arrives in ESI (register arg); the returned
//     length is left in ST0 and discarded here by FSTP ST0. (0x30049700)
//   - MatrixMultiply(in1, in2, out): in1(lhs)=ECX, in2(rhs)=EAX, out=EDX; all
//     three are row-major float[3][3] (axis_t). (0x3004a5b0)

void CG_UpdateShellShockCamera(void)
{
    /* 0x3003b670..0x3003b69b: run only if either blur component is nonzero.
     * FUCOMPP against 0.0f at .rdata 0x3007bcec (exact address dumped: the
     * .rdata float pool is 1.0f@bce0 / 2.0f@bce4 / 0.5f@bce8 / 0.0f@bcec).
     * If both blurX and blurY are exactly 0.0f, return without touching the
     * view axis. */
    if (cg_shellshockScreenBlurX == 0.0f && cg_shellshockScreenBlurY == 0.0f)
        return;

    /* Local orthonormal basis (row-major float[3][3]) built from the blur
     * amounts. Row 0 is the blur "forward" direction; rows 1 and 2 are the two
     * cross-product-derived orthogonal rows. Laid out on the stack at ESP+0x04. */
    axis_t blurAxis;
    /* The refdef view axis, gathered as a contiguous 3x3 from the three
     * consecutive vec3 globals cg_refdef.viewaxis[0]/Axis1/Axis2. Laid out at
     * ESP+0x28. */
    axis_t viewAxis;

    /* 0x3003b6a1..0x3003b6d0: seed row 0 = (1.0, blurX, blurY) and prepare the
     * "up" reference vector (0, 0, 1) reused in the cross product. The 0.0f /
     * 1.0f slots come from ECX=0x3f800000 (1.0f) and XOR EAX (0.0f as an integer
     * bit pattern for +0.0f). Then normalize row 0 in place. */
    blurAxis[0][0] = 1.0f;
    blurAxis[0][1] = cg_shellshockScreenBlurX;
    blurAxis[0][2] = cg_shellshockScreenBlurY;

    vec3_t up;
    up[0] = 0.0f;   /* ESP+0x1c */
    up[1] = 0.0f;   /* ESP+0x20 */
    up[2] = 1.0f;   /* ESP+0x24 (ECX=1.0f) */

    (void)VectorNormalize(blurAxis[0]);

    /* 0x3003b6d7..0x3003b719: row1 = cross(up, blurAxis[0]). Component form as
     * emitted (a=blurAxis[0]; each FSUBP subtracts the second-loaded product
     * from the first-loaded one, e.g. row1[0]: FLD up[1]; FMUL a[2]; FLD up[2];
     * FMUL a[1]; FSUBP => up[1]*a[2] - up[2]*a[1]):
     *   row1[0] = up[1]*a[2] - up[2]*a[1]
     *   row1[1] = up[2]*a[0] - up[0]*a[2]
     *   row1[2] = up[0]*a[1] - up[1]*a[0]
     * With up = (0,0,1) this reduces to (-a[1], a[0], 0), but the machine code
     * computes the full cross so it is written that way. */
    blurAxis[1][0] = up[1] * blurAxis[0][2] - up[2] * blurAxis[0][1];
    blurAxis[1][1] = up[2] * blurAxis[0][0] - up[0] * blurAxis[0][2];
    blurAxis[1][2] = up[0] * blurAxis[0][1] - up[1] * blurAxis[0][0];

    /* 0x3003b71d..0x3003b722: normalize row1 in place. */
    (void)VectorNormalize(blurAxis[1]);

    /* 0x3003b724..0x3003b7c6: build viewAxis from the three refdef view rows and
     * compute row2 = cross(blurAxis[0], blurAxis[1]) interleaved with the row
     * loads. Component form as emitted (a=blurAxis[0], r=blurAxis[1]):
     *   row2[0] = r[2]*a[1] - r[1]*a[2]
     *   row2[1] = r[0]*a[2] - r[2]*a[0]
     *   row2[2] = r[1]*a[0] - r[0]*a[1]
     * i.e. cross(a, r). */
    viewAxis[0][0] = cg_refdef.viewaxis[0][0];
    viewAxis[0][1] = cg_refdef.viewaxis[0][1];
    viewAxis[0][2] = cg_refdef.viewaxis[0][2];
    viewAxis[1][0] = cg_refdef.viewaxis[1][0];
    viewAxis[1][1] = cg_refdef.viewaxis[1][1];
    viewAxis[1][2] = cg_refdef.viewaxis[1][2];
    viewAxis[2][0] = cg_refdef.viewaxis[2][0];
    viewAxis[2][1] = cg_refdef.viewaxis[2][1];
    viewAxis[2][2] = cg_refdef.viewaxis[2][2];

    blurAxis[2][0] = blurAxis[1][2] * blurAxis[0][1] - blurAxis[1][1] * blurAxis[0][2];
    blurAxis[2][1] = blurAxis[1][0] * blurAxis[0][2] - blurAxis[1][2] * blurAxis[0][0];
    blurAxis[2][2] = blurAxis[1][1] * blurAxis[0][0] - blurAxis[1][0] * blurAxis[0][1];

    /* 0x3003b7a3..0x3003b7ca: out = blurAxis * viewAxis, written back over the
     * view axis in place (out=&cg_refdef.viewaxis[0], which is viewaxis[0] and is
     * contiguous with Axis1/Axis2). in1(lhs)=blurAxis in ECX, in2(rhs)=viewAxis
     * in EAX, out=EDX. */
    MatrixMultiply(blurAxis, viewAxis, cg_refdef.viewaxis);
}
