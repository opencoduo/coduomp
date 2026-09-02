// Source: uo_cgame_mp_x86.dll 0x3001e680..0x3001e7ea
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e680_3001e7ea.mcode
//
// CG_Item — the CG_AddCEntity item-entity handler. Sibling of CG_General
// (0x3001e430): both are register-ABI (centity in EBX), /GS-protected handlers
// dispatched from the eType jump table reached via 0x30022170; this one is the
// arm at 0x300221b7 (`CALL 0x3001e680; pop..; ret`). It draws a dropped/world
// item entity: it lazily registers the item's visuals, refreshes its DObj anim
// tree, queries the item's DObj handle, and submits one RT_MODEL render entity.
//
// Naming: the mechanical .mcode name "SP_func_bobbing" is REJECTED. That was a
// broad-corpus size guess (win size 0x16a == matched 0x16a), the exact
// size-matching the contract forbids. This is not a server entity-spawn function:
//   * it takes a centity in EBX (client render dispatch), not a gentity;
//   * it emits the two client item-render diagnostics "Bad item index %i on
//     entity" (0x30077310) and "No XModel loaded for item index %i (%s)"
//     (0x300772e8) via Com_ErrorMessage;
//   * it builds an on-stack refEntity_t (RT_MODEL) and submits it with
//     trap_R_AddRefEntityToScene (trap 0x3d), exactly like CG_General;
//   * it drives CG_RegisterItemVisuals (0x30044ac0) over cg_items, the CoD cgame
//     per-item registered-visuals cache.
// The same-module PPC bank confirms cgame_mp!CG_Item exists. The behavioral role
// (item-entity render handler) is CG_Item; adopted as the source name.
//
// The item definition table at .data 0x300827a0 is bg_itemlist: base +
// itemIndex*0x30, with pickupName at +0x18 and type at +0x20. The weapon test is
// therefore item->type == IT_WEAPON.
//
// Behavior proven from the bytes:
//   1. /GS frame: snapshot __security_cookie into the frame; verify via
//      __security_check_cookie (0x30061639) on both return paths. Not source-level.
//   2. 0x3001e698: if cent->currentState.itemIndex >= 134 (CMP EAX,0x86; JL skips), report
//      Com_ErrorMessage("Bad item index %i on entity", itemIndex) and continue.
//   3. 0x3001e6b5: MOV AL,[EBX+8]; TEST AL,AL; JS -> if eFlags bit 7 (EF_NODRAW)
//      set, skip everything and return (via the /GS-checked exit).
//   4. 0x3001e6c0: itemInfo = &cg_items[itemIndex] (0x304531a0, stride 0x24);
//      item = &bg_itemlist[itemIndex] (0x300827a0, stride 0x30). If the item's
//      registered flag (itemInfo->registered, +0x00, the dword loaded at 0x3001e6cc
//      and TESTed at 0x3001e6e3) is 0, call CG_RegisterItemVisuals(itemIndex) then
//      return (via the /GS-checked exit at 0x3001e702). No first-frame draw.
//   5. 0x3001e708/0x3001e70b: if itemInfo->modelHandle (+0x04) is 0, report
//      Com_ErrorMessage("No XModel loaded for item index %i (%s)", itemIndex,
//      item->pickupName). Continues either way.
//   6. 0x3001e727: refresh the DObj anim tree:
//      CG_RefreshEntityDObjAnimTree(cent->currentState.eType, itemInfo->modelHandle) with
//      ESI = cent->currentState.number (register arg); caller-cleaned, its 8 bytes are folded
//      into the ADD ESP,0x10 after the following trap 0xa5 call.
//   7. 0x3001e736: handle = cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number)
//      (`PUSH [EBX]; PUSH 0xa5; CALL [cgame_syscall]; ADD ESP,0x10`). ESI = handle.
//      TEST ESI,ESI; JZ -> no DObj skeleton: skip the draw, return.
//   8. 0x3001e751..: build the on-stack refEntity_t: REP STOSD zero-fills 0x27 (39)
//      dwords = 0x9c = sizeof(refEntity_t), then:
//        - if item->type == IT_WEAPON: AnglesToAxisNegRight(
//          re.axis, cent->lerpAngles) and set the +0x40 scale slot to 1.0f;
//          otherwise: AnglesToAxisNegRight(re.axis, cent->lerpAngles) with no
//          scale write (both arms call the same axis builder; only the scale store
//          differs). EDX = &cent->lerpAngles (+0x214), EAX = &re.axis.
//        - origin    = cent->lerpOrigin (raw dword copies of +0x208/+0x20c/+0x210)
//        - oldorigin  = cent->lerpOrigin (same three dwords)
//        - re.dobj       = handle (ESI, store at re+0x90)
//        - re.owner      = cent   (EBX, store at re+0x94)
//        - re.reType     = RT_MODEL (1)
//   9. 0x3001e7cf: trap_R_AddRefEntityToScene(&re) (PUSH &re; PUSH 0x3d).
//
// The register-arg ABI (cent in EBX) and the /GS cookie save/verify are i386
// calling-convention details, recorded here and expressed as plain C per the
// contract; refEntity base = ESP+0x10 (pre-push frame).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include <string.h>

