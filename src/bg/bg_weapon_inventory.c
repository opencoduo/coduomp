#include "bg_weapon.h"

#include "bg_weapon_binding.h"
#include "compat/coduo_int32_bits.h"

#include <stdint.h>

enum {
    BG_FIRST_PICKUP_ITEM = 1,
    BG_PICKUP_ITEM_COUNT = 134
};

/*
 * The Mac cgame/game symbol banks preserve equal sizes for the shared slot and
 * ammo-capacity bodies.  Windows cgame/game and Linux game inspection agrees
 * on their field offsets, branch rules, loop bounds, and return values.  Game
 * callers formerly passed gclient_t because playerState_t is its first member;
 * the common source contract names the actual record consumed here.
 */

int32_t BG_GetMaxPickupableAmmo(const playerState_t *ps, int32_t weapon)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
    int32_t ammoIndex = weaponInfo->ammoIndex;
    int32_t clipIndex = weaponInfo->clipIndex;
    int32_t seenAmmo[MAX_AMMO_TYPES] = {0};
    int32_t seenClip[MAX_AMMO_TYPES] = {0};
    int32_t sharedCapIndex = weaponInfo->sharedAmmoCapIndex;
    int32_t remaining;
    int32_t candidate;

    if (sharedCapIndex < 0) {
        if (weaponInfo->clipRequired != 0) {
            return coduo_int32_from_bits((uint32_t)bg_ammoClipSizes[clipIndex] - (uint32_t)ps->clips[clipIndex]);
        }
        return coduo_int32_from_bits((uint32_t)bg_ammoTypeMax[ammoIndex] - (uint32_t)ps->ammo[ammoIndex]);
    }

    remaining = bg_sharedAmmoCapSizes[sharedCapIndex];
    for (candidate = 1; candidate <= bg_numWeapons; candidate = coduo_int32_from_bits((uint32_t)candidate + 1u)) {
        const weaponInfo_t *candidateInfo;
        uint32_t bit = 1u << ((uint32_t)candidate & 31u);
        int32_t word = coduo_int32_sar((uint32_t)candidate, 5);

        if ((ps->weaponBits[word] & bit) == 0) {
            continue;
        }
        candidateInfo = bg_weaponInfos[candidate];
        if (candidateInfo->sharedAmmoCapIndex != sharedCapIndex) {
            continue;
        }

        if (candidateInfo->clipRequired != 0) {
            int32_t index = candidateInfo->clipIndex;
            if (seenClip[index] == 0) {
                remaining = coduo_int32_from_bits((uint32_t)remaining - (uint32_t)ps->clips[index]);
                seenClip[index] = 1;
            }
        } else {
            int32_t index = candidateInfo->ammoIndex;
            if (seenAmmo[index] == 0) {
                remaining = coduo_int32_from_bits((uint32_t)remaining - (uint32_t)ps->ammo[index]);
                seenAmmo[index] = 1;
            }
        }
    }
    return remaining;
}

int32_t BG_GetTotalAmmoReserve(const playerState_t *ps, int32_t weapon)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
    int32_t seenAmmo[MAX_AMMO_TYPES] = {0};
    int32_t seenClip[MAX_AMMO_TYPES] = {0};
    int32_t sharedCapIndex = weaponInfo->sharedAmmoCapIndex;
    int32_t total = 0;
    int32_t candidate;

    if (sharedCapIndex < 0) {
        return weaponInfo->clipRequired != 0 ? ps->clips[weaponInfo->clipIndex] : ps->ammo[weaponInfo->ammoIndex];
    }

    for (candidate = 1; candidate <= bg_numWeapons; candidate = coduo_int32_from_bits((uint32_t)candidate + 1u)) {
        const weaponInfo_t *candidateInfo;
        uint32_t bit = 1u << ((uint32_t)candidate & 31u);
        int32_t word = coduo_int32_sar((uint32_t)candidate, 5);

        if ((ps->weaponBits[word] & bit) == 0) {
            continue;
        }
        candidateInfo = bg_weaponInfos[candidate];
        if (candidateInfo->sharedAmmoCapIndex != sharedCapIndex) {
            continue;
        }

        if (candidateInfo->clipRequired != 0) {
            int32_t index = candidateInfo->clipIndex;
            if (seenClip[index] == 0) {
                total = coduo_int32_from_bits((uint32_t)total + (uint32_t)ps->clips[index]);
                seenClip[index] = 1;
            }
        } else {
            int32_t index = candidateInfo->ammoIndex;
            if (seenAmmo[index] == 0) {
                total = coduo_int32_from_bits((uint32_t)total + (uint32_t)ps->ammo[index]);
                seenAmmo[index] = 1;
            }
        }
    }
    return total;
}

