// Source: uo_cgame_mp_x86.dll 0x3002fe70..0x300302c9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002fe70_300302c9.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>
#include <stdint.h>
#include "compat/coduo_native_x87.h"

/*
 * CG_DrawObjectivePointers (0x3002fe70) — draw the HUD compass/objective markers.
 *
 * Name adjudication: the .mcode header's size-matched "ClientEndFrame" guess is
 * REJECTED. ClientEndFrame is a server per-frame bookkeeping routine that issues no
 * 2D draw traps; this function is pure cgame HUD drawing — it iterates the local
 * player's playerState.objectives[16], and for each active ("current") objective it
 * computes a screen bearing, a distance fade/scale, an elevation-based icon variant,
 * and draws an icon around a compass ring with trap_R_SetColor / trap_R_DrawStretchPic.
 * The behavioral name is used; see client_recovered.h.
 *
 * ABI (proven from the .mcode prologue and the sole use of registers):
 *   - `rect` arrives in EAX (register argument), saved into ESI at 0x3002fe7b. It is
 *     the compass HUD rectDef_t {x,y,w,h} (floats). Reads: rect->x [ESI+0], rect->y
 *     [ESI+4], rect->w [ESI+8], rect->h [ESI+0xc].
 *   - `color` is the one stack argument (loaded into EBP at 0x3002fe75 from
 *     [ESP+0x68]); the incoming r/g/b at color[0..2] are copied into the per-objective
 *     draw color, whose alpha this routine overrides.
 *   Callee-cleaned frame details (SUB ESP,0x5c; push ebx/ebp/esi/edi; ADD ESP,0x5c;
 *   RET) are i386 calling-convention artifacts and are not modeled as source behavior.
 *
 * Constants (all read as float ptr / double ptr from .rdata; decoded from the exact
 * addresses, not inferred): 0x3048c4a8 = cg_hudCompassSize_vmCvar.value (a HUD slide/scale float);
 * 0.5f (0x3007bce8), 1.0f (0x3007bce0), 160.0f (0x3007be90), 20.0f (0x3007be04),
 * 182.04444885f = 65536/360 ANGLE2SHORT scale (0x3007bd60), 0.0054931640625f =
 * 360/65536 SHORT2ANGLE scale (0x3007bd5c), 0.0f (0x3007bcec), pi 3.14159274f
 * (0x3007bd88), 180.0f (0x3007bd50), and the double 43.75 (0x3007c1e8).
 * cgs_screenXScale/cgs_screenYScale (0x30447aa4/0x30447aa8) scale virtual-640x480 draw
 * coordinates to the real framebuffer. cg_refdef.vieworg (0x30487a90) is the view origin.
 */

/* ANGLE2SHORT / SHORT2ANGLE scale factors (Quake3/CoD): a full turn maps to 65536
 * 16-bit angle units. Written in natural form; the .rdata values are the float
 * constants 65536/360 and 360/65536. */
enum { OBJPTR_ANGLE2SHORT_MASK = 0xffff };
#define OBJPTR_ANGLE2SHORT_SCALE (182.04444885253906f) /* 65536.0f / 360.0f */
#define OBJPTR_SHORT2ANGLE_SCALE (0.0054931640625f)    /* 360.0f / 65536.0f */
#define OBJPTR_DEG2RAD_PI        (3.1415927410125732f)  /* 0x3007bd88 (pi, float) */
#define OBJPTR_DEG2RAD_180       (180.0f)               /* 0x3007bd50 */
#define OBJPTR_RING_RADIUS_SCALE (20.0f)                /* 0x3007be04: ring radius = slide*20 */
#define OBJPTR_ICON_SIZE_SCALE   (43.75)                /* 0x3007c1e8 = 0x4045e00000000000
                                                         * (DOUBLE 43.75; FMUL QWORD at
                                                         * 0x30030167 -- must stay unsuffixed) */
#define OBJPTR_CENTER_BIAS       (160.0f)               /* 0x3007be90: vertical center bias */

/* Icon direction-variant index passed to CG_GetObjectiveShaderForDir, chosen from the
 * objective's elevation delta (dz) relative to the view: level/up/down. Exact source
 * enum unproven; named by proven role. */
enum {
    OBJPTR_DIR_LEVEL = 0,
    OBJPTR_DIR_UP    = 1, /* dz > cg_hudObjectiveMaxHeight_vmCvar.value */
    OBJPTR_DIR_DOWN  = 2  /* dz < cg_hudObjectiveMinHeight_vmCvar.value */
};