enum {
    CG_ITEM_INDEX_LIMIT = 134,  /* 0x86: CMP itemIndex,0x86; JL -> valid item index range is [0,133] */
};

void CG_Item(centity_t *cent /* EBX */)
{
    itemInfo_t *itemInfo;
    gitem_t *item;
    refEntity_t re;
    struct DObj_s *dobj;

    /* 0x3001e698: out-of-range item index is a client render diagnostic; execution
     * then continues into the eFlags check (Com_ErrorMessage does not return here in
     * source terms but the instruction stream falls through). */
    if (cent->currentState.itemIndex >= CG_ITEM_INDEX_LIMIT)
        Com_ErrorMessage("Bad item index %i on entity", cent->currentState.itemIndex);

    /* 0x3001e6b5: MOV AL,[EBX+8]; TEST AL,AL; JS -> eFlags bit 7 = EF_NODRAW. */
    if (cent->currentState.eFlags & EF_NODRAW)
        return;

    /* 0x3001e6c0..0x3001e6e9: address both per-item tables by the item index.
     * cg_items (0x304531a0, itemInfo_t, stride 0x24): ESI = &cg_items[idx].
     * bg_itemlist (0x300827a0, gitem_t, stride 0x30): ECX = &bg_itemlist[idx]. */
    itemInfo = &cg_items[cent->currentState.itemIndex];
    item = &bg_itemlist[cent->currentState.itemIndex];

    /* 0x3001e6cc/0x3001e6e3: TEST EAX,EAX on cg_items[idx][+0x00] = itemInfo->registered.
     * If not registered yet, register the item's visuals and return this frame (no draw). */
    if (itemInfo->registered == 0) {
        CG_RegisterItemVisuals(cent->currentState.itemIndex);
        return;
    }

    /* 0x3001e708/0x3001e70b: a registered item whose model handle (+0x04) is still 0
     * is a data error (the XModel failed to load); report it and continue. This is a
     * SECOND, distinct null-check on +0x04, not the +0x00 registered flag above. */
    if (itemInfo->modelHandle == 0)
        Com_ErrorMessage("No XModel loaded for item index %i (%s)", cent->currentState.itemIndex, item->pickupName);

    /* 0x3001e727: (re)bind the entity's DObj weapon anim tree. First stack arg is
     * cent->currentState.eType, second is itemInfo->modelHandle; entityNum is passed in ESI. */
    CG_RefreshEntityDObjAnimTree(cent->currentState.number, cent->currentState.eType, itemInfo->modelHandle);

    /* 0x3001e736: handle = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number). */
    dobj = (struct DObj_s *)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number);
    /* 0x3001e749: TEST ESI,ESI; JZ -> no DObj skeleton, nothing to draw. */
    if (dobj == NULL)
        return;

    /* 0x3001e751..0x3001e75c: REP STOSD zeroes 0x27 (39) dwords = 0x9c = sizeof(re). */
    memset(&re, 0, sizeof(re));

    /* 0x3001e75e..0x3001e78c: axis = AnglesToAxisNegRight(re.axis,
     * cent->lerpAngles). The item->type == IT_WEAPON arm
     * additionally writes the float scale slot at re+0x40 = 1.0f; the non-weapon arm
     * leaves it zeroed. Both arms call the same axis builder (EAX = &re.axis,
     * EDX = &cent->lerpAngles). */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);
    if (item->type == IT_WEAPON) {
        /* 0x3001e77d: MOV dword [re+0x40],0x3f800000. The executable's
         * R_RotateForModelEntity reads this shared ABI slot with FLD, proving
         * that the 1.0f store is typed float rather than an integer flag. */
        re.nonNormalizedAxes = 1.0f;
    }

    /* 0x3001e78c..0x3001e7b9: three raw source dwords are loaded, then published
     * X/X/Y/Z/Y/Z. Preserve that graph explicitly; it also avoids treating a
     * signaling-NaN payload as an arithmetic float operation on native hosts. */
    uint32_t xBits;
    uint32_t yBits;
    uint32_t zBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));
    memcpy(&re.origin[0], &xBits, sizeof(xBits));
    memcpy(&re.oldorigin[0], &xBits, sizeof(xBits));
    memcpy(&re.origin[1], &yBits, sizeof(yBits));
    memcpy(&re.origin[2], &zBits, sizeof(zBits));
    memcpy(&re.oldorigin[1], &yBits, sizeof(yBits));
    memcpy(&re.oldorigin[2], &zBits, sizeof(zBits));

    /* 0x3001e7bd/0x3001e7c4/0x3001e7cb: DObj handle, owning centity, and reType. */
    re.dobj = dobj;
    re.owner = cent;
    re.reType = RT_MODEL;

    /* 0x3001e7cf: trap_R_AddRefEntityToScene(&re). */
    trap_R_AddRefEntityToScene(&re);

    /* 0x3001e7d8..0x3001e7e9: /GS cookie verify + epilogue (omitted). */
}
