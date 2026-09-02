#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30031f70..0x30032042
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031f70_30032042.mcode
//
// CG_DrawTankPositionStatus — the mask-gated rotated-tag member of the
// 0x30031cb0..0x30032042 HUD-tag draw family. Like the rotated-tag sibling
// CG_DrawTankBody (0x30031d50), it draws one HUD "tag" (a 2D shader quad)
// for the entity the effect/HUD-tag pass is currently processing
// (cg_entities[cg_predictedPlayerState.viewLockedEntityNum]): sliding in horizontally from an animation
// fraction and rotated by the entity's aim yaw relative to the effect spin angle
// (angle = AngleSubtract(cg_refdefViewAngles[1], entity->lerpAngles[1])).
//
// What distinguishes this member is an extra per-entity bitmask gate: after the
// eType==12 and stateFilter gates, the entity's hudTagMask (+0xe4) must have the
// (1 << bitIndex) bit set, where bitIndex is a caller-supplied argument. Only when
// that bit is set does the draw happen.
//
// Name adjudication: the .mcode header's size-matched "Script_ExecOnCvarFloatValue"
// guess is REJECTED — there is no cvar read, no script exec, no format string; the
// body is a gated 2D rotated-pic draw that is byte-structurally the twin of the
// rotated-tag sibling at 0x30031d50 with an added mask gate. The Mac
// CG_DrawTankPositionStatus has the same entity-lerp, AngleSubtract, SetColor, and
// rotated-picture sequence, resolving the source name.
//
// ---- Machine-code / ABI notes ------------------------------------------------------
// Register/stack ABI (frame base E = ESP at entry, retaddr at [E]):
//   EDI  = rect, a float[4] {x,y,w,h}  (register argument; read at [EDI], [EDI+4],
//          [EDI+8], [EDI+0xc]; never set from a stack slot here).
//   ECX  = stateFilter to match (register argument). When 0 the stateFilter
//          comparison is skipped entirely (TEST ECX,ECX; JZ 0x30031fbb).
//   [E+4]  = arg0 = hShader   (forwarded as the 6th arg of CG_DrawRotatedPic)
//   [E+8]  = arg1 = color     (const float rgba[4]; passed to trap_R_SetColor)
//   [E+0xc]= arg2 = bitIndex  (shift count for the (1 << bitIndex) hudTagMask test)
//   ESI holds the entity pointer across the body (PUSH ESI saves the caller's ESI at
//   0x30031f8e; POP ESI at 0x3003203d).
//
// The mask gate (0x30031fbb..0x30031fcc): MOV ECX,[ESP+0x18] loads arg2 (bitIndex);
//   MOV EAX,1; SHL EAX,CL builds (1 << (bitIndex & 31)); TEST [ESI+0xe4],EAX; JZ end
//   requires (entity->currentState.hudTagMask & (1u << bitIndex)) != 0.
//
// entity = &cg_entities[cg_predictedPlayerState.viewLockedEntityNum] via IMUL 0x288 + base 0x3048c6e0
// (cg_entities modeled through the existing centity_t view of that base array).
//
// AngleSubtract call (0x30031fd4..0x30031fe7):
//   ECX = entity->lerpAngles[1] (raw dword at entity+0x218, the yaw component of
//         the +0x214 vec3), EDX = cg_refdefViewAngles[1] (0x30487acc, float loaded as a
//         dword). PUSH ECX; PUSH EDX => cgame arg order
//         AngleSubtract(a=cg_refdefViewAngles[1], b=lerpAngles[1]). FSTP [ESP+0x10]
//         spills the ST(0) result to scratch E-8.
//
// slidX float chain (0x30031feb..0x30032006), identical to the tag siblings:
//   FLD cg_hudCompassSize_vmCvar.value; FSUB 1.0f; FMUL 112.0f; FADD rect[0]; FSTP -> scratch(E-4)
//   0x3007bce0 = 1.0f, 0x3007c1e0 = 112.0f (verified from .rdata). Interleaved with the
//   PUSH color/PUSH 0x48 setup for the first trap_R_SetColor.
//
// trap_R_SetColor(color): after PUSH arg1(color); PUSH 0x48, CALL *cgame_syscall
//   (0x30085e9c) reads [ESP]=id=0x48 (72) and [ESP+4]=color => cgame_syscall(72, color).
//
// CG_DrawRotatedPic push trace (first pushed = last/highest arg, 0x30032010..0x3003202b):
//   PUSH arg0(hShader) | PUSH angleResult(E-8) | PUSH rect[3] | PUSH rect[2] |
//   PUSH rect[1] | PUSH slidX(E-4)  =>
//   CG_DrawRotatedPic(slidX, rect[1], rect[2], rect[3], angleResult, hShader).
//
// Tail (0x30032030..0x30032034): PUSH 0; PUSH 0x48; CALL *cgame_syscall
//   => trap_R_SetColor(NULL). The whole push region is unwound by ADD ESP,0x34 /
//   POP ESI (draw path) and by POP ESI / ADD ESP,8 on every path (the SUB ESP,8 at
//   entry is matched by the final ADD ESP,8 before RET).

