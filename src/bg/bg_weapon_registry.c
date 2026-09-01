#include "bg_weapon.h"

#include "bg_weapon_binding.h"
#include "compat/coduo_int32_bits.h"
#include "qcommon/q_string.h"

#include <stdint.h>

/*
 * The Windows cgame/game bodies have identical table accesses, lookup bounds,
 * folded comparisons, return domains, and diagnostics.  The Mac cgame/game
 * symbol banks independently preserve the same names and equal function sizes;
 * the Linux game bodies implement the same contracts.
 *
 * Windows cgame RVAs: 0x00010f70..0x00011142
 * Linux game RVAs:    0x00031a19..0x00031d30
 */

const weaponInfo_t *BG_GetInfoForWeapon(int32_t weapon)
{
    return bg_weaponInfos[weapon];
}

/*
 * These three accessors are instruction-identical between the authoritative
 * Windows cgame and game modules apart from the weapon-table relocation:
 *
 *   uo_cgame_mp_x86.dll  0x30012230, 0x30012240, 0x30012250
 *   uo_game_mp_x86.dll   0x20012170, 0x20012180, 0x20012190
 *
 * The Linux game module performs the same lookup and field loads at RVAs
 * 0x00033bad, 0x00033bd6, and 0x00033bff.  The Mac cgame/game symbol banks
 * independently preserve the canonical names.
 */
int32_t BG_ClipForWeapon(int32_t weapon)
{
    return BG_GetInfoForWeapon(weapon)->clipIndex;
}

int32_t BG_AmmoForWeapon(int32_t weapon)
{
    return BG_GetInfoForWeapon(weapon)->ammoIndex;
}

int32_t BG_WeaponIsClipOnly(int32_t weapon)
{
    return BG_GetInfoForWeapon(weapon)->clipRequired;
}

int32_t BG_GetWeaponForInfo(const weaponInfo_t *weaponInfo)
{
    return weaponInfo->weaponIndex;
}

int32_t BG_GetNumWeapons(void)
{
    return bg_numWeapons;
}

int32_t BG_GetNumAmmoTypes(void)
{
    return bg_numAmmoTypes;
}

int32_t BG_GetAmmoTypeMax(int32_t ammoIndex)
{
    return bg_ammoTypeMax[ammoIndex];
}

int32_t BG_GetNumAmmoClips(void)
{
    return bg_numAmmoClips;
}

int32_t BG_GetAmmoClipSize(int32_t clipIndex)
{
    return bg_ammoClipSizes[clipIndex];
}

int32_t BG_GetSharedAmmoCapSize(int32_t sharedAmmoCapIndex)
{
    return bg_sharedAmmoCapSizes[sharedAmmoCapIndex];
}

const char *BG_GetAmmoTypeName(int32_t ammoIndex)
{
    return bg_ammoTypeNames[ammoIndex];
}

const char *BG_GetAmmoClipName(int32_t clipIndex)
{
    return bg_ammoClipNames[clipIndex];
}

int32_t BG_GetAmmoTypeForName(const char *name)
{
    int32_t ammoIndex;

    for (ammoIndex = 0; ammoIndex < bg_numAmmoTypes;
         ammoIndex = coduo_int32_from_bits((uint32_t)ammoIndex + 1u)) {
        if (Q_stricmp(bg_ammoTypeNames[ammoIndex], name) == 0) {
            return ammoIndex;
        }
    }

    BG_WEAPON_DEBUG("Couldn't find ammo type \"%s\"\n", name);
    return 0;
}

int32_t BG_GetAmmoClipForName(const char *name)
{
    int32_t clipIndex;

    for (clipIndex = 0; clipIndex < bg_numAmmoClips;
         clipIndex = coduo_int32_from_bits((uint32_t)clipIndex + 1u)) {
        if (Q_stricmp(bg_ammoClipNames[clipIndex], name) == 0) {
            return clipIndex;
        }
    }

    BG_WEAPON_DEBUG("Couldn't find ammo clip \"%s\"\n", name);
    return 0;
}

int32_t BG_GetWeaponSlotForName(const char *name)
{
    int32_t slot;

    for (slot = 0; slot < WEAPSLOT_COUNT;
         slot = coduo_int32_from_bits((uint32_t)slot + 1u)) {
        if (Q_stricmp(name, bg_weaponSlotNames[slot]) == 0) {
            return slot;
        }
    }

    return WEAPSLOT_NONE;
}

const char *BG_GetWeaponSlotNameForIndex(int32_t slot)
{
    return bg_weaponSlotNames[slot];
}

int32_t BG_GetWeaponIndexForName(const char *name)
{
    int32_t weapon;

    for (weapon = 0; weapon <= bg_numWeapons;
         weapon = coduo_int32_from_bits((uint32_t)weapon + 1u)) {
        const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];

        if (Q_stricmp(name, weaponInfo->pickupName) == 0) {
            /* All retained bodies expose only the low byte of the matched
             * registry index; the table's valid domain is 0..127. */
            return weapon & 0xff;
        }
    }

    BG_WEAPON_DEBUG("Couldn't find weapon \"%s\"\n", name);
    return 0;
}

qboolean BG_IsAimDownSightWeapon(int32_t weapon)
{
    /* The original leaf returns the complete adsEnabled dword unchanged:
     * uo_cgame_mp_x86.dll 0x30011150, uo_game_mp_x86.dll 0x20011070,
     * and Linux game RVA 0x00031d19. */
    return BG_GetInfoForWeapon(weapon)->adsEnabled;
}
