// Source: uo_cgame_mp_x86.dll 0x3001eca0..0x3001eda3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001eca0_3001eda3.mcode
//
// CG_AddCEntity's eType-11 render handler. This is the arm reached from the
// 16-entry eType jump table at 0x30022228 whose element 11 is 0x3002218d
// (`CALL 0x3001eca0; POP..; RET`), dispatched from 0x30022170 which does
// `MOV EBX,EAX; CALL 0x3001e7f0(=CG_EntityEffects); CMP [EBX+4],0xf; JA default;
// JMP [0x30022228 + eType*4]`. The default arm prints "Bad entity type: %i"
// (0x30077138). So the dispatched object is a centity (centity_t) passed in
// EBX, with eType at +4; this file handles eType == 11.
//
// Naming: the mechanical .mcode name "String_Init" is REJECTED. It is a
// broad-corpus size guess (win size 0x103 vs matched 0x104 — the size-matching
// the contract forbids), and it is wrong on behavior: this function initializes
// no string. It builds and submits an RT_MODEL render entity via
// trap_R_AddRefEntityToScene (trap 0x3d), exactly the CG_General / CG_Item
// family. Its precise CoD source name (CG_Mover / CG_Corona / etc. for eType 11)
// is not provable from this body alone — several eType handlers build a model
// entity — so the function keeps a role name here (CG_AddCEntity eType-11 model
// handler) rather than an unproven guess. Same-module PPC bank lists CG_Mover,
// CG_Portal, CG_Beam, CG_Corona-family handlers; eType 11 is not pinned to one,
// so no server name is adopted.
//
// This function is a near-clone of CG_General (0x3001e430): identical /GS frame,
// identical EF_NODRAW gate, identical CG_RefreshEntityDObjAnimTree +
// CG_DOBJ_GET_HANDLE preamble, identical origin/oldorigin = lerpOrigin and
// AnglesToAxisNegRight(axis, lerpAngles). It DIFFERS in the render-entity
// head/tail it fills:
//   * CG_General sets reType(+0x00)=RT_MODEL and defers renderfx/lightingOrigin
//     to CG_SetupWeaponLightingOrigin, storing handle@+0x90 / owner@+0x94.
//   * This handler writes an EXPLICIT head — renderfx(+0x04)=RF_LIGHTING_ORIGIN
//     and lightingOrigin(+0x0c) = {wA0, wA1, wA2 + 32.0f} — instead of calling
//     CG_SetupWeaponLightingOrigin.
// The render entity is the ordinary refEntity_t. Its base is the pre-syscall
// `ESP+8` established by LEA at 0x3001ed0c and passed at 0x3001ed6b. The two
// syscall pushes at 0x3001ed6f/70 move ESP down by eight bytes before the final
// stores, so [ESP+0x10], [ESP+0xa0], and [ESP+0xa4] are respectively re+0x00,
// re+0x90, and re+0x94 — not re+0x08, re+0x98, and re+0x9c.
//
// Behavior proven from the bytes:
//   1. 0x3001eca0-ac / 0x3001ed91-a2: /GS frame — AND ESP,-8; SUB ESP,0xa8;
//      snapshot __security_cookie into the aligned frame; verify via
//      __security_check_cookie (0x30061639) on exit. Not source-level; omitted.
//   2. 0x3001ecb8: MOV AL,[EBX+8]; TEST AL,AL; JS -> low byte of cent->currentState.eFlags
//      has bit 7 set (EF_NODRAW = 0x80) -> skip everything and return.
//   3. 0x3001ecc5: CG_RefreshEntityDObjAnimTree(cent->currentState.eType,
//      cg_gameModels[cent->currentState.modelIndex]) with ESI = cent->currentState.number
//      (register arg). Caller-cleaned (plain RET); its two pushed args' 8 bytes
//      are folded into the ADD ESP,0x10 after the following trap-0xa5 call.
//   4. 0x3001ece1: handle = cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number)
//      (PUSH [EBX]; PUSH 0xa5; CALL [cgame_syscall]; ADD ESP,0x10). ESI = handle.
//      TEST ESI,ESI; JZ -> no DObj skeleton, nothing to draw; return.
//   5. 0x3001ecf9-0x3001ed66: build the on-stack render entity. REP STOSD zeroes
//      0x27 (39) dwords = 0x9c bytes (memset), then:
//        - renderfx      = RF_LIGHTING_ORIGIN (0x80)              [re+0x04]
//        - lightingOrigin = { wA[0], wA[1], wA[2] + 32.0f }        [re+0x0c]
//              (0x3001ecf9 FLD [ebx+0x208]; ...; 0x3001ed46 FADD [0x3007bdd0] =
//               +32.0f; the wA[2] copy is used for both lightingOrigin.z and
//               oldorigin.z/origin.z)
//        - axis          = AnglesToAxisNegRight(&re.axis@+0x1c,
//                            &cent->lerpAngles@+0x214)        [re+0x1c]
//        - origin        = cent->lerpOrigin                      [re+0x44]
//        - oldorigin     = cent->lerpOrigin                      [re+0x54]
//   6. After the two syscall pushes, 0x3001ed72/0x3001ed79/0x3001ed80 store
//      dobj=handle [re+0x90], owner=cent [re+0x94], and
//      reType=RT_MODEL(1) [re+0x00].
//   7. 0x3001ed88: trap_R_AddRefEntityToScene(&re) (PUSH &re; PUSH 0x3d).
//
// The register-arg ABI (cent in EBX) and the /GS cookie save/verify are i386
// calling-convention details, recorded here and expressed as plain C per the
// contract.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include <string.h>

