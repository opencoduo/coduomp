// Source: uo_cgame_mp_x86.dll 0x3002e520..0x3002e8ba
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002e520_3002e8ba.mcode
//
// CG_ImpactMark — project a decal (bullet hole / blast scorch) onto world
// surfaces at `origin` along the surface direction `dir`, then either draw it as
// a temporary poly this frame or persist it as a mark for CG_AddMarks to age out.
//
// The .mcode ships a size-matched name guess `VEH_UnlinkPlayer` (matched only by
// byte size 0x39a≈0x39c, which the naming rules forbid). It is REJECTED: this body
// is the cgame mark/decal projector — it gates on cg_addMarks, uses
// VectorNormalize2 / PerpendicularVector / RotatePointAroundVector to build a
// decal quad, clips it via trap_CM_MarkFragments, packs an RGBA color per vertex,
// and emits the result via trap_R_AddPolyToScene or CG_AllocMark. No player-unlink
// behavior is present.
//
// i386 ABI notes (recorded, not source behavior):
//   - `dir` arrives in ECX; all other parameters are cdecl stack args unwound by
//     the caller. The frame is huge (0x9264 bytes: the 1024-vertex projection
//     buffer + 384-fragment buffer) and is probed with _chkstk on entry
//     (MOV EAX,0x9264; CALL 0x30060a30).
//   - A stack cookie (0x30081650) is stashed on entry and re-checked by
//     __security_check_cookie (0x30061639) at the epilogue.
//   - Q_rint (0x3006be3c) consumes its argument on the x87 stack; here it is a
//     plain float->int round of value*255 for each color channel.
//   - trap_CM_MarkFragments (CG_CM_MARKFRAGMENTS) is issued through cgame_syscall
//     with the UO-specific 10-argument shape transcribed exactly in push order
//     below; it returns the fragment count in EAX.

#include "client/cgame/client_recovered.h"