int32_t BG_GetEmptySlotForWeapon(const playerState_t *ps, int32_t weapon)
{
    int32_t slot = bg_weaponInfos[weapon]->slot;

    if ((uint32_t)slot - 1u >= WEAPSLOT_LAST_DROPPABLE) {
        return WEAPSLOT_NONE;
    }
    if (slot < WEAPSLOT_PRIMARY_LIMIT) {
        if (ps->weaponSlots[WEAPSLOT_PRIMARY_FIRST] == 0) {
            return WEAPSLOT_PRIMARY_FIRST;
        }
        if (ps->weaponSlots[WEAPSLOT_PRIMARY_SECOND] == 0) {
            return WEAPSLOT_PRIMARY_SECOND;
        }
        return WEAPSLOT_NONE;
    }
    return ps->weaponSlots[slot] == 0 ? slot : WEAPSLOT_NONE;
}

int32_t BG_GetStackSlotForWeapon(const playerState_t *ps, int32_t weapon, int32_t preferredSlot)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
    int32_t slot = weaponInfo->slot;
    int32_t occupant;

    if (weaponInfo->stackable == 0 || (uint32_t)slot - 1u >= WEAPSLOT_LAST_DROPPABLE) {
        return WEAPSLOT_NONE;
    }

    if (slot < WEAPSLOT_PRIMARY_LIMIT) {
        if (preferredSlot == WEAPSLOT_PRIMARY_FIRST || preferredSlot == WEAPSLOT_PRIMARY_SECOND) {
            occupant = (int8_t)ps->weaponSlots[preferredSlot];
            if (occupant == 0 || bg_weaponInfos[occupant]->stackable != 0) {
                return preferredSlot;
            }
        }
        occupant = (int8_t)ps->weaponSlots[WEAPSLOT_PRIMARY_FIRST];
        if (occupant == 0 || bg_weaponInfos[occupant]->stackable != 0) {
            return WEAPSLOT_PRIMARY_FIRST;
        }
        occupant = (int8_t)ps->weaponSlots[WEAPSLOT_PRIMARY_SECOND];
        if (occupant == 0 || bg_weaponInfos[occupant]->stackable != 0) {
            return WEAPSLOT_PRIMARY_SECOND;
        }
        return WEAPSLOT_NONE;
    }

    occupant = (int8_t)ps->weaponSlots[slot];
    return occupant == 0 || bg_weaponInfos[occupant]->stackable != 0 ? slot : WEAPSLOT_NONE;
}

qboolean BG_SetPlayerWeaponForSlot(playerState_t *ps, int32_t slot, int32_t weapon)
{
    uint32_t bit = 1u << ((uint32_t)weapon & 31u);
    int32_t word = coduo_int32_sar((uint32_t)weapon, 5);
    int32_t weaponSlot;

    if ((ps->weaponBits[word] & bit) == 0) {
        return qfalse;
    }
    weaponSlot = bg_weaponInfos[weapon]->slot;
    if ((uint32_t)weaponSlot - 1u >= WEAPSLOT_LAST_DROPPABLE) {
        return qfalse;
    }
    if (weaponSlot < WEAPSLOT_PRIMARY_LIMIT) {
        if (slot != WEAPSLOT_PRIMARY_FIRST && slot != WEAPSLOT_PRIMARY_SECOND) {
            return qfalse;
        }
    } else if (slot != weaponSlot) {
        return qfalse;
    }
    ps->weaponSlots[slot] = (uint8_t)weapon;
    return qtrue;
}

