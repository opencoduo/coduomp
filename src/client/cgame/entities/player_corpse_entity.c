// Source: uo_cgame_mp_x86.dll 0x300346c0..0x3003482f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300346c0_3003482f.mcode
//
// CG_AddPlayerCorpseEntity (role name) — the eType == ET_PLAYER_CORPSE (== 2)
// member of the cgame per-entity render dispatch table (jump table at
// 0x30022228, dispatched by the eType switch at 0x30022170 on
// currentState.eType). CoD:UO's ET_PLAYER_CORPSE is the "script cloneplayer
// corpse" entity (server entityType_t). This handler drives the corpse's DObj
// model-part effect state, then builds and submits a model refEntity for the
// corpse to the render scene.
//
// The `.mcode` header's size-matched guess "CG_AdjustPositionForMover" is
// REJECTED: this function performs no mover-position math. It queries the DObj
// skeleton handle (CG_DOBJ_GET_HANDLE, trap 0xa5), runs the DObj model-part
// effect update, composes an axis from the entity's render angles, and submits a
// model refEntity via CG_R_ADD_REF_ENTITY_TO_SCENE (trap 0x3d). It is one leaf of
// the eType render dispatcher, not a mover helper. The exact CoD source symbol is
// unproven (no cgame symbol table recovered), so this keeps a behavioral name.
//
// The sole caller (0x300221aa) is the eType==2 arm of the render dispatcher:
// `push ebx; call 0x300346c0; add esp,4`, i.e. one caller-cleaned int32 stack
// argument (the cg_entities[] record pointer, EBX in the dispatcher). Modeled as
// the leading `cent` parameter.
//
// Instruction map (frame after AND ESP,~7 / SUB ESP,0xac; B = &modelInfo = cent+0xf4):
//   300346cc MOV EAX,__security_cookie ; 300346dc store to canary slot
//   300346d6 LEA ESI,[cent+0xf4]        ; B = &cent->corpseModelInfo
//   300346e3 MOV AL,[B+0x8]             ; low byte of modelInfo.eFlags
//   300346e6 TEST AL,AL / JS exit       ; bit 0x80 set (signed-negative) -> nothing to draw
//   300346ef MOV EAX,[B]                ; modelInfo.number
//   300346f1 LEA EBX,[EAX-0x40]         ; index = modelPartIndex - 0x40
//   300346f4 IMUL EBX,EBX,0x4d0         ; * sizeof(clientInfo_t)
//   30034708 ADD EBX,0x3044cb00         ; EBX = &cg_corpseInfo[index]
//   300346fa ADD ECX,0x284             ; ECX = &cent->corpseTagState (cent+0x284)
//   3003470{0,1,2,3} PUSH ECX,ESI,EAX,0xa5
//   3003470e CALL *cgame_syscall        ; trap(0xa5, modelPartIndex) -> dobjHandle
//   30034714 ADD ESP,8                  ; clean id + modelPartIndex; ESI,ECX stay as
//                                        ; the two stack args of the next call
//   30034717 MOV EDX,EAX                ; dobjHandle in EDX (register arg)
//   30034719 CALL 0x300058f0            ; CG_UpdateCorpseModelPartState(&modelInfo,
//                                        ;   &tagState) with EBX=corpseInfo, EDX=dobjHandle
//   3003471e MOV EAX,[B]                ; modelInfo.number again
//   3003472{0,1} PUSH EAX,0xa5 / CALL *cgame_syscall ; trap(0xa5, modelPartIndex)
//   3003472c ADD ESP,0x10               ; clean id+arg (8) and the two leftover
//                                        ; stack args from the first trap (8)
//   3003472f TEST EAX,EAX / 30034735 JZ exit ; no DObj handle -> draw nothing
//   30034731 MOV [dobjHandle slot],EAX  ; save the handle for refEntity.dobj
//   3003473b..30034746 REP STOSD 0x27 dwords from &re ; zero refEntity (0x9c bytes)
//   3003474b..30034760 store 0xff into re.shaderRGBA[0..3]  ; white
//   30034767 CALL 0x30005860            ; BG_PlayerAnimation(&modelInfo,corpseInfo)
//   3003476c MOV EDI,cent
//   30034778 LEA EDX,[cent+0x214] / LEA EAX,&re.axis / CALL 0x3004c200
//                                        ; AnglesToAxisNegRight(&re.axis, cent->renderAngles)
//   30034781 FLD [cent+0x208]           ; renderOrigin.x
//   30034787 MOV EAX,[cent+0x210]       ; renderOrigin.z (raw bits)
//   3003478d FST re.origin[0]
//   30034795 FLD [cent+0x20c]           ; renderOrigin.y
//   3003479b MOV EAX,[B+0x8]            ; modelInfo.eFlags (dword)
//   300347a0 FST re.origin[1]
//   300347a4 FLD ST(1) / FSTP re.lightingOrigin[0] ; = renderOrigin.x
//   300347aa FST re.lightingOrigin[1]              ; = renderOrigin.y
//   300347ae FLD re.origin[2]                       ; renderOrigin.z
//   300347b2 FADD [B+0xe8]              ; + modelInfo.fTorsoHeight
//   300347b8 JZ / flags bit tests: (flags&0x40)?+12 : (flags&0x20)?+20 : +32
//            (300347ba FADD [0x3007bdc4]=12.0f / 300347c6 FADD [0x3007be04]=20.0f
//             / 300347ce FADD [0x3007bdd0]=32.0f -- same map as the live-player
//             handler at 0x30034574/0x30034581/0x30034589)
//   300347d8 FSTP re.lightingOrigin[2]  ; adjusted Z
//   ... store re.origin[2]=renderOrigin.z into re.oldorigin as well (X,Y,Z) ...
//   300347f1 re.reType   = RT_MODEL (1)
//   30034807 re.renderfx = RF_LIGHTING_ORIGIN (0x80)
//   300347f9 re.dobj = dobjHandle
//   30034800 re.owner    = cent
//   30034813 CALL *cgame_syscall(0x3d, &re) ; submit the model to the scene
//   3003481c exit: __security_check_cookie(canary)
//
// All x87 is single float precision (FLD/FST/FADD on DWORD ptr). renderOrigin.z is
// carried through an int register (its raw bits) into both re.origin[2] and
// re.oldorigin[2], so it is copied bit-exact.

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Layout guards proving the offsets/stride this function relies on (4-byte i386
 * pointer width). cg_corpseInfo stride 0x4d0 matches IMUL ...,0x4d0.
 * The refEntity / centity offsets are asserted next to their definitions in
 * client_recovered.h; here we only assert the ones this file dereferences by name. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "corpse model-part stride");