void CG_ImpactMark(qhandle_t markShader, const vec3_t origin, const vec3_t dir, float orientation, float red, float green, float blue,
                   float alpha, qboolean alphaFade, float radius, qboolean temporary, int32_t markLifeTime)
{
    /* NOTE: the final two params are (temporary, markLifeTime) in stack order:
     * the DLL reads the immediate-draw flag from the 2nd-to-last slot (entry+0x28,
     * 0x3002e7ff) and the mark lifetime/duration from the LAST slot (entry+0x2C,
     * 0x3002e558, defaulted to 20000 and stored to mark->duration at 0x3002e868).
     * A prior pass declared them (markLifeTime, temporary), reading both backwards. */
    /* 0x3002e571..0x3002e5df builds one contiguous renderer projection axis:
     * [0] is normalized dir, [2] is the rotated perpendicular, and [1] is then
     * overwritten with [2] x [0]. The syscall passes &axis[0], and the engine
     * reads all three rows through its axis_t MarkFragments parameter. */
    axis_t projectionAxis;          /* [esp+0x1c..0x3f] */
    vec3_t markPoints[4];           /* [esp+0x68..]: the 4 source quad corners     */
    polyVert_t projectedPoints[1024]; /* trap output vertex buffer (maxPoints 0x400;
                                       * indexed with a 32-byte polyVert_t stride)   */
    markFragment_t markFragments[384];/* trap output fragments (maxFragments 0x180)  */
    uint8_t colorInt[4];             /* [esp+0x10]: packed RGBA byte quad           */
    int32_t numFragments;            /* trap_CM_MarkFragments return value          */

    /* 3002e536 / 3002e54b: both marks-subsystem gates. */
    if (cg_marks_vmCvar.integer == 0) {
        return;
    }
    if (cg_suppressMarksGate != 0) {
        return;
    }

    /* 3002e558: a negative lifetime defaults to 20000 ms. */
    if (markLifeTime < 0) {
        markLifeTime = 20000;
    }

    /* 3002e571-3002e57c: normalize `dir` into row 0 (length discarded). */
    VectorNormalize2(dir, projectionAxis[0]);

    /* 3002e57e-3002e584: build the temporary perpendicular in row 1. */
    PerpendicularVector(projectionAxis[1], projectionAxis[0]);

    /* 3002e589-3002e59c: rotate row 1 about row 0 into row 2. */
    RotatePointAroundVector(projectionAxis[2], projectionAxis[0], projectionAxis[1], orientation);

    /* 3002e5a1-3002e5df: overwrite row 1 with row 2 x row 0. */
    projectionAxis[1][0] = projectionAxis[2][2] * projectionAxis[0][1] - projectionAxis[2][1] * projectionAxis[0][2];
    projectionAxis[1][1] = projectionAxis[2][0] * projectionAxis[0][2] - projectionAxis[2][2] * projectionAxis[0][0];
    projectionAxis[1][2] = projectionAxis[2][1] * projectionAxis[0][0] - projectionAxis[2][0] * projectionAxis[0][1];

    /* 3002e5e3-3002e74b: form the four quad corners of side 2*radius, centered on
     * `origin` in the (axis[1], axis[2]) plane. Verified component-by-component
     * against the x87 dataflow for all of x, y and z. */
    for (int c = 0; c < 3; c++) {
        float rc = projectionAxis[1][c] * radius;
        float rr = projectionAxis[2][c] * radius;
        /* origin[c] - rc is spilled to a float slot (FSTP 3002e5f9 / 3002e65f /
         * 3002e6f0) and the ROUNDED value is reloaded for both corner 0 and
         * corner 3 — recomputing it in registers would skip that rounding. */
        float originMinusRc = origin[c] - rc;

        markPoints[0][c] = originMinusRc - rr; /* origin - r*axis1 - r*axis2 */
        markPoints[1][c] = (rc + origin[c]) - rr;
        markPoints[2][c] = (projectionAxis[2][c] + projectionAxis[1][c]) * radius + origin[c];
        markPoints[3][c] = rr + originMinusRc;
    }

    /* 3002e6b9-3002e752: clip the quad against world surfaces. trap returns the
     * fragment count. Args transcribed in exact push order (reversed to C order):
     * (id, numPoints=4, points, origin, axis, radius, maxPoints=1024, pointBuffer,
     * maxFragments=384, fragmentBuffer, markShader). */
    numFragments = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_CM_MARKFRAGMENTS, 4, /* numPoints     */
                                                                 markPoints, /* source quad   */
                                                                 origin, /* projection origin */
                                                                 projectionAxis, /* contiguous basis  */
                                                                 CG_FloatBits(radius), /* radius bits   */
                                                                 1024, /* maxPoints     */
                                                                 projectedPoints, /* point buffer  */
                                                                 384, /* maxFragments  */
                                                                 markFragments, /* frag buffer   */
                                                                 markShader)); /* mark shader   */

    /* 3002e758-3002e7b3: pack (r,g,b,a)*255 rounded to bytes. The trap return in
     * EAX is captured as numFragments before the first Q_rint call. */
    colorInt[0] = (uint8_t)coduo_fp_to_i32_extended((long double)red * (long double)255.0f);
    colorInt[1] = (uint8_t)coduo_fp_to_i32_extended((long double)green * (long double)255.0f);
    colorInt[2] = (uint8_t)coduo_fp_to_i32_extended((long double)blue * (long double)255.0f);
    colorInt[3] = (uint8_t)coduo_fp_to_i32_extended((long double)alpha * (long double)255.0f);

    /* 3002e7b7: nothing to do if the projection produced no fragments. */
    if (numFragments <= 0) {
        return;
    }

    /* 3002e7bd-3002e89d: per-fragment emit loop. */
    for (int32_t i = 0; i < numFragments; i++) {
        markFragment_t *frag = &markFragments[i];
        polyVert_t *verts;
        int32_t numPoints;

        /* 3002e7d0-3002e7d5: clamp to the poly vertex capacity, written back into
         * the fragment record (MOV DWORD PTR [ebx],0xa) so both the emit and the
         * mark copy use the clamped count. */
        if (frag->numPoints > MARK_FRAGMENT_MAX_POINTS) {
            frag->numPoints = MARK_FRAGMENT_MAX_POINTS;
        }
        numPoints = frag->numPoints;
        verts = &projectedPoints[frag->firstPoint];

        /* 3002e7db-3002e7fd: color every vertex of this fragment (one RGBA dword). */
        for (int32_t p = 0; p < numPoints; p++) {
            verts[p].modulate[0] = colorInt[0];
            verts[p].modulate[1] = colorInt[1];
            verts[p].modulate[2] = colorInt[2];
            verts[p].modulate[3] = colorInt[3];
        }

        if (temporary) {
            /* 3002e80a-3002e818: draw immediately as a transient scene poly. */
            cgame_syscall(CG_R_ADDPOLYTOSCENE, frag->shaderHandle, numPoints, verts);
        } else {
            /* 3002e81d-3002e88f: persist as a mark for CG_AddMarks. */
            markPoly_t *mark = CG_AllocMark();

            mark->markTime = cg_time; /* +0x08 */
            mark->duration = markLifeTime; /* +0x170 */
            mark->markShader = frag->shaderHandle; /* +0x0c */
            mark->numVerts = numPoints; /* +0x28 */
            mark->colors[0] = red; /* +0x14 */
            mark->colors[1] = green; /* +0x18 */
            mark->colors[2] = blue; /* +0x1c */
            mark->colors[3] = alpha; /* +0x20 */
            mark->alphaFade = alphaFade; /* +0x10 */

            /* 3002e86e-3002e88f: copy numPoints verts (32 bytes each) into the
             * mark node (REP MOVSD + REP MOVSB over numPoints*0x20 bytes). */
            for (int32_t p = 0; p < numPoints; p++) {
                mark->verts[p] = verts[p];
            }
        }
    }
}
