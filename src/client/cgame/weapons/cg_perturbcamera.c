// Source: uo_cgame_mp_x86.dll 0x30046490..0x3004656b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30046490_3004656b.mcode
//
// CG_PerturbCamera(float dist)
//
// Naming: the mechanical `# name CG_PerturbCamera` is a broad-corpus name backed
// only by a size match (win 0xdb / matched 0xdc) which the contract forbids
// trusting. The BEHAVIOR, however, does support a camera-perturbation role, so
// the name is kept as the best defensible symbol with the note below. What the
// machine code actually does, proven instruction by instruction:
//
//   1. AngleVectors(&cg_refdefViewAngles[0], fa, ra, ua) — builds an orthonormal
//      basis from the {pitch,yaw,roll} spin-angle triple stored at 0x30487ac8
//      (cg_refdefViewAngles[0] is spinAngles[0]; acc/ad0 are the yaw/roll
//      partners). Only the `forward` output (fa) is consumed; the `right`/`up`
//      outputs share the same stack scratch and are overwritten before use.
//   2. Slides the view origin along the current view-forward direction by `dist`:
//        P = cg_refdef.vieworg + dist * cg_refdef.viewaxis[0]
//      and stores P back into cg_refdef.vieworg. `dist` is the sole stack arg
//      (the one caller, 0x300468e2, pushes 0xc1980000 = -19.0f, i.e. a 19-unit
//      pull-back along forward — a third-person-style camera offset).
//   3. Recomputes the spin pitch as the pitch (Q3 `vectoangles`) of the forward
//      axis fa:
//        len   = sqrt(fa.x*fa.x + fa.y*fa.y)   (horizontal length)
//        if (len < 1.0f) len = 1.0f            (FCOM 1.0; FNSTSW; TEST AH,5; JP)
//        pitch = atan2(fa.z, len) * -57.2957763671875f   (radians -> -degrees)
//      and stores pitch back into cg_refdefViewAngles[0] (0x30487ac8).
//
// ABI: single float stack arg `dist` at [entry ESP+4]; the disassembler prints it
// as [ESP+0x34] before the three POPs and [ESP+0x28] after them — both resolve to
// the same slot. Caller cleans the arg (ADD ESP,0x4 at 0x300468f4), plain `RET`.
//
// x87 details preserved from the .mcode:
//  - The len<1.0 clamp is the FCOM/FNSTSW/TEST AH,0x5/JP idiom: JP (to skip the
//    clamp) is taken when ST0 >= 1.0 (or unordered); it falls through to
//    FSTP/FLD 1.0f only when len < 1.0f.
//  - FPATAN computes atan2(ST1, ST0); after the FXCH that is atan2(fa.z, len).
//  - The final scale is the .rdata constant at 0x3007be80
//    (bytes e0 2e 65 c2 = 0xc2652ee0 = -57.2957763671875f = -(180/pi)); combining
//    the negative constant with atan2 yields the standard vectoangles pitch,
//    pitch = -atan2(fa.z, len)*180/pi.
//  - 1.0f is the .rdata constant at 0x3007bce0 (bytes 00 00 80 3f).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>

void CG_PerturbCamera(float dist)
{
    vec3_t fa; /* forward axis (AngleVectors "forward" output; the only one used) */
    vec3_t ra; /* right axis  — computed by AngleVectors, unused (scratch)        */
    vec3_t ua; /* up axis     — computed by AngleVectors, unused (scratch)        */

    /*
     * Build the basis from the spin-angle triple at 0x30487ac8. EDX=&angles,
     * ESI=&fa (forward), EDI=&ra (right), EBX=&ua (up). All three output pointers
     * are non-NULL in the machine code, so AngleVectors writes all three, but only
     * `fa` is read back below.
     */
    AngleVectors(cg_refdefViewAngles, fa, ra, ua);

    /*
     * P = cg_refdef.vieworg + dist * cg_refdef.viewaxis[0], written back into
     * cg_refdef.vieworg. (0x300464ac..0x30046515: each component is
     * forward[i]*dist + vieworg[i]; P is stashed on the x87 stack and, after the
     * fa recovery below, stored to 0x30487a90/94/98.)
     */
    long double p0 =
        (long double)cg_refdef.viewaxis[0][0] * dist + cg_refdef.vieworg[0];
    long double p1 =
        (long double)cg_refdef.viewaxis[0][1] * dist + cg_refdef.vieworg[1];
    long double p2 =
        (long double)cg_refdef.viewaxis[0][2] * dist + cg_refdef.vieworg[2];
    float storedP2 = (float)p2;

    /* The compiler recovers the forward components from sums sharing the live
     * P accumulators. Only the Y/Z sums are spilled before subtraction. */
    float recoveredX = (float)(((long double)fa[0] + p0) - p0);
    float faPlusP1 = (float)((long double)fa[1] + p1);
    long double recoveredY = (long double)faPlusP1 - p1;
    float faPlusP2 = (float)((long double)fa[2] + p2);
    float recoveredZ = (float)((long double)faPlusP2 - storedP2);

    cg_refdef.vieworg[0] = (float)p0;
    cg_refdef.vieworg[1] = (float)p1;
    cg_refdef.vieworg[2] = storedP2;

    /*
     * The machine code recovers the pure forward-axis components by forming
     * (fa[i] + p[i]) and then subtracting p[i] again (a compiler artifact of
     * reusing the P accumulators on the x87 stack). Net result is just fa[i], so
     * the horizontal length and pitch are computed directly from the axis.
     */
    long double len = coduo_x87_sqrtl(
        recoveredY * recoveredY +
        (long double)recoveredX * recoveredX);
    if (len < 1.0f) {            /* FCOM 1.0f @0x3007bce0; clamp when len < 1.0f */
        len = 1.0f;
    }

    /* pitch = atan2(fa.z, len) * -(180/pi); constant -57.2957763671875f @0x3007be80 */
    cg_refdefViewAngles[0] = (float)(
        coduo_x87_atan2l(recoveredZ, len) * -57.2957763671875f);
}
