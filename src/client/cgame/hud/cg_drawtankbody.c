#include "../client_recovered.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30031d50..0x30031e1b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031d50_30031e1b.mcode
//
// CG_DrawTankBody — the rotated-tag sibling of
// CG_DrawJeepBody (0x30031cb0). Draws one HUD "tag" (a 2D shader quad)
// for the entity the effect/HUD-tag pass is currently processing
// (cg_entities[cg_predictedPlayerState.viewLockedEntityNum]), sliding in horizontally like the slide-tag
// sibling, but rotated by an angle derived from the entity's aim yaw relative to
// the animated effect spin angle. Where CG_DrawJeepBody hardcodes
// stateFilter == 1 and angle 0, this member takes a caller-supplied filter (ECX)
// and computes a rotation:
//     angle = AngleSubtract(cg_refdefViewAngles[1], entity->lerpAngles[1]).
//
// Name adjudication: the .mcode header's size-matched
// "script_method_player_playlocalannouncersound" guess is REJECTED — there is no
// script method dispatch, no announcer/sound path, no string; the body is a gated
// 2D rotated-pic draw that is byte-structurally the twin of the slide-tag sibling
// at 0x30031cb0. The Mac CG_DrawTankBody has the same entity-lerp,
// AngleSubtract, SetColor, and rotated-picture sequence, resolving the source
// name.
//
// ---- Machine-code / ABI notes ------------------------------------------------------
// Register/stack ABI (frame base E = ESP at entry, retaddr at [E]):
//   ESI  = rect, a float[4] {x,y,w,h}  (register argument; read at [ESI], [ESI+4],
//          [ESI+8], [ESI+0xc]).
//   ECX  = stateFilter to match (register argument; see gate below). When 0 the
//          stateFilter comparison is skipped entirely (TEST ECX,ECX; JZ).
//   [E+4] = arg0 = hShader   (forwarded as the 6th arg of CG_DrawRotatedPic)
//   [E+8] = arg1 = color     (const float rgba[4]; passed to trap_R_SetColor)
// Entry `SUB ESP,0x8` reserves two scratch dwords used to spill the AngleSubtract
// result (E-8) and the slid X (E-4). `PUSH EDI` then saves EDI (holds the entity
// pointer). The whole push region is unwound by ADD ESP,0x2c / POP EDI / ADD ESP,8
// on the draw path (0x30031df9), and by ADD ESP,8 (+ POP EDI on one path) on the
// early-out paths (0x30031e16 / 0x30031e17).
//
// Gate (all must hold, else the RET at 0x30031e1a):
//   (flags & EF_IN_VEHICLE) != 0        TEST 0x100000 ; JZ end   (0x30031d58)
//   (flags & EF_VEHICLE_ALLOW_WEAPON) == 0        TEST 0x400000 ; JNZ end  (0x30031d63)
//   entity->currentState.eType == 12                         CMP [EDI+0x4],0xc ; JNZ (0x30031d81)
//   filter == 0  ||  entity->currentState.stateFilter == filter
//                    TEST ECX,ECX; JZ 0x30031d97; CMP [EDI+0x88],ECX; JNZ end
// entity = &cg_entities[cg_predictedPlayerState.viewLockedEntityNum] via IMUL 0x288 + base 0x3048c6e0
// (cg_entities modeled through the existing centity_t view of that base array).
//
// AngleSubtract call (0x30031d9d..0x30031db0):
//   EAX = entity->lerpAngles[1] (raw dword at entity+0x218, the yaw component
//         of the +0x214 vec3), ECX = cg_refdefViewAngles[1] (float loaded as a dword by
//         the MOV reg,[float]; PUSH reg copy idiom). PUSH EAX; PUSH ECX =>
//         cgame arg order AngleSubtract(a=cg_refdefViewAngles[1], b=lerpAngles[1]).
//   FSTP [ESP+0x10] spills the ST(0) result to scratch E-8.
//
// slidX float chain (0x30031db4..0x30031dcf), identical to the slide-tag sibling:
//   FLD cg_hudCompassSize_vmCvar.value; FSUB 1.0f; FMUL 112.0f; FADD rect[0]; FSTP -> scratch(E-4)
//   0x3007bce0 = 1.0f, 0x3007c1e0 = 112.0f (verified from .rdata).
//
// trap_R_SetColor(color): after PUSH arg1(color); PUSH 0x48, CALL *cgame_syscall
//   reads [ESP]=id=0x48 (72) and [ESP+4]=color => cgame_syscall(72, color).
//
// CG_DrawRotatedPic push trace (first pushed = last/highest arg, 0x30031de4..0x30031df4):
//   PUSH arg0(hShader) | PUSH angleResult(E-8) | PUSH rect[3] | PUSH rect[2] |
//   PUSH rect[1] | PUSH slidX(E-4)  =>
//   CG_DrawRotatedPic(slidX, rect[1], rect[2], rect[3], angleResult, hShader).
//
// Tail (0x30031e00..0x30031e10): MOV [ESP+8]=0; MOV [ESP+4]=0x48; JMP *cgame_syscall
//   => tail-call trap_R_SetColor(NULL).

/* eType value that enables the HUD entity tag (an ET_* entity type; exact enum name
 * unproven — only value 12 is proven by the CMP [entity+0x4],0xc gate). Shared with
 * the slide-tag sibling. */
enum {
    CG_TAG_ENTITY_ETYPE = 12
};

void CG_DrawTankBody(const rectDef_t *rect, int32_t stateFilter, int32_t hShader, const float *color)
{
    /* 0x30031d50..0x30031d68: gate on the local predicted player's entityStateFlags. */
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
                  "CG_DrawTankBody: invalid view-lock entity %i",
                  entityNum);
        return;
    }
    centity_t *entity = &cg_entities[entityNum];
    if (entity->currentState.eType != CG_TAG_ENTITY_ETYPE)
        return;

    /* 0x30031d8b..0x30031d95: when a nonzero filter is passed, the entity's
     * stateFilter must match it. filter == 0 draws unconditionally (TEST ECX,ECX; JZ). */
    if (stateFilter != 0 && entity->currentState.stateFilter != stateFilter)
        return;

    /* 0x30031d97..0x30031d98: fill in the entity's interpolated weapon/aim angles. */
    CG_CalcEntityLerpPositions(entity);

    /* 0x30031d9d..0x30031db0: rotation angle = shortest signed difference between the
     * animated effect spin angle and the entity's aim yaw (lerpAngles[1], the
     * +0x218 middle component of the +0x214 vec3). */
    float angle = AngleSubtract(cg_refdefViewAngles[1], entity->lerpAngles[1]);

    /* 0x30031db4..0x30031dd3: set the 2D draw color, then compute the slid X.
     * The slid X (0x3007bce0 = 1.0f, 0x3007c1e0 = 112.0f) is evaluated before the
     * color call in the machine code but is only consumed by the draw below. */
    float slidX = (float)(((long double)cg_hudCompassSize_vmCvar.value - 1.0L) * 112.0L + (long double)rect->x);
    trap_R_SetColor(color);

    /* 0x30031dd9..0x30031df9: draw the tag quad, sliding in from slidX and rotated
     * by the computed angle, with the caller's shader handle. */
    CG_DrawRotatedPic(slidX, rect->y, rect->w, rect->h, angle, hShader);

    /* 0x30031e00..0x30031e10: reset the 2D draw color to opaque white (tail call). */
    trap_R_SetColor((const float *)0);
}
