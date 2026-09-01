// Source: uo_cgame_mp_x86.dll 0x3001f120..0x3001f25d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f120_3001f25d.mcode
//
// CG_Mover — the ET_MOVER (eType == 5) arm of the CG_AddCEntity render dispatch
// (CG_AddCEntity at 0x30022170 jump-tables cent->currentState.eType through the
// table at 0x30022228; entry [5] = 0x30022170's thunk at 0x300221c9 tail-calls
// this handler with cent in EBX / one stack arg). The dispatch siblings are
// CG_General (eType 0, 0x3001e430), CG_Player (1, 0x300343e0),
// CG_AddPlayerCorpseEntity (2, 0x300346c0), 0x3001e680 (3), CG_Missile
// (4, 0x3001edb0), this (5), and 0x3001f260 (8).
//
// Name evidence: the machine dispatch makes this the eType-5 arm, and its model/
// inline-brush rendering plus mover-compensation consumers identify ET_MOVER.
// It builds one
// refEntity_t whose orientation axis is AnglesToAxis(cent angles), place it at a
// single point (origin == oldorigin), set renderfx = RF_NOSHADOW (0x40), pick a
// model, and submit it via trap_R_AddRefEntityToScene. The .mcode's
// "MatrixMultiply43" name is a size-only guess (win size 0x13d ~= corpus 0x13c)
// and is rejected: there is no 4x3 matrix multiply here, this is an eType render
// handler that builds and submits a refEntity_t.
//
// ABI: cent arrives as one 32-bit stack arg (the caller PUSHes EBX then
// `CALL 0x3001f120; ADD ESP,4`, i.e. cdecl with caller cleanup). The prologue
// snapshots the MSVC stack cookie ([0x30081650]) to [ESP+0xc0] and the epilogue
// hands it to __security_check_cookie (0x30061639); that is a compiler-inserted
// /GS guard around the on-stack refEntity_t, not source-level behavior.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include <string.h>

/*
 * cg_inlineModelHandles[] — per-submodel registered render-model handle table
 * (the "*N" inline/brush models), indexed by cent->currentState.modelindex. It
 * is filled at load time by the registration loop at 0x3002c5.. which formats
 * "*%i - inline models" (string 0x30077f78) and calls the model-registration
 * trap (id 0x4d) once per inline model, storing each returned handle into this
 * table (write at 0x3002c67b: table[ebp] = trap(0x4d, ...)); the entry count is
 * kept at the adjacent word 0x30448de4. CG_Mover reads table[modelindex] as the
 * static hModel for its brush model. The mechanical export mislabeled this
 * datum g_data_matrixmultiply43_30448de8 (owner = the rejected size-guess name);
 * the real identity is this inline-model handle array. Renamed once at its
 * canonical globals definition, so every reference converges (not an alias).
 */

/*
 * CG_Mover (0x3001f120) — ET_MOVER (eType == 5) render handler.
 *
 *   if (cent->currentState.eFlags & EF_NODRAW) return;
 *
 *   dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number);
 *   // when the entity is not an inline brush model (+0xa0 != SOLID_BMODEL)
 *   // a missing skeleton (dobjHandle == 0) means "nothing to draw".
 *   if (cent->currentState.solid != SOLID_BMODEL && dobjHandle == 0)
 *       return;
 *
 *   memset(&ent, 0, 0x9c);                       // STOSD 0x27 dwords from ent
 *   ent.origin    = cent->lerpOrigin;          // +0x208 vec3 -> origin (+0x44)
 *   ent.oldorigin = cent->lerpOrigin;          // and -> oldorigin (+0x54)
 *   AnglesToAxis(cent->lerpAngles, ent.axis);  // +0x214 -> axis[3][3]
 *   ent.renderfx = RF_NOSHADOW;                  // 0x40, always
 *
 *   if (cent->currentState.solid == SOLID_BMODEL) {
 *       ent.reType = 0;                          // static inline-model beam
 *       ent.hModel = cg_inlineModelHandles[cent->currentState.itemIndex];
 *   } else {
 *       ent.reType = RT_MODEL;                   // 1, DObj-animated model
 *       ent.dobj       = dobjHandle;
 *       ent.owner      = cent;
 *   }
 *
 *   trap_R_AddRefEntityToScene(&ent);
 *
 * The axis build is exactly AnglesToAxis via AngleVectors: AngleVectors gives
 * forward/right/up (0x3004a200, EDX=angles, ESI=forward, EDI=right, EBX=up); the
 * handler stores axis[0]=forward, axis[1] = -right (0.0f - right, each component
 * via FLD 0.0 / FSUB), axis[2]=up — the standard negate-right AnglesToAxis. The
 * `right` scratch vector lives off the refEntity_t and is discarded.
 */
