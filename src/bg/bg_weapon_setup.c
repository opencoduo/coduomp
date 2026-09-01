#include "bg_weapon.h"

#include "bg_weapon_binding.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "qcommon/q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    BG_FIRST_WEAPON_ITEM = 1,
    BG_ITEM_ROW_COUNT = 134,
    BG_SHARED_AMMO_CAP_NONE = -1
};

#define BG_DEFAULT_ADS_IN_RATE 0.0033333334f  /* binary32 0x3b5a740e */
#define BG_DEFAULT_ADS_OUT_RATE 0.0020000001f /* binary32 0x3b03126f */

/*
 * These registration passes are common to the Windows cgame and game
 * modules and the Linux game module.  Within the Windows target, the original
 * cgame/game bodies have the same operation graphs and field accesses after
 * relocating their globals and calls.  The Linux game bodies perform the same
 * table transformations.  The sole role difference is the game module's
 * developer-only shared-ammo diagnostic, retained by the local binding header.
 *
 *   Windows cgame: 0x300103e0, 0x30010550, 0x300107e0,
 *                  0x300109a0, 0x30010c30
 *   Windows game:  0x20010190, 0x20010300, 0x20010590,
 *                  0x20010750, 0x200109e0
 *   Linux game:    RVA 0x00030b85, 0x00030dc4, 0x00030f9b,
 *                  0x000311b7, 0x0003138e
 */

/*
 * The ADS-rate setup bodies are likewise identical between the two Windows
 * modules apart from globals and constant relocations:
 *
 *   uo_cgame_mp_x86.dll 0x300101d0
 *   uo_game_mp_x86.dll  0x2000ff80
 *   game.mp.uo.i386.so  RVA 0x00030ab4
 *
 * All three walk weapon indices 1..bg_numWeapons, use a signed integer load
 * for each positive transition time, divide 1.0 by that exact integer, and
 * store the result as binary32.  The Windows loop is merely four-way unrolled.
 * The two fallback dwords are the exact stored reciprocals of 300 and 500 ms.
 */
void BG_SetupWeaponADSRates(void)
{
    int32_t weapon;

    for (weapon = 1; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];

        if (weaponInfo->adsInTime > 0) {
#if EMULATE_X87
            weaponInfo->adsFireDelayRate = x87f_store_f32(x87f_div(
                x87f_load_f32(1.0f),
                x87f_load_i32(weaponInfo->adsInTime)));
#else
            weaponInfo->adsFireDelayRate = (float)(
                1.0L / (long double)weaponInfo->adsInTime);
#endif
        } else {
            weaponInfo->adsFireDelayRate = BG_DEFAULT_ADS_IN_RATE;
        }

        if (weaponInfo->adsOutTime > 0) {
#if EMULATE_X87
            weaponInfo->adsFireDelayOutRate = x87f_store_f32(x87f_div(
                x87f_load_f32(1.0f),
                x87f_load_i32(weaponInfo->adsOutTime)));
#else
            weaponInfo->adsFireDelayOutRate = (float)(
                1.0L / (long double)weaponInfo->adsOutTime);
#endif
        } else {
            weaponInfo->adsFireDelayOutRate = BG_DEFAULT_ADS_OUT_RATE;
        }
    }
}

void BG_FillInWeaponItems(void)
{
    int32_t itemIndex;

    /* Weapon-list loaders validate bg_numWeapons against the shared
     * MAX_WEAPON_FILES domain before publishing it to this loop. */
    for (itemIndex = BG_FIRST_WEAPON_ITEM;
         itemIndex <= bg_numWeapons;
         itemIndex = coduo_int32_from_bits((uint32_t)itemIndex + 1u)) {
        gitem_t *item = &bg_itemlist[itemIndex];
        const weaponInfo_t *weaponInfo = bg_weaponInfos[itemIndex];
        const char *modelName = weaponInfo->pickupModel;

        if (modelName == NULL || modelName[0] == '\0') {
            modelName = weaponInfo->worldModel;
        }

        item->classname = weaponInfo->scriptClassname;
        item->pickupSound = weaponInfo->pickupSound;
        item->worldModel = modelName;
        item->iconModel = NULL;
        item->hudIcon = weaponInfo->hudIcon;
        item->ammoIcon = weaponInfo->ammoIcon;
        item->pickupName = weaponInfo->displayName;
        item->quantity = weaponInfo->startAmmo;
        item->type = IT_WEAPON;
        item->weapon = itemIndex;
        item->ammoIndex = weaponInfo->ammoIndex;
        item->clipIndex = weaponInfo->clipIndex;
    }

    for (; itemIndex < BG_ITEM_ROW_COUNT;
         itemIndex = coduo_int32_from_bits((uint32_t)itemIndex + 1u)) {
        gitem_t *item = &bg_itemlist[itemIndex];

        if (item->type == IT_AMMO) {
            int32_t weapon;

            for (weapon = BG_FIRST_WEAPON_ITEM;
                 weapon <= bg_numWeapons;
                 weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
                const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
                int32_t nameLength = (int32_t)strlen(weaponInfo->pickupName);

                if (Q_stricmpn(item->pickupName, weaponInfo->pickupName,
                               nameLength) == 0) {
                    item->weapon = weapon;
                    item->ammoIndex = weaponInfo->ammoIndex;
                    item->clipIndex = weaponInfo->clipIndex;
                    break;
                }
            }

            if (item->weapon == -1) {
                const weaponInfo_t *defaultWeaponInfo =
                    bg_weaponInfos[BG_FIRST_WEAPON_ITEM];

                BG_WEAPON_PRINT(
                    "^3WARNING^7: Could not find weapon for ammo item %s\n",
                    item->pickupName);
                item->weapon = BG_FIRST_WEAPON_ITEM;
                item->ammoIndex = defaultWeaponInfo->ammoIndex;
                item->clipIndex = defaultWeaponInfo->clipIndex;
            }
        }
    }
}

