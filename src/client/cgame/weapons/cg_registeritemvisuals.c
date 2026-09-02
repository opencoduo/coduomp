// Source: uo_cgame_mp_x86.dll 0x30044ac0..0x30044b87
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30044ac0_30044b87.mcode
//
// CG_RegisterItemVisuals(itemNum) — register (once) all render assets for a single
// item: its world model, icon model, HUD icon shader, and pickup sound. The result
// is cached in cg_items[itemNum] (itemInfo_t, 0x304531a0, stride 0x24) and the
// "registered" flag is set so a later call is a no-op.
//
// NAME ADJUDICATION: the .mcode header's mechanical pre-hint was "G_DebugCircle"
// purely from a size match (win/game size 0xc7 == 0xc7). REJECTED — this body is
// item asset registration: it contains NO x87 instructions, no sin/cos, and no
// debug-line traps. The real identity is proven by the call graph: this function is
// the lazy-init target invoked by CG_Item (0x3001e680) and CG_RegisterItems
// (config-string decode loop) whenever an item's cache slot is not yet registered,
// and it registers the item's models/icon/sound from bg_itemlist[itemNum].
//
// ABI: itemNum arrives in EAX (custom register-argument convention; the callers
// CG_Item / CG_RegisterWeapon compute the index into EAX before the CALL). The
// function pushes/pops ECX only to reserve one 4-byte stack local and returns void.
//
// The item table at .data 0x300827a0 is the shared bg_itemlist[] (gitem_t).

#include "client/cgame/client_recovered.h"

/* Model categories passed to CG_RegisterModel for the two model-name slots.
 * 0x30044b0f..b18: NEG EDX; SBB EDX,EDX; ADD EDX,7 computes 7 when the loop index
 * (EDI) is 0 and 6 when it is 1 — i.e. category 7 for worldModel, 6 for iconModel. */
enum {
    CG_ITEM_MODEL_CATEGORY_WORLD = 7, /* gitem_t.worldModel (+0x08) */
    CG_ITEM_MODEL_CATEGORY_ICON  = 6, /* gitem_t.iconModel  (+0x0c) */
    CG_ITEM_ICON_SHADER_PARAM    = 5  /* second arg to CG_RegisterShader for hudIcon */
};

void CG_RegisterItemVisuals(int itemNum)
{
    /* 0x30044ac2..ad5: index the per-item visuals cache (cg_items, stride 0x24 =
     * itemNum*9 dwords) and bail if this slot is already registered. */
    itemInfo_t *itemInfo = &cg_items[itemNum];
    if (itemInfo->registered != 0)
        return;

    /* 0x30044add..ae3: address this item's definition row (bg_itemlist[itemNum],
     * 0x300827a0 + itemNum*0x30) as a gitem_t. */
    const gitem_t *item = &bg_itemlist[itemNum];

    /* 0x30044aed: clear the registered flag while (re)building the entry. */
    itemInfo->registered = 0;

    /* 0x30044af6..b36: register the two item model names. The loop walks the two
     * consecutive const char * slots gitem_t.worldModel (+0x08) and
     * gitem_t.iconModel (+0x0c), and stores the returned handles into the two
     * consecutive itemInfo_t handle slots modelHandle (+0x04) and iconModelHandle
     * (+0x08). worldModel uses category 7, iconModel uses category 6. A slot whose
     * name pointer is NULL or an empty string ("" -> first byte 0) is skipped and
     * its handle slot is left untouched. */
    const char *const modelNames[2] = { item->worldModel, item->iconModel };
    const int modelCategories[2] = {
        CG_ITEM_MODEL_CATEGORY_WORLD,
        CG_ITEM_MODEL_CATEGORY_ICON
    };
    int32_t *modelHandleSlot = &itemInfo->modelHandle; /* &+0x04, then &+0x08 */
    for (int i = 0; i < 2; ++i) {
        const char *name = modelNames[i];
        /* 0x30044b04..b0d: TEST name; CMP byte[name],0 — non-NULL and non-empty. */
        if (name != 0 && name[0] != '\0')
            *modelHandleSlot = CG_RegisterModel(name, modelCategories[i]);
        modelHandleSlot++;
    }

    /* 0x30044b38..b43: register the HUD icon shader (gitem_t.hudIcon, +0x10) and
     * cache its handle in itemInfo->iconShader (+0x0c). This is the generic
     * material/shader register (0x3003db80), reused here for an item icon; the
     * declaration's "CG_RegisterMaterial" name is a first-caller misnomer.
     * No NULL check on hudIcon (the machine code registers unconditionally). */
    itemInfo->iconShader = CG_RegisterMaterial(item->hudIcon,
                                                    CG_ITEM_ICON_SHADER_PARAM);

    /* 0x30044b46..b5f: if the pickup-sound name (gitem_t.pickupSound, +0x04) is
     * non-NULL, register it via cgame_syscall(0xc3, name) and cache the returned
     * sound value in itemInfo->pickupSound (+0x1c). The trap returns an engine
     * sound value stored in the const char *-typed slot (as modeled by itemInfo_t;
     * CG_EntityEvent later consumes +0x1c/+0x20). */
    if (item->pickupSound != 0)
        itemInfo->pickupSound = trap_Com_SoundAliasString(item->pickupSound);

    /* 0x30044b66..b69: unconditionally mirror the pickup sound into +0x20. Note the
     * copy reads itemInfo->pickupSound whether or not the branch above ran (the
     * machine code performs this copy before the item-type test below). */
    itemInfo->pickupSoundAlt = itemInfo->pickupSound;

    /* 0x30044b62/b6c..b77: for a weapon item (gitem_t.type == IT_WEAPON), register
     * the underlying weapon by its index (gitem_t.weapon, +0x24). CG_RegisterWeapon
     * (0x300435d0) is the weapon-visuals registrar; it calls back into this function
     * for the weapon's own item. */
    if (item->type == IT_WEAPON)
        CG_RegisterWeapon(item->weapon);

    /* 0x30044b7c: mark this item's visuals as registered. */
    itemInfo->registered = 1;
}