int32_t BG_IsPlayerWeaponInSlot(const playerState_t *ps, int32_t weapon, qboolean includeAltWeapons)
{
    uint32_t bit = 1u << ((uint32_t)weapon & 31u);
    int32_t word = coduo_int32_sar((uint32_t)weapon, 5);
    int32_t current = weapon;

    if ((ps->weaponBits[word] & bit) == 0) {
        return WEAPSLOT_NONE;
    }
    do {
        const weaponInfo_t *weaponInfo = bg_weaponInfos[current];
        int32_t slot = weaponInfo->slot;

        if ((uint32_t)slot - 1u >= WEAPSLOT_LAST_DROPPABLE) {
            return WEAPSLOT_NONE;
        }
        if (slot < WEAPSLOT_PRIMARY_LIMIT) {
            if ((int8_t)ps->weaponSlots[WEAPSLOT_PRIMARY_FIRST] == current) {
                return WEAPSLOT_PRIMARY_FIRST;
            }
            if ((int8_t)ps->weaponSlots[WEAPSLOT_PRIMARY_SECOND] == current) {
                return WEAPSLOT_PRIMARY_SECOND;
            }
        } else if ((int8_t)ps->weaponSlots[slot] == current) {
            return slot;
        }

        if (includeAltWeapons == qfalse || weaponInfo->altWeapon == 0) {
            break;
        }
        current = weaponInfo->altWeapon;
    } while (current != weapon);
    return WEAPSLOT_NONE;
}

qboolean BG_IsPlayerWeaponAnAlt(int32_t weapon, int32_t altWeapon)
{
    int32_t current = bg_weaponInfos[weapon]->altWeapon;

    while (current != 0) {
        if (current == altWeapon) {
            return qtrue;
        }
        if (current == weapon) {
            return qfalse;
        }
        current = bg_weaponInfos[current]->altWeapon;
    }
    return qfalse;
}

qboolean BG_GivePlayerWeapon(playerState_t *ps, int32_t weapon)
{
    uint32_t bit = 1u << ((uint32_t)weapon & 31u);
    int32_t word = coduo_int32_sar((uint32_t)weapon, 5);
    const weaponInfo_t *weaponInfo;
    int32_t altWeapon;

    if ((ps->weaponBits[word] & bit) != 0) {
        return qfalse;
    }
    weaponInfo = bg_weaponInfos[weapon];
    if (weaponInfo->weaponClass == WEAPCLASS_TURRET || weaponInfo->weaponClass == WEAPCLASS_NON_PLAYER) {
        return qfalse;
    }
    ps->weaponBits[word] |= bit;
    ps->weaponRechamberBits[word] &= ~bit;

    switch (weaponInfo->slot) {
    case WEAPSLOT_PRIMARY_FIRST:
    case WEAPSLOT_PRIMARY_SECOND:
        if (ps->weaponSlots[WEAPSLOT_PRIMARY_FIRST] == 0) {
            ps->weaponSlots[WEAPSLOT_PRIMARY_FIRST] = (uint8_t)weapon;
        } else if (ps->weaponSlots[WEAPSLOT_PRIMARY_SECOND] == 0) {
            ps->weaponSlots[WEAPSLOT_PRIMARY_SECOND] = (uint8_t)weapon;
        }
        break;
    case WEAPSLOT_PISTOL:
    case WEAPSLOT_GRENADE:
    case WEAPSLOT_SMOKE_GRENADE:
    case WEAPSLOT_SATCHEL:
    case WEAPSLOT_BINOCULAR:
        if (ps->weaponSlots[weaponInfo->slot] == 0) {
            ps->weaponSlots[weaponInfo->slot] = (uint8_t)weapon;
        }
        break;
    default:
        break;
    }

    altWeapon = weaponInfo->altWeapon;
    while (altWeapon != 0) {
        uint32_t altBit = 1u << ((uint32_t)altWeapon & 31u);
        int32_t altWord = coduo_int32_sar((uint32_t)altWeapon, 5);
        if ((ps->weaponBits[altWord] & altBit) != 0) {
            break;
        }
        ps->weaponBits[altWord] |= altBit;
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ps->weaponRechamberBits[word] &= ~bit;
        altWeapon = bg_weaponInfos[altWeapon]->altWeapon;
    }
    return qtrue;
}

