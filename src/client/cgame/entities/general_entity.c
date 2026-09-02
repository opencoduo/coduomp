// Source: uo_cgame_mp_x86.dll 0x3001e430..0x3001e50c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e430_3001e50c.mcode
//
// CG_General — the eType-0 handler of CG_AddCEntity's dispatch.
//
// Naming: the mechanical .mcode name "CG_LoadShellShockCvars" is REJECTED. It was a
// broad-corpus/size guess (win size 0xdc == matched 0xdc), exactly the size-matching
// the contract forbids. This function loads no cvars and reads no shellshock config;
// it builds and submits a render entity. Call-graph proof of the real identity:
//   * The dispatcher at 0x30022170 does `MOV EBX,EAX; CALL 0x3001e7f0; CMP [EBX+4],0xf;
//     JA default; JMP [0x30022228 + eType*4]`. That 16-entry jump table's element 0 is
//     0x30022196, whose body is `CALL 0x3001e430; POP..; RET` — i.e. this function is
//     the eType==0 arm.
//   * Its caller 0x3001f6f0 checks `[ESI+4] < 0x10` (eType < 16), calls the weapon-angle
//     lerp 0x30021d30, then `MOV EAX,ESI; CALL 0x30022170`. So the dispatched object is a
//     centity_t with eType at +4 and lerpOrigin at +0x208.
//   In id-Tech/CoD, CG_AddCEntity switches on cent->currentState.eType and routes
//   ET_GENERAL (0) to CG_General, which builds an RT_MODEL render entity, orients it,
//   sets the DObj model handle and lighting origin, and submits it — exactly this body.
//
// Behavior (proven from the bytes):
//   1. /GS frame: snapshot __security_cookie into the frame; verify via
//      __security_check_cookie (0x30061639) on exit. Not source-level; omitted from body.
//   2. `MOV AL,[EBX+8]; TEST AL,AL; JS 0x3001e4fa` — if the low byte of cent->currentState.eFlags has
//      bit 7 set (EF_NODRAW = 0x80), skip everything and return.
//   3. Refresh the entity's DObj weapon anim tree: load
//      cg_gameModels[cent->currentState.modelIndex] from the registered-model table at
//      0x304480e4, and call CG_RefreshEntityDObjAnimTree(cent->currentState.eType, thatParam) with
//      ESI = cent->currentState.number (register arg). It is caller-cleaned (plain RET); the two
//      pushed args' cleanup is deferred and folded into the `ADD ESP,0x10` after the
//      following trap 0xa5 call, so no `ADD ESP,8` follows the CALL at 0x3001e469 itself.
//   4. `handle = cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number)`
//      (`PUSH [EBX]; PUSH 0xa5; CALL [cgame_syscall]; ADD ESP,0x10` — this 0x10 also
//      cleans the two anim-tree args pushed in step 3). ESI = handle.
//      `TEST ESI,ESI; JZ 0x3001e4fa` — a zero handle means no DObj: skip and return.
//   5. Build an on-stack refEntity_t: REP STOSD zero-fills 0x27 (39) dwords = 0x9c =
//      sizeof(refEntity_t) (memset), then:
//        - origin    = cent->lerpOrigin (raw dword copies of +0x208/+0x20c/+0x210)
//        - oldorigin  = cent->lerpOrigin (same three dwords)
//        - axis      = AnglesToAxisNegRight(re.axis, cent->lerpAngles)
//                       (EAX = &re.axis @ ESP+0x24, EDX = &cent->lerpAngles @ +0x214)
//        - re.dobj       = handle (ESI, store at re+0x90)
//        - re.owner      = cent   (EBX, store at re+0x94)
//        - re.reType     = RT_MODEL (1)
//        - CG_SetupWeaponLightingOrigin(cent, &re) (ECX = cent, EDX = &re): fills
//          re.lightingOrigin and OR-s in RF_LIGHTING_ORIGIN.
//   6. Submit: `MOV EAX,EDX(=&re); PUSH EAX; PUSH 0x3d; CALL [cgame_syscall]; ADD ESP,8`
//      = trap_R_AddRefEntityToScene(&re).
//
// The register-arg ABI (cent in EBX) and the /GS cookie save/verify are i386
// calling-convention details, recorded here and expressed as plain C, per the contract.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include <string.h>

void CG_General(centity_t *cent /* EBX */)
{
    refEntity_t re;
    struct DObj_s *dobj;

    /* 0x3001e448: MOV AL,[EBX+8]; TEST AL,AL; JS -> low byte's sign bit = eFlags bit 7. */
    if (cent->currentState.eFlags & EF_NODRAW)
        return;

    /* 0x3001e455: MOV EAX,[EBX+0x90]; MOV ECX,[EAX*4+0x304480e4]. The table base
     * is cg_gameModels[0], proven by its registration loop and vmMain command 10. */
    int32_t modelIndex = cent->currentState.modelIndex;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)modelIndex >= (uint32_t)CS_MODELS_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_General: invalid model index %i",
                  modelIndex);
        return;
    }
    qhandle_t modelHandle = cg_gameModels[modelIndex];
    int32_t eType = cent->currentState.eType;
    int32_t entityNum = cent->currentState.number;
    CG_RefreshEntityDObjAnimTree(entityNum, eType, modelHandle);

    /* 0x3001e46e: handle = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number). */
    entityNum = cent->currentState.number;
    dobj = (struct DObj_s *)cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);
    /* 0x3001e481: TEST ESI,ESI; JZ -> no DObj skeleton, nothing to draw. */
    if (dobj == NULL)
        return;

    /* 0x3001e485 snapshots Z before the REP clear. The later X/Y loads and all six
     * destination writes are raw dword MOVs, so keep their bit patterns and their
     * exact publication order rather than expressing this as two vector copies. */
    uint32_t zBits;
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));

    /* 0x3001e48b..: build the render entity. REP STOSD zeroes 0x9c bytes = sizeof(re). */
    memset(&re, 0, sizeof(re));

    uint32_t xBits;
    uint32_t yBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));
    memcpy(&re.origin[0], &xBits, sizeof(xBits));
    memcpy(&re.origin[2], &zBits, sizeof(zBits));
    memcpy(&re.oldorigin[0], &xBits, sizeof(xBits));
    memcpy(&re.oldorigin[2], &zBits, sizeof(zBits));
    memcpy(&re.origin[1], &yBits, sizeof(yBits));
    memcpy(&re.oldorigin[1], &yBits, sizeof(yBits));

    /* 0x3001e4b4/0x3001e4ba/0x3001e4c6: axis = AnglesToAxisNegRight(re.axis,
     * cent->lerpAngles). EAX = &re.axis, EDX = &cent->lerpAngles. */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);

    /* 0x3001e4d1/0x3001e4d8/0x3001e4df: DObj handle, owning centity, and reType. */
    re.dobj = dobj;
    re.owner = cent;
    re.reType = RT_MODEL;

    /* 0x3001e4e7: CG_SetupWeaponLightingOrigin(cent, &re) — ECX = cent, EDX = &re. */
    CG_SetupWeaponLightingOrigin(cent, &re);

    /* 0x3001e4ee: trap_R_AddRefEntityToScene(&re). */
    trap_R_AddRefEntityToScene(&re);

    /* 0x3001e4fa..0x3001e50b: /GS cookie verify + epilogue (omitted). */
}
