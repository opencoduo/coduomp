// Source: uo_cgame_mp_x86.dll 0x3001f260..0x3001f3be
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f260_3001f3be.mcode
//
// CG_ScriptMover — the eType == 8 (ET_SCRIPTMOVER) arm of the
// CG_AddCEntity render dispatch.
//
// Naming: the .mcode size-guess name "G_SetFixedLink" is REJECTED. It was matched
// only by byte size (win 0x15e == corpus 0x15e), exactly the size matching the
// contract forbids, and the body contradicts it (no link/parent fixup — it builds
// and submits a refEntity_t). Call-graph proof of the real identity: the
// CG_AddCEntity jump table (0x30022228, indexed by cent->currentState.eType) has
// element 8 = 0x30022248 -> thunk 0x300221d6, whose body is
// `PUSH EBX; CALL 0x3001f260; ADD ESP,4; ...; RET` — i.e. this function is the
// eType-8 handler, invoked with the centity_t as one 32-bit stack
// arg (cdecl, caller cleanup). The shared header already declares this exact
// address as CG_ScriptMover (ET_SCRIPTMOVER = 8); that role name
// is adopted here.
//
// Shape: this handler is CG_AddCEntity_General (0x3001e430) plus CG_Mover's
// (0x3001f120) static-inline-model branch:
//   * Like CG_General, it (re)binds the entity's DObj anim tree via
//     CG_RefreshEntityDObjAnimTree(cent->currentState.eType, cg_gameModels[modelIndex])
//     (entityNum in ESI), and — on the DObj-model path — fills the lighting origin via
//     CG_SetupWeaponLightingOrigin(cent, &re) before submitting.
//   * Like CG_Mover, it early-outs on a null DObj skeleton only when
//     solid != SOLID_BMODEL, and when
//     solid == SOLID_BMODEL it draws a static
//     inline/brush model instead (reType 0, hModel = cg_inlineModelHandles[itemIndex]).
//
// The axis is built the same way CG_Mover builds it: AngleVectors (0x3004a200,
// register ABI EDX=angles, ESI=forward, EDI=right, EBX=up) gives forward/right/up,
// then axis[0]=forward, axis[1] = 0.0f - right (per-component FLD 0.0 / FSUB, the
// 0.0f constant at .rdata 0x3007bcec — dumped `00 00 00 00`), axis[2]=up: the
// standard negate-right AnglesToAxis. CG_General instead calls the packaged
// AnglesToAxisNegRight (0x3004c200); both yield the same basis. The `right` scratch
// vector lives off-refEntity (frame slot [ESP+0x14]) and is discarded.
//
// ABI / frame: cent arrives as one 32-bit stack arg. The prologue snapshots the
// MSVC /GS stack cookie ([0x30081650]) into the frame and the epilogue hands it to
// __security_check_cookie (0x30061639); that guards the on-stack refEntity_t and is
// a compiler artifact, not source-level behavior. The register-arg ABI of the
// callees is documented, not encoded as attributes, per the contract.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include <string.h>