/* .rdata 0x3007bdd0 = 0x42000000 = 32.0f: added to lerpOrigin[2] to form the
 * lighting-origin Z used for this eType-11 model entity. */
#define CG_ET11_LIGHTING_Z_OFFSET 32.0f

void CG_AddCEntity_ET11(centity_t *cent /* EBX */)
{
    refEntity_t re;
    struct DObj_s *dobj;

    /* 0x3001ecb8: MOV AL,[EBX+8]; TEST AL,AL; JS -> eFlags bit 7 = EF_NODRAW. */
    if (cent->currentState.eFlags & EF_NODRAW)
        return;

    int32_t modelIndex = cent->currentState.modelIndex;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)modelIndex >= (uint32_t)CS_MODELS_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_AddCEntity_ET11: invalid model index %i",
                  modelIndex);
        return;
    }
    qhandle_t modelHandle = cg_gameModels[modelIndex];
    int32_t eType = cent->currentState.eType;
    int32_t entityNum = cent->currentState.number;
    CG_RefreshEntityDObjAnimTree(entityNum, eType, modelHandle);

    /* 0x3001ece1: handle = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number). */
    entityNum = cent->currentState.number;
    dobj = (struct DObj_s *)cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);
    /* 0x3001ecf1: TEST ESI,ESI; JZ -> no DObj skeleton, nothing to draw. */
    if (dobj == NULL)
        return;

    /* 0x3001ecf9/0x3001ecff: X remains live in x87 across the REP clear, while
     * Y is independently captured as a raw dword for the later oldorigin copy.
     * Z is deliberately not read until after the clear. */
    long double xCarrier = (long double)cent->lerpOrigin[0];
    uint32_t yBits;
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));

    /* 0x3001ed05..0x3001ed10: REP STOSD zeroes 0x27 (39) dwords = 0x9c bytes. */
    memset(&re, 0, sizeof(re));

    /* 0x3001ecf9..0x3001ed5e: reproduce the interleaved raw copies and the one
     * x87 Z addition. The original publishes renderfx only after those stores. */
    re.origin[0] = (float)xCarrier;
    long double yCarrier = (long double)cent->lerpOrigin[1];
    re.origin[1] = (float)yCarrier;
    uint32_t zBits;
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));
    re.lightingOrigin[0] = (float)xCarrier;
    re.lightingOrigin[1] = (float)yCarrier;
    memcpy(&re.origin[2], &zBits, sizeof(zBits));
    memcpy(&re.oldorigin[1], &yBits, sizeof(yBits));
    memcpy(&re.oldorigin[2], &zBits, sizeof(zBits));
    uint32_t xBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&re.oldorigin[0], &xBits, sizeof(xBits));
    re.lightingOrigin[2] = (float)(
        (long double)re.origin[2] + (long double)CG_ET11_LIGHTING_Z_OFFSET);
    re.renderfx = RF_LIGHTING_ORIGIN;

    /* 0x3001ed4c/0x3001ed52/0x3001ed66: axis = AnglesToAxisNegRight(re.axis,
     * cent->lerpAngles). EAX = &re.axis (+0x1c), EDX = &lerpAngles. */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);

    /* 0x3001ed72/0x3001ed79: after the two syscall pushes, [ESP+0xa0] and
     * [ESP+0xa4] address the original re+0x90 and re+0x94. */
    re.dobj = dobj;
    re.owner = cent;
    re.reType = RT_MODEL;

    /* 0x3001ed88: trap_R_AddRefEntityToScene(&re) (PUSH &re; PUSH 0x3d). */
    trap_R_AddRefEntityToScene(&re);
}
