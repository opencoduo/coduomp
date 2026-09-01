// Source: uo_cgame_mp_x86.dll 0x30031e20..0x30031f6d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031e20_30031f6d.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_DrawTankBarrel — the world-tag member of the
 * 0x30031cb0..0x30032042 HUD-tag draw family. Where the fixed-rect siblings
 * (CG_DrawJeepBody 0x30031cb0 / CG_DrawTankBody 0x30031d50 /
 * CG_DrawTankPositionStatus 0x30031f70) draw a tag quad at a rect corner, this one
 * draws a turret icon at the currently-processed vehicle entity's *world*
 * "tag_turret" bone position: it resolves the DObj bone world matrix, projects the
 * tag matrix to Euler angles, and submits a rotated icon quad via the trap-0x4c draw.
 *
 * Name adjudication: the .mcode header's size-matched "Item_ValidateTypeData" guess
 * is REJECTED. There is no item lookup, no type validation, and no item struct
 * access anywhere in the body; the function gates on the SAME predicted-player
 * entityStateFlags bits and the SAME entity->currentState.eType == 12 (ET_VEHICLE) / stateFilter
 * gates as its proven HUD-tag-draw siblings, then does DObj tag resolution + 2D
 * projection + a trap-0x4c rotated-quad draw. It is byte-structurally the world-tag
 * cousin of those siblings. The Mac CG_DrawTankBarrel performs the corresponding
 * DObj world-tag, matrix-to-angles, color, and rotated-quad sequence, resolving
 * the source name.
 *
 * ---- Register / stack ABI (frame base E = ESP after `SUB ESP,0x58`; retaddr at E+0x58) ----
 *   EDI = rect, a float[4] {x,y,w,h}  (register argument; read at [EDI], [EDI+4],
 *         [EDI+8], [EDI+0xc]; never loaded from a stack slot in this body).
 *   ECX = stateFilter to match (register argument). When 0 the stateFilter compare
 *         is skipped (TEST ECX,ECX; JZ 0x30031e6b).
 *   [E+0x5c] = arg0 = hShader  (forwarded as the last arg of the trap-0x4c draw)
 *   [E+0x60] = arg1 = color    (const float rgba[4]; passed to trap_R_SetColor)
 * ESI holds the entity pointer across the body (PUSH ESI at 0x30031e3e / POP ESI at
 * 0x30031f68); EBX holds the DObj handle (PUSH EBX at 0x30031e6d / POP EBX at
 * 0x30031f67). Non-cleaned callee argument pushes (AngleSubtract's two dwords, the
 * trap-0x48 arg, the five trap-0x4c args, and the final trap-0x48 pair) are all
 * folded into the single `ADD ESP,0x28` at 0x30031f64 before the register restores.
 *
 * ---- Gates (each failure exits via the shared epilogue) ----
 *   0x30031e28  TEST flags,0x100000 ; JZ  end   -> EF_IN_VEHICLE must be set
 *   0x30031e33  TEST flags,0x400000 ; JNZ end   -> EF_VEHICLE_ALLOW_WEAPON must be clear
 *   0x30031e51  CMP  entity->currentState.eType,0xc ; JNZ    -> eType == 12 (ET_VEHICLE)
 *   0x30031e5b  TEST ECX,ECX ; JZ  ; CMP entity->currentState.stateFilter,ECX ; JNZ
 *                                              -> filter==0 || entity->currentState.stateFilter==filter
 *   0x30031e7f  TEST EBX,EBX ; JZ            -> DObj handle (trap 0xa5) must be nonzero
 *   0x30031ea2  TEST EAX,EAX ; JZ           -> tag world-matrix lookup must succeed
 *
 * entity = &cg_entities[cg_predictedPlayerState.viewLockedEntityNum] via IMUL 0x288 + base 0x3048c6e0
 * (cg_entities modeled through the existing centity_t view of that base array).
 *
 * ---- slidX float chain (0x30031ecc..0x30031ee5), the shared HUD slide idiom ----
 *   FLD cg_hudCompassSize_vmCvar.value(0x3048c4a8); FSUB 1.0f(0x3007bce0);
 *   FMUL 112.0f(0x3007c1e0); FADD rect[0]([EDI]) -> slidX (stored at E+0x8).
 *
 * ---- corner-offset quad (0x30031ee9..0x30031f30) ----
 * A four-corner {x,y} offset table for the icon quad, built from rect[2] (w) and
 * rect[3] (h) with the .rdata scale constants 0.5f(0x3007bce8), 0.25f(0x3007be58),
 * 0.75f(0x3007be38). The FPU stack is reused so each of the four corners repeats the
 * two half/quarter offsets:
 *   c[0] = -(w*0.5f)   c[1] =  (h*0.25f)     (FST +0x2c / MOV +0x48 <- +0x14; FSTP +0x34)
 *   c[2] = -(w*0.5f)   c[3] = -(h*0.75f)     (FSTP +0x30; FST +0x38)
 *   c[4] =  (w*0.5f)   c[5] = -(h*0.75f)     (FSTP +0x3c; FSTP +0x40)
 *   c[6] =  (w*0.5f)   c[7] =  (h*0.25f)     (FSTP +0x44; +0x34 already holds h*0.25f)
 * The buffer's base (E+0x18) is handed to the draw callee via LEA EDX,[ESP+0x40].
 *
 * ---- draw sequence ----
 *   0x30031f34  trap_R_SetColor(color)                         (via wrapper 0x3003e0d0)
 *   0x30031f55  CG_DrawTurretTagQuad(&corners, slidX, rect[1],
 *                    cg_turretTagShaderParams, angleResult, hShader)   (trap 0x4c)
 *   0x30031f5e  trap_R_SetColor(NULL)                          (reset draw color)
 */

