// Source: uo_cgame_mp_x86.dll 0x30044890..0x30044a07
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30044890_30044a07.mcode
//
// CG_RefreshWeaponDObjModelSet -- rebuild the first-person view-weapon DObj model
// set for one already-registered weapon whose config-string name has changed, and
// re-cache that name.
//
// NAMING ADJUDICATION: the .mcode header's "AxisToAngles" is a pure size-match
// guess (win size 0x177 == this function's 0x177) with ZERO behavioral basis and
// is REJECTED. There is no matrix/atan2/asin math anywhere in the body: it formats
// "xmodel/<name>" strings, registers XModels, wraps them into engine DObj model
// objects (CG_DOBJ_WRAP_MODEL, trap 0x32), rebuilds the two-element tagged model
// set (CG_CLIENT_DOBJ_CREATE, trap 0xa7), re-acquires the weapon's DObj handle
// (CG_DOBJ_GET_HANDLE, trap 0xa5), and copies the new config name into the
// cgWeaponInfo record. This is weapon-DObj refresh, distinct from the corpus
// AxisToAngles at 0x3004c2a0 (which is a real 3x3-axis->euler routine referenced
// by the dObj cluster / CG_PlayerVehiclePositionAndBlend).
//
// ABI (proven from the sole caller CG_RefreshWeaponInfosForConfigString,
// 0x30044a10): register-argument client ABI.
//   ECX          = weaponIndex  -- 1..bg_numWeapons; used to index bg_weaponInfos[]
//                                  and cg_weaponInfos[] (stride 0x1c4).
//   arg0 (stack) = configName   -- the incoming weapon config-string name; the
//                                  caller pushes it (PUSH EBX) and cleans one dword
//                                  (ADD ESP,4) => cdecl one-stack-arg.
// The function is caller-cleaned; EBX/EDI/EBP/ESI save/restore and the /GS cookie
// on the 0xa8-byte frame are calling-convention detail.
//
// Every behavior-affecting statement is checked against the .mcode / objdump.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

/* Base of the per-weapon DObj/model handle key band: the engine keys this weapon's
 * view-DObj registration under (weaponIndex + 0x400) (0x300448ea ADD EDI,0x400).
 * Same band CG_FreeWeaponDObjHandles (0x30044a80) releases with LEA [ESI+0x400];
 * see CG_VIEW_WEAPON_DOBJ_HANDLE_BASE in the shared cgame declarations. */

/* Q_strncpyz destsize used for the cached weapon name at cgWeaponInfo.name (+0x64):
 * a 0x3f-byte copy, after which the function forces a NUL at name[0x3f]. */
enum {
    CG_WEAPON_NAME_COPY_SIZE = 0x3f
};

/* The engine model-set handed to CG_CLIENT_DOBJ_CREATE here is exactly two
 * DObjModel elements (count 0x2 at 0x300449a8): element 0 is the view
 * gun/hand model (no attach tag), element 1 is the same model attached at
 * "tag_weapon". */
enum {
    CG_WEAPON_DOBJ_MODEL_SET_COUNT = 2
};