void BG_SetupAmmoIndexes(void)
{
    int32_t weapon;

    for (weapon = 1; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
        int32_t ammoIndex;

        Q_strlwr((char *)weaponInfo->ammoName);
        for (ammoIndex = 0; ammoIndex < bg_numAmmoTypes;
             ammoIndex = coduo_int32_from_bits((uint32_t)ammoIndex + 1u)) {
            if (Q_stricmp(bg_ammoTypeNames[ammoIndex],
                          weaponInfo->ammoName) == 0) {
                int32_t previous;

                weaponInfo->ammoIndex = ammoIndex;
                if (bg_ammoTypeMax[ammoIndex] == weaponInfo->maxAmmo ||
                    ammoIndex == 0) {
                    break;
                }

                for (previous = 1; previous < weapon;
                     previous = coduo_int32_from_bits(
                         (uint32_t)previous + 1u)) {
                    weaponInfo_t *previousInfo = bg_weaponInfos[previous];

                    if (Q_stricmp(bg_ammoTypeNames[ammoIndex],
                                  previousInfo->ammoName) == 0 &&
                        previousInfo->maxAmmo == bg_ammoTypeMax[ammoIndex]) {
                        BG_WEAPON_ERROR(
                            "\x15" "Max ammo mismatch for \"%s\" ammo: "
                            "'%s\" set it to %i, but \"%s\" already set it "
                            "to %i.\n",
                            weaponInfo->ammoName, weaponInfo->pickupName,
                            weaponInfo->maxAmmo, previousInfo->pickupName,
                            previousInfo->maxAmmo);
                    }
                }
                break;
            }
        }

        if (ammoIndex == bg_numAmmoTypes) {
            bg_ammoTypeNames[ammoIndex] = weaponInfo->ammoName;
            bg_ammoTypeMax[ammoIndex] = weaponInfo->maxAmmo;
            weaponInfo->ammoIndex = ammoIndex;
            bg_numAmmoTypes = coduo_int32_from_bits(
                (uint32_t)bg_numAmmoTypes + 1u);
        }
    }
}

void BG_SetupSharedAmmoIndexes(void)
{
    int32_t weapon;

    for (weapon = 1; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
        int32_t sharedAmmoCapIndex;

        weaponInfo->sharedAmmoCapIndex = BG_SHARED_AMMO_CAP_NONE;
        if (weaponInfo->sharedAmmoCapName[0] == '\0') {
            continue;
        }

        BG_WEAPON_SHARED_AMMO_DEBUG("%s: %s\n",
                                    weaponInfo->pickupName,
                                    weaponInfo->sharedAmmoCapName);
        Q_strlwr((char *)weaponInfo->sharedAmmoCapName);

        for (sharedAmmoCapIndex = 0;
             sharedAmmoCapIndex < bg_numSharedAmmoCaps;
             sharedAmmoCapIndex = coduo_int32_from_bits(
                 (uint32_t)sharedAmmoCapIndex + 1u)) {
            if (Q_stricmp(bg_sharedAmmoCapNames[sharedAmmoCapIndex],
                          weaponInfo->sharedAmmoCapName) == 0) {
                int32_t previous;

                weaponInfo->sharedAmmoCapIndex = sharedAmmoCapIndex;
                if (bg_sharedAmmoCapSizes[sharedAmmoCapIndex] ==
                        weaponInfo->sharedAmmoCap ||
                    sharedAmmoCapIndex == 0) {
                    break;
                }

                for (previous = 1; previous < weapon;
                     previous = coduo_int32_from_bits(
                         (uint32_t)previous + 1u)) {
                    weaponInfo_t *previousInfo = bg_weaponInfos[previous];

                    if (Q_stricmp(bg_sharedAmmoCapNames[sharedAmmoCapIndex],
                                  previousInfo->sharedAmmoCapName) == 0 &&
                        previousInfo->sharedAmmoCap ==
                            bg_sharedAmmoCapSizes[sharedAmmoCapIndex]) {
                        BG_WEAPON_ERROR(
                            "\x15" "Shared ammo cap mismatch for \"%s\" shared "
                            "ammo cap: '%s\" set it to %i, but \"%s\" already "
                            "set it to %i.\n",
                            weaponInfo->sharedAmmoCapName,
                            weaponInfo->pickupName,
                            weaponInfo->sharedAmmoCap,
                            previousInfo->pickupName,
                            previousInfo->sharedAmmoCap);
                    }
                }
                break;
            }
        }

        if (sharedAmmoCapIndex == bg_numSharedAmmoCaps) {
            bg_sharedAmmoCapNames[sharedAmmoCapIndex] =
                weaponInfo->sharedAmmoCapName;
            bg_sharedAmmoCapSizes[sharedAmmoCapIndex] =
                weaponInfo->sharedAmmoCap;
            weaponInfo->sharedAmmoCapIndex = sharedAmmoCapIndex;
            bg_numSharedAmmoCaps = coduo_int32_from_bits(
                (uint32_t)bg_numSharedAmmoCaps + 1u);
        }
    }
}

