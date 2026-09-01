// Source: uo_cgame_mp_x86.dll 0x30032da0..0x30032fd2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032da0_30032fd2.mcode

#include "client/cgame/client_recovered.h"

/*
 * CG_AddPlayerWaterShadow (0x30032da0) — draw the flat "water-line" shadow decal on
 * the water surface underneath a player/vehicle entity that is standing in water.
 *
 * The .mcode-assigned name "MatrixMultiplyEquals" is REJECTED: it is a pure win/PPC
 * size match (win size 0x232 == some game_mp_uo symbol of the same size) with zero
 * behavioral basis. There is NO matrix arithmetic here (no 3x3/4x4 FMUL/FADD
 * accumulation); the body issues cgame system calls (point-contents probes, a trace,
 * and trap_R_AddPolyToScene) and builds a screen-flat textured quad. The prior
 * caller-observed provisional name "CG_AddPlayerHeatEffect" is likewise a guess and is
 * superseded by this behavioral name. It is the water sibling of CG_PlayerShadow
 * (0x30032c20): same shadow-mode gate, but it uses the separately registered
 * `wake` material and targets a horizontal water surface instead of the ground.
 *
 * Register ABI (proven from the CG_Player caller at 0x300344df, which leaves `cent`
 * live in ESI): ESI = cent (centity_t *, an incoming register arg). No stack args;
 * the EAX result is unused by the caller. /GS-guarded (frame cookie saved from
 * __security_cookie [0x30081650] and checked by __security_check_cookie (0x30061639)
 * before RET).
 *
 * Gating global cg_shadows_vmCvar.integer (0x3044fdec) is the shadow-mode flag
 * (idTech cg_shadows): the whole effect is skipped when it is zero. (Its label
 * "g_isvehicleusable" is a size-match guess, not its real name — same global gates
 * CG_PlayerShadow.)
 *
 * Behaviour, proven step by step from the machine code:
 *   1. If shadow mode (0x3044fdec) == 0 -> nothing.                       (30032db2 TEST;JZ)
 *   2. Point-contents probe 24u BELOW the entity origin:
 *        cgame_syscall(CG_CM_POINT_CONTENTS, &{wa[0],wa[1],wa[2]-24}, 0)  (30032dbf..30032dec)
 *      Require the returned contents mask to have CONTENTS_WATER (0x20) SET, i.e. the
 *      point below the entity is in water; else nothing.                  (30032df5 TEST AL,0x20;JZ)
 *   3. Point-contents probe 32u ABOVE the entity origin:
 *        cgame_syscall(CG_CM_POINT_CONTENTS, &{wa[0],wa[1],wa[2]+32}, 0)  (30032dfd..30032e2a)
 *      Require the returned mask to have (CONTENTS_WATER|CONTENTS_SOLID = 0x21) CLEAR,
 *      i.e. the point above is open air (not underwater, not inside a brush); else
 *      nothing.                                                           (30032e33 TEST AL,0x21;JNZ)
 *      (2)+(3) together detect that the entity straddles a water surface.
 *   4. Trace 0x26 (CG_CM_BOX_TRACE) from the above-point down to the below-point to locate
 *      the exact water-surface hit:
 *        cgame_syscall(CG_CM_BOX_TRACE, &trace, &above, &below, 0, 0, 0, 0x20) (30032e3b..30032e54)
 *      If trace.fraction == 1.0 (reached the far end without a surface hit) -> nothing.
 *                                                                         (30032e5a..30032e6e)
 *   5. Build a 64x64 (+-32 in X and Y) axis-aligned HORIZONTAL quad on the surface,
 *      all four corners at the trace hit Z (trace.endpos[2], copied as a raw dword so
 *      every vertex shares the identical surface height), textured with the unit UVs
 *      (0,0)/(0,1)/(1,1)/(1,0) and colored opaque white (modulate = 0xffffffff):
 *        v0 = { ex-32, ey-32, ez }  st (0,0)
 *        v1 = { ex-32, ey+32, ez }  st (0,1)
 *        v2 = { ex+32, ey+32, ez }  st (1,1)
 *        v3 = { ex+32, ey-32, ez }  st (1,0)
 *      where (ex,ey,ez) = trace.endpos.                                   (30032e74..30032f37)
 *   6. Submit the quad:
 *        cgame_syscall(CG_R_ADDPOLYTOSCENE, cgs.media.wake, 4, verts)
 *                                                                         (30032f3e..30032fbc)
 *
 * .rdata / immediate constants (dumped exact via objdump -s -j .rdata):
 *   0x3007bdd4 = 24.0f   (below-probe Z drop)
 *   0x3007bdd0 = 32.0f   (above-probe Z rise AND the +-32 quad half-extent)
 *   0x3007bcf8 = 1.0     (double; trace.fraction "no-hit" sentinel)
 *   0x3f800000 = 1.0f    (unit texcoord)
 *   0xff bytes           (packed RGBA 0xffffffff = opaque white per vertex)
 *
 * Callee (provisional decl; reuse name only, arity/types re-derived from these bytes):
 *   cgame_syscall (indirect through [0x30085e9c]).
 *
 * The two point-contents probes use CG_CM_POINT_CONTENTS (0x24): the same id CG_Player's mark
 * pipeline (0x30035420) issues as a point-query; here it returns the CONTENTS_* mask of
 * the probed point. The trailing 0x20 handle passed to the trace matches the shape at
 * the other CG_CM_BOX_TRACE sites.
 */

