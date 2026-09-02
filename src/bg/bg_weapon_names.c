#include "bg_weapon.h"

/*
 * Complete shared weapon-enum name-table cluster.  The authoritative Windows
 * cgame and game modules contain the same seven consecutive pointer arrays at
 * image-relative 0x842e8..0x843a4.  Every pointer resolves to the same string,
 * and each parser selects the same independent base and enum-defined count.
 * Linux game retains the same strings, table domains, and lookup results.
 * Keeping the original order also preserves the physical adjacency observed
 * by the unchecked BG_GetWeaponTypeName table lookup.
 */
const char *const bg_weaponTypeNames[WEAPTYPE_COUNT] = {"bullet", "grenade", "projectile", "spotter", "gas"};

const char *const bg_weaponOverlayReticleNames[WEAPON_OVERLAY_RETICLE_COUNT] = {"none", "crosshair", "FG42", "Springfield", "Gewehr43"};

const char *const bg_weaponSlotNames[WEAPSLOT_COUNT] = {"none",    "primary",      "primaryb", "pistol",
                                                        "grenade", "smokegrenade", "satchel",  "binocular"};

const char *const bg_weaponStanceNames[WEAPON_STANCE_COUNT] = {"stand", "duck", "prone"};

const char *const bg_weaponClassNames[WEAPCLASS_COUNT] = {"rifle",          "mg",     "smg",     "lmg",        "pistol",      "grenade",
                                                          "rocketlauncher", "turret", "spotter", "non-player", "flamethrower"};

const char *const bg_weaponAmmoTypeNames[WEAPON_AMMO_TYPE_COUNT] = {"smg", "pistol", "rifle", "lmg", "hmg", "umg"};

const char *const bg_weaponGrenadeTypeNames[WEAPON_GRENADE_TYPE_COUNT] = {"grenade", "smoke", "rocket", "molotov", "artillery",
                                                                          "mortar",  "tank",  "b17",    "none"};

/* Windows cgame 0x3000fa10 and Windows game 0x2000f7c0 are identical after
 * relocating the common table base. Linux game retains the same single indexed
 * pointer load at RVA 0x30100. The original performs no range check. */
const char *BG_GetWeaponTypeName(int32_t weaponType)
{
    return bg_weaponTypeNames[weaponType];
}