#endif
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(centity_t, corpseModelInfo) == 0xf4, "corpseModelInfo +0xf4");
_Static_assert(offsetof(centity_t, corpseTagState) == 0x284, "corpseTagState +0x284");
_Static_assert(offsetof(centity_t, lerpOrigin) == 0x208, "lerpOrigin +0x208");
_Static_assert(offsetof(centity_t, lerpAngles) == 0x214, "lerpAngles +0x214");
#endif
_Static_assert(offsetof(entityState_t, number) == 0x00, "number +0x00");
_Static_assert(offsetof(entityState_t, eFlags) == 0x08, "eFlags +0x08");
_Static_assert(offsetof(entityState_t, fTorsoHeight) == 0xe8,
               "fTorsoHeight +0xe8");

/* cg_corpseInfo (0x3044cb00, 0x4d0-stride table indexed by
 * entity number - 0x40) is declared in client_recovered.h; storage in globals.c.
 *
 * CG_UpdateCorpseModelPartState / CG_UpdateCorpseModelPartState below is the
 * provisional caller-observed record for the register-arg callee at 0x300058f0.
 */

/*
 * CG_UpdateCorpseModelPartState (0x300058f0) — provisional, caller-observed ABI
 * only, superseded by its own .mcode reconstruction. Register-argument client ABI:
 * `corpseInfo` in EBX (the clientInfo_t table entry), `dobjHandle` in EDX;
 * two caller-supplied stack dwords: the corpse model-info sub-object (cent+0xf4)
 * and the corpse tag-state sub-object (cent+0x284). Internally it re-derives a
 * validity flag from `dobjHandle` and the model-info's entity-state fields
 * (+0xcc, +0x8 masked 0x106000, +0x88), then issues DObj traps (0xa8, 0x32) and
 * per-bone lookups over the model-part record to (re)bind the corpse's skeleton.
 * Only the ABI is recorded here. NOTE the unusual stack discipline: the caller's
 * first trap left ESI/ECX on the stack (ADD ESP,8 cleaned only id+arg), and those
 * two dwords ARE this function's stack arguments. */
/* Vertical (Z) offset added to the corpse's lighting origin, selected by the
 * model-info flag bits. Values are the .rdata float literals at 0x3007bdc4 (12.0),
 * 0x3007be04 (20.0), 0x3007bdd0 (32.0). Exact source meaning of the flag bits is
 * unproven; named by their proven role in this selector. */
#define CG_CORPSE_FLAG_LIGHTOFS_LOW  ((uint32_t)0x40) /* -> +12.0f lighting-origin Z */
#define CG_CORPSE_FLAG_LIGHTOFS_HIGH ((uint32_t)0x20) /* -> +20.0f lighting-origin Z */