void BG_SetupClipIndexes(void)
{
    int32_t weapon;

    for (weapon = 1; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
        int32_t clipIndex;

        Q_strlwr((char *)weaponInfo->clipName);
        for (clipIndex = 0; clipIndex < bg_numAmmoClips;
             clipIndex = coduo_int32_from_bits((uint32_t)clipIndex + 1u)) {
            if (Q_stricmp(bg_ammoClipNames[clipIndex],
                          weaponInfo->clipName) == 0) {
                int32_t previous;

                weaponInfo->clipIndex = clipIndex;
                if (bg_ammoClipSizes[clipIndex] == weaponInfo->clipSize ||
                    clipIndex == 0) {
                    break;
                }

                for (previous = 1; previous < weapon;
                     previous = coduo_int32_from_bits(
                         (uint32_t)previous + 1u)) {
                    weaponInfo_t *previousInfo = bg_weaponInfos[previous];

                    if (Q_stricmp(bg_ammoClipNames[clipIndex],
                                  previousInfo->clipName) == 0 &&
                        previousInfo->clipSize == bg_ammoClipSizes[clipIndex]) {
                        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                        BG_WEAPON_ERROR(
                            "\x15" "Clip Size mismatch for \"%s\" clip: "
                            "'%s\" set it to %i, but \"%s\" already set it "
                            "to %i.\n",
                            weaponInfo->ammoName, weaponInfo->pickupName,
                            weaponInfo->clipSize, previousInfo->pickupName,
                            previousInfo->clipSize);
                    }
                }
                break;
            }
        }

        if (clipIndex == bg_numAmmoClips) {
            bg_ammoClipNames[clipIndex] = weaponInfo->clipName;
            bg_ammoClipSizes[clipIndex] = weaponInfo->clipSize;
            weaponInfo->clipIndex = clipIndex;
            bg_numAmmoClips = coduo_int32_from_bits(
                (uint32_t)bg_numAmmoClips + 1u);
        }
    }
}

void BG_SetupAltWeaponIndexes(void)
{
    int32_t weapon;

    for (weapon = 1; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        bg_weaponInfos[weapon]->altWeapon = 0;
    }

    for (weapon = 1; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
        weaponInfo_t *current;

        if (weaponInfo->altWeapon != 0 ||
            weaponInfo->altWeaponName[0] == '\0') {
            continue;
        }

        current = weaponInfo;
        while (current->altWeapon == 0) {
            int32_t altWeapon;

            for (altWeapon = 1; altWeapon <= bg_numWeapons;
                 altWeapon = coduo_int32_from_bits(
                     (uint32_t)altWeapon + 1u)) {
                weaponInfo_t *altInfo = bg_weaponInfos[altWeapon];

                if (Q_stricmp(current->altWeaponName,
                              altInfo->pickupName) != 0) {
                    continue;
                }

                current->altWeapon = altWeapon;
                if (current->slot != altInfo->slot) {
                    BG_WEAPON_ERROR(
                        "\x15" "weapon '%s' does not have same weaponSlot "
                        "setting as its alt weapon '%s'",
                        current->pickupName, altInfo->pickupName);
                }
                if (current->stackable != altInfo->stackable) {
                    BG_WEAPON_ERROR(
                        "\x15" "weapon '%s' does not have same slotStackable "
                        "setting as its alt weapon '%s'",
                        current->pickupName, altInfo->pickupName);
                }
                break;
            }

            if (current->altWeapon == 0) {
                BG_WEAPON_ERROR(
                    "\x15" "could not find altWeapon '%s' for weapon '%s'",
                    current->altWeaponName, current->pickupName);
            }

            /* If the fatal-error boundary returns, all retained binaries use
             * the exhausted count+1 cursor here. */
            current = bg_weaponInfos[altWeapon];
        }

        if (current != weaponInfo) {
            BG_WEAPON_ERROR(
                "\x15" "weapon '%s' has a bad altWeapon '%s'",
                weaponInfo->pickupName, weaponInfo->altWeaponName);
        }
    }
}