void CG_ScriptMover(centity_t *cent /* one 32-bit stack arg */)
{
    refEntity_t re;
    vec3_t right;      /* AngleVectors "right" scratch; only -right is kept in axis[1] */
    struct DObj_s *dobj;

    /* 0x3001f27c: MOV AL,[cent+8]; TEST AL,AL; JS -> skip rendering when eFlags bit 7
     * (EF_NODRAW) is set. The JS target is the /GS cookie-verify epilogue, so this is
     * a plain "don't draw this frame" return. */
    if (cent->currentState.eFlags & EF_NODRAW)
        return;

    /* 0x3001f289..0x3001f29d: MOV ECX,[EAX*4+0x304480e4] selects the registered
     * model handle; eType is the first stack arg and entityNum arrives in ESI. */
    int32_t modelIndex = cent->currentState.modelIndex;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)modelIndex >= (uint32_t)CS_MODELS_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_ScriptMover: invalid model index %i",
                  modelIndex);
        return;
    }
    qhandle_t modelHandle = cg_gameModels[modelIndex];
    int32_t eType = cent->currentState.eType;
    int32_t entityNum = cent->currentState.number;
    CG_RefreshEntityDObjAnimTree(entityNum, eType, modelHandle);

    /* 0x3001f2a2..0x3001f2b0: handle = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number).
     * The two anim-tree args are cleaned together with this call's two args by the
     * single ADD ESP,0x10 at 0x3001f2b6. */
    entityNum = cent->currentState.number;
    dobj = (struct DObj_s *)cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);

    /* 0x3001f2b9..0x3001f2c7: CMP [cent+0xa0],0xffffff; JZ skips the null-skeleton
     * reject. When solid != SOLID_BMODEL and the DObj
     * handle came back 0, there is no model to draw this frame -> return. */
    if (cent->currentState.solid != (int32_t)SOLID_BMODEL && dobj == NULL)
        return;

    uint32_t zBits;
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));

    /* 0x3001f2d3..0x3001f2de: XOR EAX,EAX; MOV ECX,0x27; LEA EDI,re; REP STOSD ->
     * zero 0x27 dwords = 0x9c bytes = sizeof(re). */
    memset(&re, 0, sizeof(re));

    /* 0x3001f2cd snapshots Z before the clear, then loads X/Y and publishes
     * Z/Z/X/Y/X/Y as raw dwords before AngleVectors. */
    uint32_t xBits;
    uint32_t yBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));
    memcpy(&re.origin[2], &zBits, sizeof(zBits));
    memcpy(&re.oldorigin[2], &zBits, sizeof(zBits));
    memcpy(&re.origin[0], &xBits, sizeof(xBits));
    memcpy(&re.origin[1], &yBits, sizeof(yBits));
    memcpy(&re.oldorigin[0], &xBits, sizeof(xBits));
    memcpy(&re.oldorigin[1], &yBits, sizeof(yBits));

    /* 0x3001f2f4..0x3001f357: axis = AnglesToAxis(cent->lerpAngles), negate-right
     * form. AngleVectors gets forward=re.axis[0], right=scratch, up=re.axis[2]; then
     * each axis[1] component = 0.0f - right[component] (FLD 0.0f @0x3007bcec / FSUB). */
    AngleVectors(cent->lerpAngles, re.axis[0], right, re.axis[2]);
    re.axis[1][0] = 0.0f - right[0];
    re.axis[1][1] = 0.0f - right[1];
    re.axis[1][2] = 0.0f - right[2];

    /* 0x3001f333: MOV [re.renderfx],0x40 — unconditional, before the model-select
     * branch. Bit 0x40 == RF_NOSHADOW. */
    re.renderfx = (int32_t)RF_NOSHADOW;

    /* 0x3001f32a/0x3001f35b: CMP [cent+0xa0],0xffffff; JZ -> inline-model path. */
    if (cent->currentState.solid == (int32_t)SOLID_BMODEL) {
        /* 0x3001f382..0x3001f39a: static inline/brush model. hModel from the
         * inline-model handle table indexed by currentState.itemIndex (cent+0x8c),
         * reType 0. */
        /* The unsigned 9-bit wire field exactly spans this 512-entry table. */
        re.hModel = cg_inlineModelHandles[cent->currentState.itemIndex];
        re.reType = 0;
    } else {
        /* 0x3001f35d..0x3001f37b: animated DObj model. Remember the skeleton handle
         * (re+0x90) and owning centity (re+0x94), reType RT_MODEL, and fill the
         * lighting origin via CG_SetupWeaponLightingOrigin(cent, &re)
         * (ECX = cent, EDX = &re). */
        re.dobj = dobj;
        re.owner = cent;
        re.reType = RT_MODEL;
        CG_SetupWeaponLightingOrigin(cent, &re);
    }

    /* 0x3001f39b..0x3001f3a8: trap_R_AddRefEntityToScene(&re)
     * (PUSH &re; PUSH 0x3d; CALL [cgame_syscall]; ADD ESP,8). */
    trap_R_AddRefEntityToScene(&re);

    /* 0x3001f3ab..0x3001f3bd: /GS cookie verify + epilogue (omitted from source). */
}
