#include "bg_weapon.h"

#include "bg_weapon_binding.h"
#include "qcommon/q_string.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BG_FIRST_ITEM_INDEX = 1,
    BG_ITEM_COUNT = 134
};

/*
 * Complete BG item-name lookup subsystem.  The two Windows module copies are
 * instruction-identical after accounting for relocated globals, calls, and
 * each module's fatal-error boundary:
 *
 *   uo_cgame_mp_x86.dll  0x30005ca0, 0x30005cd0
 *   uo_game_mp_x86.dll   0x20005a20, 0x20005a50
 *
 * Linux game retains the same bounds and lookup decisions at RVAs 0x0001fa6c
 * and 0x0001faca.  Its Q_stricmp calls include the null rejection and 99,999
 * byte comparison limit that the optimized Windows bodies expose inline.
 */
gitem_t *BG_FindItemForWeapon(int32_t weapon)
{
    if (weapon < 0 || weapon > bg_numWeapons) {
        BG_WEAPON_ERROR("\x15"
                        "BG_FindItemForWeapon: weapon out of range %i",
                        weapon);
    }

    return &bg_itemlist[weapon];
}

gitem_t *BG_FindItem(const char *pickupName)
{
    weaponInfo_t **const weaponInfos = bg_weaponInfos;

    for (int32_t itemIndex = BG_FIRST_ITEM_INDEX; itemIndex < BG_ITEM_COUNT; ++itemIndex) {
        gitem_t *const item = &bg_itemlist[itemIndex];

        if (itemIndex <= bg_numWeapons) {
            const weaponInfo_t *const weaponInfo = weaponInfos[itemIndex];

            if (Q_stricmp(weaponInfo->pickupName, pickupName) == 0) {
                return item;
            }
            continue;
        }

        if (Q_stricmp(item->pickupName, pickupName) == 0 || Q_stricmp(item->classname, pickupName) == 0) {
            return item;
        }
    }

    return NULL;
}
