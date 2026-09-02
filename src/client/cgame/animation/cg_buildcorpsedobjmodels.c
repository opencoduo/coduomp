// Source: uo_cgame_mp_x86.dll 0x300058f0..0x30005b50
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300058f0_30005b50.mcode
//
// CG_BuildCorpseDObjModels -- (re)build the DObj model set for one corpse
// model-part and commit it to the engine.
//
// NAMING ADJUDICATION: the .mcode header carries the mechanical working name
// "G_FreeVehicle", assigned ONLY by a corpus size match (win 0x260 ~ 0x261).
// That is REJECTED: this function does not free a vehicle. The machine code
// registers a base model plus up to six attach models (CG_RegisterModel,
// 0x3003d940), wraps each registered handle into an engine DObj model object
// (CG_DOBJ_WRAP_MODEL, trap 0x32), optionally appends the corpse's held-weapon
// world model (cg_weaponInfos[weaponIndex].worldModelHandle), releases any prior
// registration (CG_SAFE_CLIENT_DOBJ_FREE, trap 0xa8), and commits the whole tagged
// set to the engine (CG_CLIENT_DOBJ_CREATE, trap 0xa7). It caches the weapon
// index and a build-generation byte so an unchanged corpse skips the rebuild.
//
// ABI (proven from the caller CG_AddPlayerCorpseEntity, 0x300346c0 / 0x30034830):
// register-argument client ABI.
//   EBX = info        -- &cg_corpseInfo[modelPartIndex-0x40]
//                        (the 0x4d0 clientInfo_t table entry)
//   EDX = dobjHandle  -- the DObj handle from CG_DOBJ_GET_HANDLE (trap 0xa5)
//   arg0 (stack) = renderEntity  -- cent->corpseModelInfo (cent+0xf4)
//   arg1 (stack) = generationOut -- byte at cent+0x284; the record's build
//                                   generation is written here (and the function
//                                   short-circuits when it is already current).
// The function is cdecl caller-cleaned (plain RET; the caller does ADD ESP,8
// after the two stack pushes). EBP/ESI/EDI save/restore and the 0x8c-byte frame
// are calling-convention detail; the source-shaped body is below.
//
// Every behavior-affecting statement is checked against the .mcode / objdump.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

/*
 * The engine model-set array CG_CLIENT_DOBJ_CREATE receives holds at most one
 * base model + CLIENT_INFO_ATTACHMENT_COUNT (6) attach models + one weapon
 * model = 8 elements. The i386 builds it on the stack.
 */
enum { DOBJ_CORPSE_MODEL_SET_MAX = 1 + CLIENT_INFO_ATTACHMENT_COUNT + 1 };