/* eType value that enables the HUD entity tag: ET_VEHICLE (12), proven by the
 * CMP [entity+0x4],0xc gate and shared with the tag-family siblings. */
enum { CG_TAG_ENTITY_ETYPE = 12 };

void CG_DrawTankBarrel(const rectDef_t *rect, int32_t stateFilter,
                       int32_t hShader,
                               const float *color)
{
    /* 0x30031e20..0x30031e38: gate on the local predicted player's entityStateFlags. */
    uint32_t flags = cg_predictedPlayerState.entityStateFlags;
    if ((flags & EF_IN_VEHICLE) == 0)
        return;
    if ((flags & EF_VEHICLE_ALLOW_WEAPON) != 0)
        return;

    int32_t entityNum = cg_predictedPlayerState.viewLockedEntityNum;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_DrawTankBarrel: invalid view-lock entity %i",
                  entityNum);
        return;
    }
    centity_t *entity = &cg_entities[entityNum];
    if (entity->currentState.eType != CG_TAG_ENTITY_ETYPE)
        return;

    /* 0x30031e5b..0x30031e65: when a nonzero filter is passed, the entity's
     * stateFilter must match it. filter == 0 skips this check (TEST ECX,ECX; JZ). */
    if (stateFilter != 0 && entity->currentState.stateFilter != stateFilter)
        return;

    /* 0x30031e6b..0x30031e81: obtain the entity's live DObj handle from the engine
     * (trap 0xa5, one arg = currentState.number). A zero handle means no model. */
    void *dobj = (void *)(intptr_t)cgame_syscall(
        CG_DOBJ_GET_HANDLE, entity->currentState.number);
    if (dobj == NULL)
        return;

    /* 0x30031e87..0x30031e88: fill in the entity's interpolated weapon/aim angles
     * (feeds the tag world-matrix build below). */
    CG_CalcEntityLerpPositions(entity);

    /* 0x30031e8d..0x30031ea4: resolve the "tag_turret" bone's world matrix on the
     * entity's DObj skeleton. The call site loads EAX = "tag_turret" (the tag name,
     * 0x300771ec) and ECX = the DObj handle; the entity is the stack argument and
     * `tagWorldMatrix` (E+0x18) receives the composed DObjSkelMat. A zero
     * (qfalse) return means the tag is absent, and the draw is skipped. (EAX is the
     * tagName argument the callee forwards to trap(0xb2), proven from its body.) */
    DObjSkelMat tagWorldMatrix;
    if (!CG_DObjGetWorldTagMatrix(dobj, "tag_turret",
                                         entity, &tagWorldMatrix))
        return;

    /* 0x30031eaa..0x30031eb2: convert the resolved tag matrix to Euler angles.
     * EAX = the output vector and ECX = the tag world matrix. */
    vec3_t tagAngles;
    Axis4ToAngles(&tagWorldMatrix, tagAngles);

    /* 0x30031eb7..0x30031ec8: rotation term = AngleSubtract(cg_refdefViewAngles[1], the
     * tag yaw). MOV ECX,[ESP+0x18] loads tagAngles[1] (scratchA is based at
     * [ESP+0x14]=tagAngles[0], so [ESP+0x18] is +4), MOV EDX,[0x30487acc] loads
     * cg_refdefViewAngles[1]; PUSH ECX; PUSH EDX gives the cgame argument order
     * (a=cg_refdefViewAngles[1], b=tagAngles[1]). */
    float angleResult = AngleSubtract(cg_refdefViewAngles[1], tagAngles[1]);

    /* 0x30031ecc..0x30031ee5: slid X, the shared HUD slide idiom, computed from
     * cg_hudCompassSize_vmCvar.value (0x3048c4a8), 1.0f (0x3007bce0), 112.0f (0x3007c1e0). */
    float slidX = (float)(
        ((long double)cg_hudCompassSize_vmCvar.value - 1.0L) * 112.0L +
        (long double)rect->x);

    /* 0x30031ee9..0x30031f30: build the icon quad's four {x,y} corner offsets from
     * the rect width (rect[2]) and height (rect[3]). 0.5f (0x3007bce8), 0.25f
     * (0x3007be58) and 0.75f (0x3007be38) are the .rdata scale constants; the FPU
     * stack reuse produces the repeated half/quarter offsets recorded above. */
    /* Each product remains in an x87 register until its destination FST/FSTP.
     * In particular, FCHS precedes the stores of the negative lanes; rounding a
     * positive temporary to float first would differ under a directed x87
     * rounding mode. */
    long double halfW = (long double)rect->w * (long double)0.5f;
    long double quarterH = (long double)rect->h * (long double)0.25f;
    long double threeQuarterH =
        (long double)rect->h * (long double)0.75f;

    float cornerOffsets[8];
    cornerOffsets[0] = (float)-halfW;         /* +0x18 (FST [ESP+0x2c]) */
    cornerOffsets[1] = (float)quarterH;       /* +0x1c (FSTP [ESP+0x30]) */
    cornerOffsets[7] = cornerOffsets[1];      /* +0x34 (MOV from rounded scratch) */
    cornerOffsets[2] = (float)-halfW;         /* +0x20 (FSTP [ESP+0x34]) */
    cornerOffsets[3] = (float)-threeQuarterH; /* +0x24 (FST  [ESP+0x38]) */
    cornerOffsets[4] = (float)halfW;          /* +0x28 (FSTP [ESP+0x3c]) */
    cornerOffsets[5] = (float)-threeQuarterH; /* +0x2c (FSTP [ESP+0x40]) */
    cornerOffsets[6] = (float)halfW;          /* +0x30 (FSTP [ESP+0x44]) */

    /* 0x30031f34: set the 2D draw color (trap_R_SetColor via wrapper 0x3003e0d0,
     * which forwards arg1 to cgame_syscall(0x48, color)). */
    trap_R_SetColor(color);

    /* 0x30031f39..0x30031f55: draw the rotated turret icon quad. Push trace
     * (first pushed = deepest arg): hShader(arg0), angleResult, cg_turretTagShaderParams
     * (0x30071a3c), rect[1], slidX; EDX = &cornerOffsets. So the argument order is
     * (cornerOffsets, x=slidX, y=rect[1], shaderParams, rotOrScale=angleResult, hShader). */
    CG_DrawTurretTagQuad(cornerOffsets, slidX, rect->y,
                         cg_turretTagShaderParams, angleResult, hShader);

    /* 0x30031f5a..0x30031f5e: reset the 2D draw color to opaque white
     * (cgame_syscall(0x48, 0) == trap_R_SetColor(NULL)). */
    trap_R_SetColor((const float *)0);
}
