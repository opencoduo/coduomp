#include "bg_weapon.h"

#include "bg_weapon_binding.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>
#endif

/*
 * Complete weapon-definition descriptor table.  The Windows cgame table at
 * 0x300843a8 and Windows game table at 0x200853a8 contain the same 293 names,
 * member offsets, and parse discriminants.  Linux game uses the same table,
 * and the Mac cgame/game code banks retain the common parser names.
 */
#define BG_WEAPON_FIELD(index_, key_, member_, i386_offset_, type_) \
    { (key_), (int32_t)offsetof(weaponInfo_t, member_), (type_) }
const parseField_t bg_weaponFieldDefs[BG_WEAPON_FIELD_COUNT] = {
#include "bg_weapon_field_defs.inc"
};
#undef BG_WEAPON_FIELD

#if UINTPTR_MAX == UINT32_MAX
/* The retained i386 offsets are direct table evidence, not host-layout
 * assumptions. */
enum {
#define BG_WEAPON_FIELD(index_, key_, member_, i386_offset_, type_) \
    bg_weapon_field_offset_check_##index_ = \
        1 / ((offsetof(weaponInfo_t, member_) == (i386_offset_)) ? 1 : 0)
#include "bg_weapon_field_defs.inc"
#undef BG_WEAPON_FIELD
};
#endif

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the seven identical
 * CRT string-comparison loops in the original switch cases. */
static int32_t bg_compat_find_weapon_enum_value(
    const char *value, const char *const *names, int32_t count)
{
    int32_t index;

    for (index = 0; index < count; ++index) {
#if defined(_WIN32)
        if (_stricmp(value, names[index]) == 0) {
#else
        if (strcasecmp(value, names[index]) == 0) {
#endif
            return index;
        }
    }
    return count;
}

/*
 * The seven scans, stores, diagnostics, and return paths agree in the
 * authoritative Windows modules (cgame 0x3000fad0, game 0x2000f860) and the
 * Linux game body at RVA 0x00030263.  Mac traceback symbols independently
 * retain BG_ParseWeaponInfoSpecificFieldType in both modules.
 */
qboolean BG_ParseWeaponInfoSpecificFieldType(void *weaponInfoBase,
                                              const char *value,
                                              int32_t fieldType)
{
    weaponInfo_t *weaponInfo = weaponInfoBase;
    int32_t parsed;

    switch (fieldType) {
    case WEAPON_FIELD_PARSE_WEAPON_TYPE:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponTypeNames, WEAPTYPE_COUNT);
        if (parsed == WEAPTYPE_COUNT) {
            BG_WEAPON_ERROR("\x15" "Unknown weapon type \"%s\" in \"%s\"\n",
                            value, weaponInfo->pickupName);
        } else {
            weaponInfo->weaponType = (weaponType_t)parsed;
        }
        return qtrue;

    case WEAPON_FIELD_PARSE_WEAPON_CLASS:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponClassNames, WEAPCLASS_COUNT);
        if (parsed == WEAPCLASS_COUNT) {
            BG_WEAPON_ERROR("\x15" "Unknown weapon class \"%s\" in \"%s\"\n",
                            value, weaponInfo->pickupName);
        } else {
            weaponInfo->weaponClass = (weaponClass_t)parsed;
        }
        return qtrue;

    case WEAPON_FIELD_PARSE_AMMO_TYPE:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponAmmoTypeNames, WEAPON_AMMO_TYPE_COUNT);
        if (parsed == WEAPON_AMMO_TYPE_COUNT) {
            BG_WEAPON_ERROR("\x15" "Unknown ammo type \"%s\" in \"%s\"\n",
                            value, weaponInfo->pickupName);
        } else {
            weaponInfo->ammoType = parsed;
        }
        return qtrue;

    case WEAPON_FIELD_PARSE_OVERLAY_RETICLE:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponOverlayReticleNames,
            WEAPON_OVERLAY_RETICLE_COUNT);
        if (parsed == WEAPON_OVERLAY_RETICLE_COUNT) {
            BG_WEAPON_ERROR(
                "\x15" "Unknown weapon overlay reticle \"%s\" in \"%s\"\n",
                value, weaponInfo->pickupName);
        } else {
            weaponInfo->adsOverlayReticle = parsed;
        }
        return qtrue;

    case WEAPON_FIELD_PARSE_WEAPON_SLOT:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponSlotNames, WEAPSLOT_COUNT);
        if (parsed == WEAPSLOT_COUNT) {
            BG_WEAPON_ERROR("\x15" "Unknown weapon slot \"%s\" in \"%s\"\n",
                            value, weaponInfo->pickupName);
        } else {
            weaponInfo->slot = parsed;
        }
        return qtrue;

    case WEAPON_FIELD_PARSE_WEAPON_STANCE:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponStanceNames, WEAPON_STANCE_COUNT);
        if (parsed == WEAPON_STANCE_COUNT) {
            BG_WEAPON_ERROR("\x15" "Unknown weapon stance \"%s\" in \"%s\"\n",
                            value, weaponInfo->pickupName);
        } else {
            weaponInfo->stance = parsed;
        }
        return qtrue;

    case WEAPON_FIELD_PARSE_PROJECTILE_EXPLOSION:
        parsed = bg_compat_find_weapon_enum_value(
            value, bg_weaponGrenadeTypeNames, WEAPON_GRENADE_TYPE_COUNT);
        if (parsed == WEAPON_GRENADE_TYPE_COUNT) {
            BG_WEAPON_ERROR(
                "\x15" "Unknown projectile explosion \"%s\" in \"%s\"\n",
                value, weaponInfo->pickupName);
        } else {
            weaponInfo->projectileExplosionType = parsed;
        }
        return qtrue;

    default:
        BG_WEAPON_ERROR("\x15" "Bad field type %i in %s\n",
                        fieldType, weaponInfo->pickupName);
        return qfalse;
    }
}