void CG_Mover(centity_t *cent /* one 32-bit stack arg */)
{
    refEntity_t ent;
    vec3_t right; /* AngleVectors "right" scratch; only -right is kept in axis[1] */
    struct DObj_s *dobj;

    /* MOV AL,[cent+8]; TEST AL,AL; JS -> skip everything (bare return). */
    if ((uint8_t)cent->currentState.eFlags & EF_NODRAW)
        return;

    /* PUSH cent->number; PUSH 0xa5; CALL cgame_syscall; ADD ESP,8. */
    dobj = (struct DObj_s *)cgame_syscall(
        CG_DOBJ_GET_HANDLE, cent->currentState.number);

    /* CMP [cent+0xa0],0xffffff; JZ skips this null-skeleton reject.
     * When != 0xffffff and the DObj handle came back 0, there is no model to
     * draw for this frame -> return. */
    if (cent->currentState.solid != (int32_t)SOLID_BMODEL &&
        dobj == NULL)
        return;

    uint32_t zBits;
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));

    /* XOR EAX,EAX; MOV ECX,0x27; LEA EDI,ent; REP STOSD -> zero the whole
     * 0x9c-byte PE32 refEntity_t. Native pointer fields widen, so clear the
     * complete host representation. */
    memset(&ent, 0, sizeof(ent));

    /* Z is captured before the clear; X/Y after it. The target publishes Z/Z,
     * then X/Y/X/Y before calling AngleVectors. */
    uint32_t xBits;
    uint32_t yBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));
    memcpy(&ent.origin[2], &zBits, sizeof(zBits));
    memcpy(&ent.oldorigin[2], &zBits, sizeof(zBits));
    memcpy(&ent.origin[0], &xBits, sizeof(xBits));
    memcpy(&ent.origin[1], &yBits, sizeof(yBits));
    memcpy(&ent.oldorigin[0], &xBits, sizeof(xBits));
    memcpy(&ent.oldorigin[1], &yBits, sizeof(yBits));

    /* Orientation basis from cent->lerpAngles (cent+0x214), AnglesToAxis
     * shape: axis[0]=forward, axis[1]=-right, axis[2]=up. */
    AngleVectors(cent->lerpAngles, ent.axis[0], right, ent.axis[2]);
    ent.axis[1][0] = 0.0f - right[0];
    ent.axis[1][1] = 0.0f - right[1];
    ent.axis[1][2] = 0.0f - right[2];

    /* MOV [ent.renderfx],0x40 — unconditional, before the model-select branch. */
    ent.renderfx = RF_NOSHADOW;

    if (cent->currentState.solid == (int32_t)SOLID_BMODEL) {
        /* Static inline/brush model beam: reType 0, model from the inline-model
         * handle table indexed by currentState.modelindex (cent+0x8c). */
        ent.reType = 0;
        /* The unsigned 9-bit wire field exactly spans this 512-entry table. */
        ent.hModel = cg_inlineModelHandles[cent->currentState.itemIndex];
    } else {
        /* Animated DObj model: reType RT_MODEL, remember the skeleton handle and
         * the owning centity for the engine. */
        ent.reType     = RT_MODEL;
        ent.dobj = dobj;
        ent.owner      = cent;
    }

    /* PUSH &ent; PUSH 0x3d; CALL cgame_syscall; ADD ESP,8. */
    trap_R_AddRefEntityToScene(&ent);
}
