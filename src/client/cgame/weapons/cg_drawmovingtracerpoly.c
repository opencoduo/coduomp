// Source: uo_cgame_mp_x86.dll 0x30048460..0x300489f6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048460_300489f6.mcode
//
// CG_DrawMovingTracerPoly - submit the moving tracer as a camera-facing ribbon
// plus a camera-facing square centered halfway along it. Original ABI: tail in
// EDI, head and width on the stack. Name is proven by both tracer call sites and
// corroborated by the same-module PPC symbol bank. The VEH_UpdateClient size
// guess is rejected.

#include "client/cgame/client_recovered.h"

void CG_DrawMovingTracerPoly(const vec3_t tailPoint, const vec3_t headPoint,
                             float width)
{
    polyVert_t verts[4];
    vec3_t direction;
    vec3_t side;
    vec3_t center;
    long double dotRight;
    long double dotUp;
    int i;

    direction[0] = headPoint[0] - tailPoint[0];
    direction[1] = headPoint[1] - tailPoint[1];
    direction[2] = headPoint[2] - tailPoint[2];

    /* 0x300484a2..0x300484d6: the multiply/add chains run lane 2, lane 1,
     * then lane 0. Keep that x87 dependency order instead of expressing the
     * algebra in ascending C-array order. */
    dotRight =
        (long double)cg_refdef.viewaxis[1][2] * (long double)direction[2] +
        (long double)cg_refdef.viewaxis[1][1] * (long double)direction[1] +
        (long double)cg_refdef.viewaxis[1][0] * (long double)direction[0];
    dotUp =
        (long double)cg_refdef.viewaxis[2][2] * (long double)direction[2] +
        (long double)cg_refdef.viewaxis[2][1] * (long double)direction[1] +
        (long double)cg_refdef.viewaxis[2][0] * (long double)direction[0];

    /* 0x300484e2/0x300484ff: lanes 0 and 1 spill right*dotUp to float before
     * adding up*(-dotRight). Lane 2 remains live through its final add. */
    {
        float rightPartial0 = (float)(
            (long double)cg_refdef.viewaxis[1][0] * dotUp);
        float rightPartial1 = (float)(
            (long double)cg_refdef.viewaxis[1][1] * dotUp);
        side[0] = (float)(
            (long double)rightPartial0 +
            (long double)cg_refdef.viewaxis[2][0] * -dotRight);
        side[1] = (float)(
            (long double)rightPartial1 +
            (long double)cg_refdef.viewaxis[2][1] * -dotRight);
        side[2] = (float)(
            (long double)cg_refdef.viewaxis[1][2] * dotUp +
            (long double)cg_refdef.viewaxis[2][2] * -dotRight);
    }
    VectorNormalize(side);

    if (sv_night_vmCvar.integer) {
        width = (float)((long double)width *
                        (long double)cg_tracernightscale_vmCvar.value);
    }

    for (i = 0; i < 3; ++i) {
        /* Each side*width product remains live across all four vertex stores;
         * the scratch FSTs at 0x3004861d/672/6b8 do not round its consumers. */
        long double offset = (long double)side[i] * (long double)width;
        verts[0].xyz[i] = (float)((long double)headPoint[i] + offset);
        verts[1].xyz[i] = (float)((long double)headPoint[i] - offset);
        verts[2].xyz[i] = (float)((long double)tailPoint[i] - offset);
        verts[3].xyz[i] = (float)((long double)tailPoint[i] + offset);
    }
    /* Each vertex also copies st into lightmapCoords (+0x14/+0x18): 0x30048572/
     * 0x3004857c store v0.lightmapCoords = v0.st, and likewise for v1..v3. A prior
     * pass wrote only st, leaving lightmapCoords uninitialized stack for the
     * R_AddPolyToScene submission. */
    verts[0].st[0] = 1.0f; verts[0].st[1] = 1.0f;
    verts[0].lightmapCoords[0] = 1.0f; verts[0].lightmapCoords[1] = 1.0f;
    verts[1].st[0] = 1.0f; verts[1].st[1] = 0.0f;
    verts[1].lightmapCoords[0] = 1.0f; verts[1].lightmapCoords[1] = 0.0f;
    verts[2].st[0] = 0.0f; verts[2].st[1] = 0.0f;
    verts[2].lightmapCoords[0] = 0.0f; verts[2].lightmapCoords[1] = 0.0f;
    verts[3].st[0] = 0.0f; verts[3].st[1] = 1.0f;
    verts[3].lightmapCoords[0] = 0.0f; verts[3].lightmapCoords[1] = 1.0f;
    for (i = 0; i < 4; ++i) {
        verts[i].modulate[0] = 255;
        verts[i].modulate[1] = 255;
        verts[i].modulate[2] = 255;
        verts[i].modulate[3] = 255;
    }
    cgame_syscall(CG_R_ADDPOLYTOSCENE, cgs_media_tracerShader, 4, verts);

    for (i = 0; i < 3; ++i) {
        center[i] = (float)((long double)direction[i] * 0.5L +
                            (long double)tailPoint[i]);
    }
    {
        long double plusRight[3];
        long double minusRight[3];
        long double plusUp[3];
        long double minusUp[3];
        float plusRightRounded[3];
        float minusRightRounded[3];
        float plusUpRounded[3];
        float minusUpRounded[3];

        for (i = 0; i < 3; ++i) {
            plusRight[i] =
                (long double)cg_refdef.viewaxis[1][i] * (long double)width +
                (long double)center[i];
            minusRight[i] =
                (long double)cg_refdef.viewaxis[1][i] *
                    -(long double)width +
                (long double)center[i];
            plusUp[i] =
                (long double)cg_refdef.viewaxis[2][i] * (long double)width;
            minusUp[i] =
                (long double)cg_refdef.viewaxis[2][i] *
                    -(long double)width;
            plusRightRounded[i] = (float)plusRight[i];
            minusRightRounded[i] = (float)minusRight[i];
            plusUpRounded[i] = (float)plusUp[i];
            minusUpRounded[i] = (float)minusUp[i];
        }

        /* 0x3004878e..0x300489cc deliberately mixes live x87 values with
         * rounded scratch copies.  The asymmetry below is the exact spill graph,
         * not an algebraic simplification of center +/- right +/- up. */
        verts[0].xyz[0] = (float)(plusUp[0] +
                                  (long double)plusRightRounded[0]);
        verts[0].xyz[1] = (float)(plusUp[1] + plusRight[1]);
        verts[0].xyz[2] = (float)(plusUp[2] + plusRight[2]);

        verts[1].xyz[0] = (float)((long double)plusUpRounded[0] +
                                  (long double)minusRightRounded[0]);
        verts[1].xyz[1] = (float)((long double)plusUpRounded[1] +
                                  minusRight[1]);
        verts[1].xyz[2] = (float)((long double)plusUpRounded[2] +
                                  minusRight[2]);

        verts[2].xyz[0] = (float)(minusUp[0] +
                                  (long double)minusRightRounded[0]);
        verts[2].xyz[1] = (float)(minusUp[1] + minusRight[1]);
        verts[2].xyz[2] = (float)(minusUp[2] +
                                  (long double)minusRightRounded[2]);

        verts[3].xyz[0] = (float)((long double)minusUpRounded[0] +
                                  (long double)plusRightRounded[0]);
        verts[3].xyz[1] = (float)((long double)minusUpRounded[1] +
                                  plusRight[1]);
        verts[3].xyz[2] = (float)((long double)minusUpRounded[2] +
                                  (long double)plusRightRounded[2]);
    }
    /* lightmapCoords = st per vertex again for the second poly (0x30048736/0x30048740
     * store v0.lightmapCoords = v0.st, etc.); the recon wrote only st here too. */
    verts[0].st[0] = 1.0f; verts[0].st[1] = 1.0f;
    verts[0].lightmapCoords[0] = 1.0f; verts[0].lightmapCoords[1] = 1.0f;
    verts[1].st[0] = 1.0f; verts[1].st[1] = 0.0f;
    verts[1].lightmapCoords[0] = 1.0f; verts[1].lightmapCoords[1] = 0.0f;
    verts[2].st[0] = 0.0f; verts[2].st[1] = 0.0f;
    verts[2].lightmapCoords[0] = 0.0f; verts[2].lightmapCoords[1] = 0.0f;
    verts[3].st[0] = 0.0f; verts[3].st[1] = 1.0f;
    verts[3].lightmapCoords[0] = 0.0f; verts[3].lightmapCoords[1] = 1.0f;
    for (i = 0; i < 4; ++i) {
        verts[i].modulate[0] = 255;
        verts[i].modulate[1] = 255;
        verts[i].modulate[2] = 255;
        verts[i].modulate[3] = 255;
    }
    cgame_syscall(CG_R_ADDPOLYTOSCENE, cgs_media_tracerShader, 4, verts);
}