int32_t BG_TakePlayerWeapon(playerState_t *ps, int32_t weapon)
{
    uint32_t bit = 1u << ((uint32_t)weapon & 31u);
    int32_t word = coduo_int32_sar((uint32_t)weapon, 5);
    const weaponInfo_t *weaponInfo;
    int32_t occupiedSlot;
    int32_t altWeapon;

    if ((ps->weaponBits[word] & bit) == 0) {
        return 0;
    }
    weaponInfo = bg_weaponInfos[weapon];
    occupiedSlot = BG_IsPlayerWeaponInSlot(ps, weapon, qtrue);
    if (occupiedSlot != 0) {
        if (weaponInfo->stackable == 0) {
            ps->weaponSlots[occupiedSlot] = 0;
        } else {
            int32_t candidate = 1;
            qboolean found = qfalse;

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            while (candidate <= bg_numWeapons) {
                if (weaponInfo->stackable != 0) {
                    int32_t ignoredSlot = weaponInfo->slot;
                    (void)ignoredSlot;
                    if ((ps->weaponBits[coduo_int32_sar((uint32_t)candidate, 5)] & (1u << ((uint32_t)candidate & 31u))) != 0 &&
                        BG_IsPlayerWeaponInSlot(ps, candidate, qtrue) == 0) {
                        ps->weaponSlots[occupiedSlot] = (uint8_t)candidate;
                        if (candidate <= bg_numWeapons) {
                            found = qtrue;
                        }
                        break;
                    }
                }
                candidate = coduo_int32_from_bits((uint32_t)candidate + 1u);
            }
            if (found == qfalse) {
                ps->weaponSlots[occupiedSlot] = 0;
            }
        }
    }

    ps->weaponBits[word] &= ~bit;
    altWeapon = weaponInfo->altWeapon;
    while (altWeapon != 0) {
        uint32_t altBit = 1u << ((uint32_t)altWeapon & 31u);
        int32_t altWord = coduo_int32_sar((uint32_t)altWeapon, 5);
        if ((ps->weaponBits[altWord] & altBit) == 0) {
            break;
        }
        ps->weaponBits[altWord] &= ~altBit;
        altWeapon = bg_weaponInfos[altWeapon]->altWeapon;
    }
    return 1;
}

qboolean BG_CanItemBeGrabbed(const entityState_t *itemState, const playerState_t *ps, int32_t traceMode)
{
    int32_t itemIndex = itemState->itemIndex;
    const gitem_t *item;
    int32_t weapon;

    if (itemIndex < BG_FIRST_PICKUP_ITEM || itemIndex >= BG_PICKUP_ITEM_COUNT) {
        BG_WEAPON_ERROR("\x15"
                        "BG_CanItemBeGrabbed: index out of range");
    }
    itemIndex = itemState->itemIndex;
    item = &bg_itemlist[itemIndex];
    if (itemState->clientNum == ps->psClientNum) {
        return qfalse;
    }

    weapon = item->weapon;
    switch (item->type) {
    case IT_WEAPON:
        if ((ps->weaponBits[coduo_int32_sar((uint32_t)weapon, 5)] & (1u << ((uint32_t)weapon & 31u))) != 0) {
            return BG_GetMaxPickupableAmmo(ps, weapon) > 0 ? qtrue : qfalse;
        }
        return traceMode == 0 ? qtrue : qfalse;
    case IT_AMMO:
        if ((ps->weaponBits[coduo_int32_sar((uint32_t)weapon, 5)] & (1u << ((uint32_t)weapon & 31u))) != 0) {
            return BG_GetMaxPickupableAmmo(ps, weapon) > 0 ? qtrue : qfalse;
        }
        if (bg_weaponInfos[weapon]->clipRequired == 0) {
            return qfalse;
        }
        return BG_GetMaxPickupableAmmo(ps, weapon) > 0 ? qtrue : qfalse;
    case IT_HEALTH:
        return ps->stats[STAT_HEALTH] < ps->stats[STAT_MAX_HEALTH] ? qtrue : qfalse;
    case IT_BAD:
        BG_WEAPON_ERROR("\x15"
                        "BG_CanItemBeGrabbed: IT_BAD");
        return qfalse;
    default:
        return qfalse;
    }
}
