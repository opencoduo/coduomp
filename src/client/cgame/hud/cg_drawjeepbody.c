#include "../client_recovered.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30031cb0..0x30031d4e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031cb0_30031d4e.mcode
//
// CG_DrawJeepBody — draw one HUD "tag" for the entity the
// effect/HUD-tag pass is currently processing (cg_entities[cg_predictedPlayerState.viewLockedEntityNum]),
// but only when the local predicted player is in the tag-enabling state and the
// target entity matches the eType==12 / stateFilter==1 gate. The tag is a 2D
// shader quad whose X position slides in horizontally from an animation fraction.
//
// This is the plainest member of the 0x30031cb0..0x30032042 HUD-tag draw family
// (0x30031d50, 0x30031e20, 0x30031f70 are the siblings): same predicted-player and
// entity gates, same cg_entities[] indexing, same `(cg_hudCompassSize_vmCvar.value-1)*112+rect.x`
// horizontal slide, same trap_R_SetColor(color) / draw / trap_R_SetColor(NULL) bracket.
// Where the siblings draw a rotated name string over a bone, this one draws a single
// non-rotated quad (angle 0) with the caller's shader handle.
//
// Name adjudication: the .mcode header's size-matched "BG_AnimParseError" guess is
// REJECTED — there is no format string, no parse, and no error path; the body is a
// gated 2D draw. The Mac CG_DrawJeepBody has the same entity-lerp and
// rotated-picture calls, with its SetColor wrappers kept out of line, resolving
// the source name.
//
// ---- Machine-code / ABI notes ------------------------------------------------------
// Register/stack ABI (frame base E = ESP at entry, retaddr at [E]):
//   ESI  = rect, a float[4] {x,y,w,h}  (register argument; read at [ESI], [ESI+4],
//          [ESI+8], [ESI+0xc]; never set from a stack slot here)
//   [E+4] = arg0 = hShader   (the shader handle drawn; forwarded to CG_DrawRotatedPic)
//   [E+8] = arg1 = color     (const float rgba[4]; passed to trap_R_SetColor)
// The entry `PUSH ECX` reserves one scratch dword (E-4) that later holds the computed
// slid X. `PUSH EAX` (the entity ptr) at E-8 is the cdecl arg to
// CG_CalcEntityLerpPositions and is left on the stack (no ADD ESP after that call);
// the whole 0x28-byte push region is unwound by the single `ADD ESP,0x28` at 0x30031d33.
//
// cgame_syscall stack layout for trap_R_SetColor: after `PUSH color; PUSH 0x48`, the
// call reads [ESP]=id=0x48 and [ESP+4]=color, i.e. cgame_syscall(72, arg1); the args
// are caller-cleaned (they stay live until the final ADD ESP,0x28). The tail issues the
// second color reset by writing [ESP+4]=0x48, [ESP+8]=0 and JMP-ing to *cgame_syscall,
// i.e. a tail-call trap_R_SetColor(NULL).
//
// CG_DrawRotatedPic push trace (first pushed = last/highest arg):
//   PUSH arg0(hShader) | PUSH 0 (angle) | PUSH rect[3] | PUSH rect[2] | PUSH rect[1] |
//   PUSH slidX  =>  CG_DrawRotatedPic(slidX, rect[1], rect[2], rect[3], 0.0f, hShader).
//
// slidX float chain (0x30031cf1..0x30031d0c):
//   FLD cg_hudCompassSize_vmCvar.value; FSUB 1.0f; FMUL 112.0f; FADD rect[0]; FSTP -> scratch(E-4)
//   0x3007bce0 = 1.0f, 0x3007c1e0 = 112.0f (verified from .rdata).
//
// Gate (all must hold, else the plain POP ECX; RET at 0x30031d4c):
//   (flags & EF_IN_VEHICLE) != 0      TEST 0x100000 ; JZ end
//   (flags & EF_VEHICLE_ALLOW_WEAPON) == 0      TEST 0x400000 ; JNZ end
//   entity->currentState.eType == 12                       CMP [+0x4],0xc ; JNZ end
//   entity->currentState.stateFilter == 1                  CMP [+0x88],1  ; JNZ end
// entity = &cg_entities[cg_predictedPlayerState.viewLockedEntityNum] via IMUL 0x288 + base 0x3048c6e0
// (cg_entities is modeled through the existing centity_t view of that base array).

/* eType value that enables the HUD entity tag (an ET_* entity type; exact enum name
 * unproven — only value 12 is proven by the CMP [entity+0x4],0xc gate). */
enum { CG_TAG_ENTITY_ETYPE = 12 };

/* stateFilter value this member requires (siblings compare it to a passed-in filter;
 * this one hardcodes 1). */
enum { CG_TAG_STATE_FILTER = 1 };

void CG_DrawJeepBody(const rectDef_t *rect, int32_t hShader,
                     const float *color)
{
    /* 0x30031cb1..0x30031cc6: gate on the local predicted player's entityStateFlags. */
    uint32_t flags = cg_predictedPlayerState.entityStateFlags;
    if ((flags & EF_IN_VEHICLE) == 0)
        return;
    if ((flags & EF_VEHICLE_ALLOW_WEAPON) != 0)
        return;

    int32_t entityNum = cg_predictedPlayerState.viewLockedEntityNum;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_DrawJeepBody: invalid view-lock entity %i",
                  entityNum);
        return;
    }
    centity_t *entity = &cg_entities[entityNum];
    if (entity->currentState.eType != CG_TAG_ENTITY_ETYPE)
        return;
    if (entity->currentState.stateFilter != CG_TAG_STATE_FILTER)
        return;

    /* 0x30031ceb..0x30031cec: fill in the entity's interpolated weapon/aim angles. */
    CG_CalcEntityLerpPositions(entity);

    /* 0x30031cf1..0x30031d10: set the 2D draw color, then compute the slid X.
     * The slid X (0x3007bce0 = 1.0f, 0x3007c1e0 = 112.0f) is evaluated before the
     * color call in the machine code but is only consumed by the draw below. */
    float slidX = (float)(
        ((long double)cg_hudCompassSize_vmCvar.value - 1.0L) * 112.0L +
        (long double)rect->x);
    trap_R_SetColor(color);

    /* 0x30031d16..0x30031d33: draw the tag quad (angle 0), sliding in from slidX. */
    CG_DrawRotatedPic(slidX, rect->y, rect->w, rect->h, 0.0f, hShader);

    /* 0x30031d36..0x30031d46: reset the 2D draw color to opaque white (tail call). */
    trap_R_SetColor((const float *)0);
}