void CG_RefreshWeaponDObjModelSet(int weaponIndex, const char *configName)
{
    /* 0x3004489f..0x300448aa: weaponIndex 0 is the null slot -> nothing to do. */
    if (weaponIndex == 0) {
        return;
    }

    /* 0x300448b0/0x300448b6: bg_weaponInfos[weaponIndex] holds this weapon's parsed
     * weaponInfo_t (gunModel/handModel names live here). */
    weaponInfo_t *weapon = bg_weaponInfos[weaponIndex];

    /* 0x300448ba..0x300448c2: &cg_weaponInfos[weaponIndex] (0x1c4-byte stride). */
    cgWeaponInfo_t *info = &cg_weaponInfos[weaponIndex];

    /* 0x300448c8..0x300448cc: only refresh a weapon that already has a live view
     * DObj self handle; an unregistered slot (viewDObjSelf == 0) is skipped. */
    if (info->viewDObjSelf == 0) {
        return;
    }

    /* 0x300448d2..0x300448d7: and only when the weapon actually declares a view
     * gun model ("gunModel" key non-empty). */
    if (weapon->gunModel[0] == '\0') {
        return;
    }

    /* 0x300448dd..0x300448f6: resolve this weapon view DObj's runtime tree before
     * rebuilding the model set. The returned tree is retained and later forwarded
     * to CG_CLIENT_DOBJ_CREATE. */
    intptr_t runtimeTree = cgame_syscall(CG_DOBJ_GET_TREE, (intptr_t)info->viewDObjSelf);

    /* 0x300448ea ADD EDI,0x400: preserve the target dword result once and use
     * that exact handle for release, rebuild, and reacquisition. */
    int32_t dobjHandle = coduo_int32_from_bits((uint32_t)weaponIndex + (uint32_t)CG_VIEW_WEAPON_DOBJ_HANDLE_BASE);

    /* 0x300448e9..0x300448fa: release the weapon's existing DObj model registration
     * (keyed by weaponIndex + the handle band base), flag 0. */
    (void)cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, dobjHandle, 0);

    /* The two-element model set assembled on the stack and handed to
     * CG_CLIENT_DOBJ_CREATE below. */
    /* The PE32 producer writes every consumed lane below but leaves each
     * abiGap_00a word untouched; CoDUOMP.exe skips that word. */
    DObjModel modelSet[CG_WEAPON_DOBJ_MODEL_SET_COUNT];

    /* ---- element 0: the hands XModel, no attach tag ---- */
    /* 0x30044900..0x30044913: sprintf(handModelPath, "%s%s", "xmodel/",
     * weapon->handModel). */
    char handModelPath[MAX_QPATH];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (strlen(weapon->handModel) > sizeof(handModelPath) - sizeof("xmodel/") || strlen(weapon->gunModel) > MAX_QPATH - sizeof("xmodel/")) {
        Com_Error(ERR_DROP, "\x15"
                            "CG_RefreshWeaponDObjModelSet: model path is too long");
        return;
    }
    Com_sprintf(handModelPath, sizeof(handModelPath), "xmodel/%s", weapon->handModel);

    /* 0x30044918..0x30044932: register the model. When the incoming configName is
     * non-empty the machine code registers that name directly; otherwise it falls
     * back to the just-built "xmodel/<handModel>" path. Category 0x6.
     * NOTE(arg order): the bytes push the name pointer LAST (PUSH 0x6; PUSH name),
     * i.e. the model NAME is the cdecl first argument and 0x6 the second. The shared
     * CG_RegisterModel decl and the sibling reconstructions (CG_BuildCorpseDObjModels
     * et al.) express the arguments in (category, name) order; kept consistent with
     * the corpus decl here. */
    modelSet[0].modelIndex = (int16_t)CG_RegisterModel(configName[0] != '\0' ? configName : handModelPath, 6);

    /* 0x3004493a..0x3004495f: register the hands XModel path itself and wrap the
     * resulting handle into an engine DObj model object; that object is element 0.
     * Element 0 has no attach tag (tagName NULL) and flags 0. */
    int16_t handModelIndex = (int16_t)CG_RegisterModel(handModelPath, 6);
    modelSet[0].model = (XModel *)(intptr_t)cgame_syscall(CG_DOBJ_WRAP_MODEL, handModelIndex);
    modelSet[0].tagName = 0;             /* 0x3004495b (base+0x04 = 0) */
    modelSet[0].ignoreCollision = 0;     /* 0x3004495f (base+0x0c = 0) */

    /* ---- element 1: the gun XModel, attached at "tag_weapon" ---- */
    /* 0x30044963..0x30044979: sprintf(gunModelPath, "%s%s", "xmodel/",
     * weapon->gunModel). */
    char gunModelPath[MAX_QPATH];
    Com_sprintf(gunModelPath, sizeof(gunModelPath), "xmodel/%s", weapon->gunModel);

    /* 0x3004497e..0x30044998: register it, store the 16-bit handle in the element,
     * and wrap the handle into a DObj object stored as the element's object. */
    modelSet[1].modelIndex = (int16_t)CG_RegisterModel(gunModelPath, 6);
    modelSet[1].model = (XModel *)(intptr_t)cgame_syscall(CG_DOBJ_WRAP_MODEL, modelSet[1].modelIndex);
    modelSet[1].tagName = "tag_weapon"; /* 0x300449b4 (0x30079a7c) */
    modelSet[1].ignoreCollision = 0;    /* 0x300449bc */

    /* 0x300449a3..0x300449c0: commit the rebuilt two-element model set to the
     * engine, forwarding the tag-setup state token captured above. (The 0xa7 return
     * is discarded.) */
    (void)cgame_syscall(CG_CLIENT_DOBJ_CREATE, (intptr_t)modelSet, CG_WEAPON_DOBJ_MODEL_SET_COUNT, runtimeTree, dobjHandle);

    /* 0x300449c9..0x300449de: re-acquire the weapon's DObj self handle for the
     * refreshed registration and cache it back into the record. */
    info->viewDObjSelf = (struct DObj_s *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, dobjHandle);

    /* 0x300449d5..0x300449ed: cache the new config name (bounded to 0x3f bytes,
     * then a forced NUL at name[0x3f]). */
    Q_strncpyz(info->name, configName, CG_WEAPON_NAME_COPY_SIZE);
    info->name[CG_WEAPON_NAME_COPY_SIZE] = '\0';
}