void CG_AddPlayerCorpseEntity(centity_t *cent)
{
    entityState_t *modelInfo = &cent->corpseModelInfo; /* cent+0xf4 */

    /* EF_NODRAW is bit 0x80, the tested low-byte sign bit. */
    if ((modelInfo->eFlags & EF_NODRAW) != 0)
        return;

    uint32_t modelPartIndexBits = modelInfo->numberBits;
    int32_t modelPartIndex = coduo_int32_from_bits(modelPartIndexBits);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    uint32_t corpseIndex =
        modelPartIndexBits - (uint32_t)PLAYER_CLONE_ENTITYNUM_BASE;
    if (corpseIndex >= (uint32_t)PLAYER_CLONE_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_AddPlayerCorpseEntity: "
                  "invalid player clone entity %i",
                  modelPartIndex);
        return;
    }
    clientInfo_t *corpseInfo = &cg_corpseInfo[corpseIndex];

    /* First DObj handle query; its result and the two sub-objects drive the
     * model-part state (re)bind. trap(0xa5, modelPartIndex) -> handle. */
    intptr_t dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, modelPartIndex);
    CG_BuildCorpseDObjModels(corpseInfo, dobjHandle,
                             modelInfo, cent->corpseTagState);

    /* Re-query the DObj handle for the render entity; a zero handle means the
     * corpse has no live skeleton this frame, so draw nothing. */
    intptr_t renderDobjHandle =
        cgame_syscall(CG_DOBJ_GET_HANDLE, (int32_t)modelInfo->numberBits);
    if (renderDobjHandle == 0)
        return;

    refEntity_t re;
    /* REP STOSD zeroes 0x27 dwords = 0x9c bytes = the whole refEntity_t. */
    memset(&re, 0, sizeof(re));

    /* shaderRGBA = {255,255,255,255} (opaque white). */
    re.shaderRGBA[0] = 0xff;
    re.shaderRGBA[1] = 0xff;
    re.shaderRGBA[2] = 0xff;
    re.shaderRGBA[3] = 0xff;

    /* Advance the corpse's model-part effect emitters. */
    BG_PlayerAnimation(modelInfo, corpseInfo);

    /* Orientation basis from the corpse's render angles (cent+0x214). */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);

    /* World origin (cent+0x208); oldorigin gets the same point. renderOrigin.z is
     * carried through an int register, so origin.z / oldorigin.z are bit-exact. */
    float originX = cent->lerpOrigin[0];
    float originY = cent->lerpOrigin[1];
    int32_t originZBits;
    {
        float z = cent->lerpOrigin[2];
        memcpy(&originZBits, &z, sizeof originZBits);
    }
    re.origin[0] = originX;
    re.origin[1] = originY;
    memcpy(&re.origin[2], &originZBits, sizeof originZBits);

    /* lightingOrigin.xy = origin.xy; lightingOrigin.z = origin.z + groundOffset +
     * a flag-selected vertical bias. */
    uint32_t flags = modelInfo->eFlags;
    float originZ;
    memcpy(&originZ, &originZBits, sizeof originZ);

    re.lightingOrigin[0] = originX;
    re.lightingOrigin[1] = originY;
    /* One 80-bit chain, one rounding: 0x300347ae FLD origin.z; 0x300347b2 FADD
     * groundOffset; ONE flag-selected FADD (0x300347ba 12.0f [0x3007bdc4] /
     * 0x300347c6 20.0f [0x3007be04] / 0x300347ce 32.0f [0x3007bdd0]); the single
     * FSTP at 0x300347d8. A `lightZ +=` accumulator would round at every step. */
    if (flags & CG_CORPSE_FLAG_LIGHTOFS_LOW)
        re.lightingOrigin[2] = (float)(
            (long double)originZ + (long double)modelInfo->fTorsoHeight + 12.0L);
    else if (flags & CG_CORPSE_FLAG_LIGHTOFS_HIGH)
        re.lightingOrigin[2] = (float)(
            (long double)originZ + (long double)modelInfo->fTorsoHeight + 20.0L);
    else
        re.lightingOrigin[2] = (float)(
            (long double)originZ + (long double)modelInfo->fTorsoHeight + 32.0L);

    re.oldorigin[0] = originX;
    re.oldorigin[1] = originY;
    memcpy(&re.oldorigin[2], &originZBits, sizeof originZBits);

    re.reType = RT_MODEL;
    re.renderfx = (int32_t)RF_LIGHTING_ORIGIN;
    re.dobj = (struct DObj_s *)(uintptr_t)renderDobjHandle;
    re.owner = cent;

    cgame_syscall(CG_R_ADD_REF_ENTITY_TO_SCENE, &re);

    /* Epilogue restores ESP=EBP and runs __security_check_cookie(canary); that
     * canary bookkeeping is i386 /GS calling-convention detail, not source-level
     * behavior, so it is not modeled here. */
}