/* Contents bits read back from the point-contents probes. CONTENTS_WATER (0x20) is the
 * shared define; the SOLID bit (0x1) has no corpus define yet, so it is spelled out. */
#define CG_WATER_SHADOW_CONTENTS_SOLID 0x00000001u

void CG_AddPlayerWaterShadow(centity_t *cent)
{
    vec3_t     probeBelow;   /* origin, 24u down: must be in water   */
    vec3_t     probeAbove;   /* origin, 32u up:   must be in air      */
    trace_t trace; /* water-surface trace result           */
    polyVert_t verts[4];     /* the flat surface quad                 */
    int32_t    contentsBelow;
    int32_t    contentsAbove;

    if (cg_shadows_vmCvar.integer == 0)
        return;

    /* Probe just below the entity origin: require water there. */
    probeBelow[0] = cent->lerpOrigin[0];
    probeBelow[1] = cent->lerpOrigin[1];
    probeBelow[2] = cent->lerpOrigin[2] - 24.0f;
    contentsBelow = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_CM_POINT_CONTENTS, (intptr_t)probeBelow, 0));
    if ((contentsBelow & (int32_t)CONTENTS_WATER) == 0)
        return;

    /* Probe above the entity origin: require open air (no water, no solid) there. */
    probeAbove[0] = cent->lerpOrigin[0];
    probeAbove[1] = cent->lerpOrigin[1];
    probeAbove[2] = cent->lerpOrigin[2] + 32.0f;
    contentsAbove = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_CM_POINT_CONTENTS, (intptr_t)probeAbove, 0));
    if ((contentsAbove &
         (int32_t)(CONTENTS_WATER | CG_WATER_SHADOW_CONTENTS_SOLID)) != 0)
        return;

    /* Trace from the air point down to the water point to find the surface hit. */
    cgame_syscall(CG_CM_BOX_TRACE, (intptr_t)&trace,
                  (intptr_t)probeAbove,
                  (intptr_t)probeBelow, 0, 0, 0, 0x20);

    /* A full 1.0 fraction means the trace reached the far end with no surface hit. */
    if (trace.fraction == doubleOne)
        return;

    /* Four corners of a 64x64 horizontal quad centered on the surface hit, sharing the
     * hit Z. The Z is copied as the raw endpos dword into each vertex (bit-identical). */
    verts[0].xyz[0] = trace.endpos[0] - 32.0f;
    verts[0].xyz[1] = trace.endpos[1] - 32.0f;
    verts[0].xyz[2] = trace.endpos[2];
    verts[0].st[0] = 0.0f;
    verts[0].st[1] = 0.0f;

    verts[1].xyz[0] = trace.endpos[0] - 32.0f;
    verts[1].xyz[1] = trace.endpos[1] + 32.0f;
    verts[1].xyz[2] = trace.endpos[2];
    verts[1].st[0] = 0.0f;
    verts[1].st[1] = 1.0f;

    verts[2].xyz[0] = trace.endpos[0] + 32.0f;
    verts[2].xyz[1] = trace.endpos[1] + 32.0f;
    verts[2].xyz[2] = trace.endpos[2];
    verts[2].st[0] = 1.0f;
    verts[2].st[1] = 1.0f;

    verts[3].xyz[0] = trace.endpos[0] + 32.0f;
    verts[3].xyz[1] = trace.endpos[1] - 32.0f;
    verts[3].xyz[2] = trace.endpos[2];
    verts[3].st[0] = 1.0f;
    verts[3].st[1] = 0.0f;

    /* Opaque white modulate on every vertex (four 0xff bytes = 0xffffffff). */
    for (int32_t i = 0; i < 4; ++i) {
        verts[i].modulate[0] = 0xff;
        verts[i].modulate[1] = 0xff;
        verts[i].modulate[2] = 0xff;
        verts[i].modulate[3] = 0xff;
    }

    cgame_syscall(CG_R_ADDPOLYTOSCENE,
                  (int32_t)cgs_media_wakeMarkShader, 4,
                  (intptr_t)verts);
}