void CG_BuildCorpseDObjModels(clientInfo_t *info, intptr_t dobjHandle,
                              entityState_t *renderEntity,
                              uint8_t *generationOut)
{
    /* EDI at entry = (dobjHandle != 0). 0x300058f9..0x30005902:
     * XOR/CMP EDX,0; SETNZ AL; MOV EDI,EAX. */
    const int haveExistingDObj = (dobjHandle != 0);

    /* [ESP+0xc] weapon index: renderEntity->weapon, kept only when this
     * corpse's flags/pose say it carries a weapon; otherwise forced to 0.
     * 0x3000590b MOV ECX,[renderEntity+0xcc]; 0x30005911 store. */
    uint32_t weaponIndex = renderEntity->weaponIndex;

    /* 0x30005915 TEST [renderEntity+8],0x106000; only entities whose flags carry
     * EF_RESTRICTED_MASK evaluate the pose gate.  When those flags are
     * CLEAR (a normal on-foot player), the JZ at 0x3000591c targets 0x3000593c --
     * the merge point PAST the clear at 0x30005938 -- so weaponIndex is KEPT and
     * the held-weapon world model is attached below.  There is NO else-clear in
     * the machine code.  (A prior pass misread the JZ target as 0x30005938 and
     * added a spurious `else { weaponIndex = 0; }`, which dropped the weapon model
     * for every on-foot player -- the "no weapon on enemy players" bug.) */
    if (renderEntity->eFlags & EF_RESTRICTED_MASK) {
        /* 0x3000591e..0x30005936: keep the weapon index only for the specific pose
         * (poseType & 0x38)==0x8 AND (poseType & 0x7)==0x3. Both byte compares. */
        const uint32_t poseType = renderEntity->poseType;
        if (((poseType & 0x38u) == 0x8u) && ((poseType & 0x7u) == 0x3u)) {
            /* keep weaponIndex */
        } else {
            weaponIndex = 0; /* 0x30005938 MOV [ESP+0xc],0 */
        }
    }

    /* 0x3000593c: an invalid record (infoValid == 0) or an empty base model
     * name means there is nothing to build -- release any prior DObj and return.
     * 0x30005944 loads the first byte of modelName; ESI = &modelName. */
    if (info->infoValid == 0 || info->modelName[0] == '\0') {
        /* 0x30005b33 error/cleanup tail: CG_SAFE_CLIENT_DOBJ_FREE(handleKey, 1). */
        cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, renderEntity->numberBits, 1);
        return;
    }

    /* 0x30005952: when a DObj already exists, decide whether nothing changed and
     * we can bail without rebuilding. 0x3000597f otherwise releases it and rebuilds. */
    if (haveExistingDObj) {
        if (info->dobjSavedModel == weaponIndex &&
            info->dobjNeedsUpdate == 0 &&
            info->dobjVersion == *generationOut) {
            /* 0x30005979 JZ -> 0x30005b46: unchanged; return with no work and no
             * generation write. */
            return;
        }
        /* 0x3000597f: release the existing registration before rebuilding.
         * CG_SAFE_CLIENT_DOBJ_FREE(handleKey = renderEntity->number, 0). */
        cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, renderEntity->numberBits, 0);
    }

    /* --- build the model set on the stack --- */
    DObjModel elements[DOBJ_CORPSE_MODEL_SET_MAX];

    /* 0x30005991: runtime animation tree saved for the commit trap. */
    XAnimTree *animTree = info->animTree;

    /* Base model = element 0. 0x30005997: CG_RegisterModel(7, modelName) for the
     * 16-bit handle; a zero handle warns "Could not load model '%s'". A second
     * CG_RegisterModel call (0x300059c3) is wrapped by CG_DOBJ_WRAP_MODEL into the
     * element's engine object. Both registrations are emitted by the original. */
    int16_t modelIndex = (int16_t)CG_RegisterModel(info->modelName, 7);
    elements[0].modelIndex = modelIndex;
    if (modelIndex == 0) {
        Com_Error(ERR_DROP, bg_couldNotLoadModelErrorFormat,
                           info->modelName);
    }
    elements[0].model =
        (XModel *)(intptr_t)cgame_syscall(
            CG_DOBJ_WRAP_MODEL, CG_RegisterModel(info->modelName, 7));
    elements[0].tagName = 0;   /* 0x300059d1 [ESP+0x1c]=0 */
    elements[0].ignoreCollision = 0; /* 0x300059d5 [ESP+0x24]=0 */

    /* EBP tracks the element count; element 0 already placed, so count starts at 1.
     * 0x300059e0 MOV EBP,1. */
    uint32_t count = 1;

    /* Attach models: clientInfo_t.attachModelNames[0..5], 0x40 stride. Only
     * nonempty slots are added. 0x300059e9..0x30005a56 loop. */
    for (int i = 0; i < CLIENT_INFO_ATTACHMENT_COUNT; ++i) {
        if (info->attachModelNames[i][0] == '\0') {
            continue; /* 0x300059f7 CMP [EDI],0; JZ skip */
        }
        int16_t attachModelIndex =
            (int16_t)CG_RegisterModel(info->attachModelNames[i], 7);
        elements[count].modelIndex = attachModelIndex;
        if (attachModelIndex == 0) {
            Com_Error(ERR_DROP, bg_couldNotLoadModelErrorFormat,
                               info->attachModelNames[i]);
        }
        elements[count].model =
            (XModel *)(intptr_t)cgame_syscall(
                CG_DOBJ_WRAP_MODEL,
                CG_RegisterModel(info->attachModelNames[i], 7));
        /* 0x30005a34 addresses the same-index entry in the parallel tag array. */
        elements[count].tagName = info->attachTagNames[i];
        elements[count].ignoreCollision = 0; /* 0x30005a3f [ESI+0xc]=0 */
        ++count;                   /* 0x30005a46 INC EBP */
    }

    /* Held-weapon model. 0x30005a58..0x30005acb. Added only when a weapon index is
     * kept AND the record's clientNum still matches this render entity's
     * modelPartIndex, AND the weapon has a world model handle. */
    if (weaponIndex != 0 &&
        renderEntity->numberBits == (uint32_t)info->clientNum) {
        /* 0x30005a70 IMUL 0x1c4; ADD 0x30413580: cg_weaponInfos[weaponIndex]. */
        const cgWeaponInfo_t *weaponInfo = &cg_weaponInfos[weaponIndex];
        qhandle_t worldModel = weaponInfo->worldModelHandle;
        if (worldModel != 0) {
            /* The weapon's world model handle is wrapped directly (no
             * CG_RegisterModel). 0x30005a86 PUSH worldModel; PUSH 0x32. */
            elements[count].model =
                (XModel *)(intptr_t)cgame_syscall(
                    CG_DOBJ_WRAP_MODEL, worldModel);
            /* 0x30005a94 MOV DX,[weaponInfo+0xbc]: low 16 bits as the handle. */
            elements[count].modelIndex = (int16_t)worldModel;
            /* 0x30005a9f..0x30005ab6: gunHandLeft picks the attach tag side.
             * The default (fall-through, gunHandLeft != 0) loads 0x3007155c =
             * "tag_weapon_left"; the JNE-skipped arm (gunHandLeft == 0) loads
             * 0x30071548 = "tag_weapon_right".  (A prior pass swapped these two,
             * attaching the weapon to the wrong-hand tag.) */
            elements[count].tagName = (info->gunHandLeft != 0)
                                          ? bg_leftWeaponTagName
                                          : bg_rightWeaponTagName;
            elements[count].ignoreCollision = 0; /* 0x30005abf [ESP+ESI+0x24]=0 */
            ++count;                   /* 0x30005acb INC EBP */
        }
    }

    /* Commit the model set. 0x30005acc..0x30005ae9:
     * cgame_syscall(0xa7, &elements, count, animTree, renderEntity->number).
     * The count is passed as a 16-bit value (MOVZX EAX,BP). */
    cgame_syscall(CG_CLIENT_DOBJ_CREATE, (intptr_t)&elements[0],
                  (intptr_t)(uint16_t)count, (intptr_t)animTree,
                  (int)renderEntity->numberBits);

    /* 0x30005aef snapshots the rebuild word immediately after the trap, before
     * the cached weapon-index store at 0x30005afa. */
    int32_t rebuildRequested = info->dobjNeedsUpdate;

    /* 0x30005afa: record the weapon index used for this build. */
    info->dobjSavedModel = weaponIndex;

    /* 0x30005aef..0x30005b14: a requested rebuild bumps the generation byte and
     * clears the request. INC AL computes the wrapped byte first, but retail
     * clears rebuildRequested before publishing the new generation byte. */
    if (rebuildRequested != 0) {
        uint8_t nextGeneration = (uint8_t)(info->dobjVersion + 1u); /* INC AL */
        info->dobjNeedsUpdate = 0;                                 /* [EBX+0x404]=0 */
        info->dobjVersion = nextGeneration;                         /* 0x30005b14 */
    }

    /* 0x30005b1a..0x30005b29: publish the current generation to the caller. */
    uint8_t generation = info->dobjVersion;
    *generationOut = generation;
}