void CG_DrawObjectivePointers(const rectDef_t *rect, const vec3_t color)
{
    /* 0x3002fe7d: advance the smoothed compass reference yaw for this frame. */
    CG_UpdateCompassOrientation();

    /* 0x3002fe82..0x3002fed7: compass ring center in virtual screen space, derived
     * from the HUD rect and the current slide fraction.
     *   centerX = cg_hudCompassSize_vmCvar.value * rect->w * 0.5f + rect->x
     *   centerY = cg_hudCompassSize_vmCvar.value * rect->h * 0.5f + rect->y
     *   centerYBiased = centerY - (cg_hudCompassSize_vmCvar.value - 1.0f) * 160.0f
     * (the FSUBP at 0x3002fed5 subtracts the (slide-1)*160 term from centerY). */
    const float centerX = (float)(
        (long double)cg_hudCompassSize_vmCvar.value *
        (long double)rect->w * 0.5L + (long double)rect->x);
    const long double centerYAnchor =
        (long double)cg_hudCompassSize_vmCvar.value *
        (long double)rect->h * 0.5L + (long double)rect->y;
    const long double centerYSlide =
        ((long double)cg_hudCompassSize_vmCvar.value - 1.0L) *
        (long double)OBJPTR_CENTER_BIAS;
    const float centerY = (float)(centerYAnchor - centerYSlide);

    /* 0x3002fe8b..0x3002feab: gate on the local player's animation state being valid
     * (bgs.clientinfo[cg_snap->ps.psClientNum].infoValid != 0). ECX = cg_snap is
     * reloaded each iteration; the objectives array lives in the snapshot. */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = cg_snap->ps.psClientNum;
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_DrawObjectivePointers: invalid client number %i",
                  clientNum);
        return;
    }
    if (bgs.clientinfo[clientNum].infoValid == 0)
        return; /* 0x3002fedb JZ 0x300302c1 */

    /* Register the draw color once; the RGB comes from the caller, alpha is per-marker.
     * EBX = 0x3f800000 = 1.0f (used as the near-distance alpha and as the s2/t2 texcoord
     * of the icon quad). */

    for (int i = 0; i < PLAYERSTATE_OBJECTIVE_COUNT; ++i) {
        objective_t *obj = &cg_snap->ps.objectives[i];

        /* 0x3002fef0..0x3002ff01: only "current" objectives are drawn. */
        if (obj->state != OBJECTIVE_STATE_CURRENT)
            continue;

        /* 0x3002ff07..0x3002ff51: the objective's world position. When it tracks an
         * entity (entityNum != 0x3ff) use that entity's interpolated position from the
         * entity pool (cg_entities[entityNum].lerpOrigin vec3 at +0x208, which this
         * consumer treats as the tracked entity's world origin); otherwise use the
         * objective's own static origin. */
        vec3_t objPos;
        if (obj->entityNum == ENTITYNUM_NONE) {
            objPos[0] = obj->origin[0];
            objPos[1] = obj->origin[1];
            objPos[2] = obj->origin[2];
        } else {
            /* IMUL entityNum,0x288 + base 0x3048c6e0 == cg_entities[entityNum]. */
            const centity_t *ent =
                cg_entities + obj->entityNum;
            objPos[0] = ent->lerpOrigin[0]; /* [ent+0x208] */
            objPos[1] = ent->lerpOrigin[1]; /* [ent+0x20c] */
            objPos[2] = ent->lerpOrigin[2]; /* [ent+0x210] */
        }

        /* 0x3002ff51..0x3002ffc3: the reference point the bearing/distance are measured
         * from. Normally the view origin (cg_refdef.vieworg); when the local player is
         * following another entity in first person (psEFlags bit 0x100000) and that
         * followed entity is valid, measure relative to the followed entity's position
         * from the same entity pool instead. */
        vec3_t dir;
        if ((cg_snap->ps.entityStateFlags & EF_IN_VEHICLE) &&
            cg_snap->ps.viewLockedEntityNum != ENTITYNUM_NONE) {
            const int32_t followedEntityNum =
                cg_snap->ps.viewLockedEntityNum;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            if ((uint32_t)followedEntityNum >=
                (uint32_t)MAX_GENTITIES) {
                Com_Error(ERR_DROP,
                          "\x15" "CG_DrawObjectivePointers: invalid "
                          "view-lock entity %i",
                          followedEntityNum);
                return;
            }
            const centity_t *followed =
                &cg_entities[followedEntityNum];
            dir[0] = objPos[0] - followed->lerpOrigin[0]; /* [followed+0x208] */
            dir[1] = objPos[1] - followed->lerpOrigin[1];
            dir[2] = objPos[2] - followed->lerpOrigin[2];
        } else {
            dir[0] = objPos[0] - cg_refdef.vieworg[0]; /* 0x30487a90 */
            dir[1] = objPos[1] - cg_refdef.vieworg[1]; /* 0x30487a94 */
            dir[2] = objPos[2] - cg_refdef.vieworg[2]; /* 0x30487a98 */
        }

        /* 0x3002ffcb..0x3002fffc: screen-relative bearing. Take the world yaw of the
         * horizontal direction, subtract the compass reference yaw, fold through
         * ANGLE2SHORT (round + mask to 16 bits) then SHORT2ANGLE to normalize into
         * [0,360). vectoyaw reads dir[0],dir[1]. */
        /* 0x3002ffcb CALL vectoyaw; 0x3002ffd0 FSUB cg_compassRefYaw; 0x3002ffd6
         * FMUL ANGLE2SHORT_SCALE; 0x3002ffdc CALL _ftol2 -- the whole product is one
         * 80-bit chain fed RAW to _ftol2, with NO intermediate store. Kept as a single
         * expression so no float rounding is inserted before the truncation (a `float
         * rawYaw` local would round the subtraction the DLL leaves in st(0)). */
        uint32_t shortAngle =
            (uint32_t)coduo_fp_to_i32_extended(
                ((long double)vectoyaw(dir) -
                 (long double)cg_compassRefYaw) *
                (long double)OBJPTR_ANGLE2SHORT_SCALE)
            & OBJPTR_ANGLE2SHORT_MASK;
        float bearingDeg = (float)(int32_t)shortAngle * OBJPTR_SHORT2ANGLE_SCALE;

        /* 0x30030000..0x30030028: horizontal distance to the objective, sqrt(dx^2+dy^2).
         * (The FADDP order is dy^2 + dx^2; the sum is identical.) */
        float distance = (float)__builtin_sqrtl(
            (long double)dir[1] * (long double)dir[1] +
            (long double)dir[0] * (long double)dir[0]);

        /* 0x3003002c..0x300300c2: marker alpha, faded by distance. Near/full at
         * cg_objectivePointerNearDist (0x304219c8) and below; clamped to
         * cg_objectivePointerMinAlpha (0x3044fcc8) at cg_objectivePointerFarDist
         * (0x304517c8) and beyond; linearly interpolated between. */
        float drawColor[4];
        drawColor[0] = color[0]; /* [EBP+0] */
        drawColor[1] = color[1]; /* [EBP+4] */
        drawColor[2] = color[2]; /* [EBP+8] */

        const float nearDist = cg_hudCompassMaxRange_vmCvar.value;
        const float farDist  = cg_hudObjectiveMaxRange_vmCvar.value;
        const float minAlpha = cg_hudObjectiveMinAlpha_vmCvar.value;
        if (distance <= nearDist) {
            drawColor[3] = 1.0f; /* 0x30030049 */
        } else if (distance >= farDist) {
            drawColor[3] = minAlpha; /* 0x30030065 */
        } else {
            float alphaFrac = 0.0f; /* 0x30030071 */
            float denom = farDist - nearDist; /* 0x3003007f (FLD far; FSUB near) */
            if (denom != 0.0f) /* 0x30030083..0x30030094 FUCOMPP guard */
                alphaFrac = (float)(
                    ((long double)distance - (long double)nearDist) /
                    (long double)denom); /* 0x30030096..0x300300a4 */
            /* 0x300300a8: 1.0 - (1.0 - minAlpha) * alphaFrac. */
            drawColor[3] = (float)(1.0L -
                (1.0L - (long double)minAlpha) *
                (long double)alphaFrac);
        }

        /* 0x300300c2..0x3003014d: marker size scale, also distance-driven. At/above
         * cg_objectivePointerMaxSizeDist (0x30452548) the scale is
         * cg_objectivePointerMaxSize (0x30456568); at/below nearDist it is 1.0; between,
         * interpolated. */
        const float maxSizeDist = cg_hudCompassMinRange_vmCvar.value;
        const float maxSize     = cg_hudCompassMinRadius_vmCvar.value;
        float size;
        if (distance <= maxSizeDist) {
            size = maxSize; /* 0x300300d9 */
        } else if (distance >= nearDist) {
            size = 1.0f; /* 0x300300f0 */
        } else {
            float sizeFrac = 0.0f; /* 0x300300fc */
            float denom2 = nearDist - maxSizeDist; /* 0x300300f6..0x3003010a */
            if (denom2 != 0.0f) /* 0x3003010e..0x3003011f FUCOMPP guard */
                sizeFrac = (float)(
                    ((long double)nearDist - (long double)distance) /
                    (long double)denom2); /* 0x30030121..0x3003012f */
            /* 0x30030133: 1.0 - (1.0 - maxSize) * sizeFrac. */
            size = (float)(1.0L -
                (1.0L - (long double)maxSize) *
                (long double)sizeFrac);
        }

        /* 0x3003014d..0x3003016d: icon half-size in virtual pixels. The trailing
         * scale enters as a DOUBLE (FMUL QWORD [0x3007c1e8] @0x30030167), so the
         * chain widens to double before the single FSTP DWORD [0x14] @0x3003016d.
         * Do NOT narrow the constant to float -- the operand width is load-bearing. */
        float iconSize = (float)(
            (long double)cg_hudCompassSize_vmCvar.value *
            (long double)size * (long double)OBJPTR_ICON_SIZE_SCALE);

        /* 0x30030171..0x30030197: sin/cos of the bearing (converted to radians). The
         * FSINCOS leaves cos on top, sin below; the code stores cos then sin. */
        float bearingRad = (float)(
            (long double)bearingDeg * (long double)OBJPTR_DEG2RAD_PI /
            (long double)OBJPTR_DEG2RAD_180);
        float cosB, sinB;
        coduo_x87_sincosf(bearingRad, &sinB, &cosB);

        /* 0x3003019b..0x300301eb: place the icon on the compass ring. The ring radius R
         * is slide*20; the icon quad's top-left corner sits at
         *   iconX = (centerX  - R*0.5f) - sinB * iconSize
         *   iconY = (centerYB - R*0.5f) - cosB * iconSize   */
        float ringRadius = cg_hudCompassSize_vmCvar.value *
                           OBJPTR_RING_RADIUS_SCALE;
        float iconX = (float)(
            ((long double)centerX - (long double)ringRadius * 0.5L) -
            (long double)sinB * (long double)iconSize);
        float iconY = (float)(
            ((long double)centerY - (long double)ringRadius * 0.5L) -
            (long double)cosB * (long double)iconSize);

        /* 0x300301ef..0x3003022d: choose the icon direction variant from the elevation
         * delta dz, then resolve the shader.
         *   dz >  cg_hudObjectiveMaxHeight_vmCvar.value (0x30456448) -> DIR_UP
         *   dz <  cg_hudObjectiveMinHeight_vmCvar.value (0x3044fba8) -> DIR_DOWN
         *   otherwise                                            -> DIR_LEVEL
         * The FCOMP/TEST-AH idioms: `dz > hi` is (not (dz <= hi)); `dz < lo` is C0 set.
         * obj->icon is passed as the shader-index argument in every branch. */
        float dz = dir[2];
        int dir_variant;
        if (dz > cg_hudObjectiveMaxHeight_vmCvar.value) {
            dir_variant = OBJPTR_DIR_UP; /* 0x30030200..0x30030209 */
        } else if (dz < cg_hudObjectiveMinHeight_vmCvar.value) {
            dir_variant = OBJPTR_DIR_DOWN; /* 0x3003021c..0x30030225 */
        } else {
            dir_variant = OBJPTR_DIR_LEVEL; /* 0x30030227..0x3003022b */
        }
        qhandle_t shader = CG_GetObjectiveShaderForDir(dir_variant, obj->icon);

        /* 0x30030235..0x3003023e: set the 2D draw color (trap 72) to the faded marker
         * color, then draw. */
        trap_R_SetColor(drawColor);

        /* 0x30030244..0x30030297: draw the icon quad, scaling the virtual-640x480
         * coordinates to the real framebuffer. Position uses iconX/iconY; the quad is
         * the full ring square (R x R). Texcoords are (s1,t1,s2,t2) = (0,0,1,1). */
        trap_R_DrawStretchPic(CG_FloatBits(cgs_screenXScale * iconX),      /* x */
                              CG_FloatBits(cgs_screenYScale * iconY),      /* y */
                              CG_FloatBits(cgs_screenXScale * ringRadius), /* w */
                              CG_FloatBits(cgs_screenYScale * ringRadius), /* h */
                              CG_FloatBits(0.0f),                          /* s1 */
                              CG_FloatBits(0.0f),                          /* t1 */
                              CG_FloatBits(1.0f),                          /* s2 */
                              CG_FloatBits(1.0f),                          /* t2 */
                              shader);                                         /* hShader */
    }

    /* 0x300302b4..0x300302be: reset the 2D draw color to opaque white (trap 72, NULL). */
    trap_R_SetColor((const float *)0);
}
