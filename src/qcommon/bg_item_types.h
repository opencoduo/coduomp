#ifndef QCOMMON_BG_ITEM_TYPES_H
#define QCOMMON_BG_ITEM_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Shared BG item discriminator used by the client and game-module item
 * tables. BG_CanItemBeGrabbed in both binaries dispatches on these values,
 * including the IT_BAD diagnostic for zero. */
typedef enum {
    IT_BAD = 0,
    IT_WEAPON = 1,
    IT_AMMO = 2,
    IT_HEALTH = 3
} itemType_t;

/* Shared item-definition row. Windows cgame indexes bg_itemlist at
 * 0x300827a0 with a 0x30-byte stride; the Windows and Linux game modules use
 * the same fields and stride while constructing and consuming their table.
 * These are native source pointers, so maintained 64-bit builds deliberately
 * use native sizeof(gitem_t). */
typedef struct gitem_s {
    const char *classname;
    const char *pickupSound;
    const char *worldModel;
    const char *iconModel;
    const char *hudIcon;
    const char *ammoIcon;
    const char *pickupName;
    int32_t quantity;
    itemType_t type;
    int32_t weapon;
    int32_t ammoIndex;
    int32_t clipIndex;
} gitem_t;

#if UINTPTR_MAX == UINT32_MAX
#define BG_ITEM_LAYOUT_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
BG_ITEM_LAYOUT_ASSERT(q_bg_item_classname_offset, offsetof(gitem_t, classname) == 0x00);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_pickup_sound_offset, offsetof(gitem_t, pickupSound) == 0x04);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_world_model_offset, offsetof(gitem_t, worldModel) == 0x08);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_icon_model_offset, offsetof(gitem_t, iconModel) == 0x0c);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_hud_icon_offset, offsetof(gitem_t, hudIcon) == 0x10);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_ammo_icon_offset, offsetof(gitem_t, ammoIcon) == 0x14);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_pickup_name_offset, offsetof(gitem_t, pickupName) == 0x18);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_quantity_offset, offsetof(gitem_t, quantity) == 0x1c);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_type_offset, offsetof(gitem_t, type) == 0x20);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_weapon_offset, offsetof(gitem_t, weapon) == 0x24);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_ammo_index_offset, offsetof(gitem_t, ammoIndex) == 0x28);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_clip_index_offset, offsetof(gitem_t, clipIndex) == 0x2c);
BG_ITEM_LAYOUT_ASSERT(q_bg_item_extent, sizeof(gitem_t) == 0x30);
#undef BG_ITEM_LAYOUT_ASSERT
#endif

#endif