/* eType value that enables the HUD entity tag (an ET_* entity type; exact enum name
 * unproven — only value 12 is proven by the CMP [entity+0x4],0xc gate). Shared with
 * the tag-family siblings. */
enum {
    CG_TAG_ENTITY_ETYPE = 12
};

void CG_DrawTankPositionStatus(const rectDef_t *rect, int32_t stateFilter, int32_t hShader, const float *color, int32_t bitIndex)
{
    /* 0x30031f70..0x30031f88: gate on the local predicted player's entityStateFlags. */
    uint32_t flags = cg_predictedPlayerState.entityStateFlags;
    if ((flags & EF_IN_VEHICLE) == 0)
        return;
    if ((flags & EF_VEHICLE_ALLOW_WEAPON) != 0)
        return;

    int32_t entityNum = cg_predictedPlayerState.viewLockedEntityNum;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_DrawTankPositionStatus: invalid view-lock "
                  "entity %i",
                  entityNum);
        return;
    }
    centity_t *entity = &cg_entities[entityNum];
    if (entity->currentState.eType != CG_TAG_ENTITY_ETYPE)
        return;

    /* 0x30031fab..0x30031fb9: when a nonzero filter is passed, the entity's stateFilter
     * must match it. filter == 0 skips this check (TEST ECX,ECX; JZ 0x30031fbb). */
    if (stateFilter != 0 && entity->currentState.stateFilter != stateFilter)
        return;

    /* 0x30031fbb..0x30031fcc: mask gate — the entity's hudTagMask must have the bit
     * selected by bitIndex set. SHL by CL uses only the low 5 bits of the count. */
    if ((entity->currentState.hudTagMask & (1u << ((uint32_t)bitIndex & 31u))) == 0)
        return;

    /* 0x30031fce..0x30031fcf: fill in the entity's interpolated weapon/aim angles. */
    CG_CalcEntityLerpPositions(entity);

    /* 0x30031fd4..0x30031fe7: rotation angle = shortest signed difference between the
     * animated effect spin angle and the entity's aim yaw (lerpAngles[1], the
     * +0x218 middle component of the +0x214 vec3). */
    float angle = AngleSubtract(cg_refdefViewAngles[1], entity->lerpAngles[1]);

    /* 0x30031feb..0x30032006: set the 2D draw color, then compute the slid X.
     * The slid X (0x3007bce0 = 1.0f, 0x3007c1e0 = 112.0f) is evaluated before the
     * color call in the machine code but is only consumed by the draw below. */
    float slidX = (float)(((long double)cg_hudCompassSize_vmCvar.value - 1.0L) * 112.0L + (long double)rect->x);
    trap_R_SetColor(color);

    /* 0x30032010..0x3003202b: draw the tag quad, sliding in from slidX and rotated by
     * the computed angle, with the caller's shader handle. */
    CG_DrawRotatedPic(slidX, rect->y, rect->w, rect->h, angle, hShader);

    /* 0x30032030..0x30032034: reset the 2D draw color to opaque white. */
    trap_R_SetColor((const float *)0);
}
