#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30047960..0x30047bd3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30047960_30047bd3.mcode
//
// CG_CycleWeap — circular weapon-selection scanner. It is the orchestrator
// for the weapon-select cluster around CG_SelectWeaponIndex: if the current weapon
// belongs to a numbered inventory slot, scan neighboring slots first; otherwise scan
// held weapon indices first. The two fallback orders intentionally differ by path,
// matching the instruction flow at 0x30047a35 and 0x30047b57.
//
// Name adjudication: the .mcode header's assigned vmMain name is a size/corpus
// collision and is rejected. This function is not the exported VM dispatcher; it
// reads cg_snap, cg_weaponSelect_vmCvar.integer, bg_weaponInfos, cg weapon-slot/ammo globals, and
// commits cvars through CG_SelectWeaponIndex/trap_Cvar_Set. The existing header
// The Mac CG_CycleWeap shares BG_GetStackSlotForWeapon and
// CG_SelectFirstWeaponInSlot, and the corresponding next/previous-weapon command
// callers establish the same circular selection role, resolving the source name.

void CG_CycleWeap(int32_t forward, int32_t requireAmmo)
{
    playerState_t *ps;
    int32_t step;
    int32_t stopSlot;
    int32_t stopWeapon;
    int32_t currentWeapon;
    int32_t slot;

    /* 0x30047960..0x30047977: no weapon cycling unless a snapshot exists and the
     * local snapshot flags include the 0x80000 player-state flag. */
    if (cg_snap == 0) {
        return;
    }
    if ((cg_snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) {
        return;
    }

    if (forward != 0) {
        step = 1;
        stopSlot = WEAPSLOT_PRIMARY_FIRST;
        stopWeapon = 1;
    } else {
        step = -1;
        stopSlot = WEAPSLOT_LAST_DROPPABLE;
        stopWeapon = bg_numWeapons;
    }

    currentWeapon = cg_weaponSelect_vmCvar.integer;
    ps = &cg_predictedPlayerState;

    slot = BG_IsPlayerWeaponInSlot(ps, currentWeapon, 1);
    if (slot == WEAPSLOT_NONE) {
        slot = BG_GetStackSlotForWeapon(ps, currentWeapon, WEAPSLOT_NONE);
        if (slot == WEAPSLOT_NONE) {
            int32_t weapon = currentWeapon;
            int32_t wrapDelta = coduo_int32_from_bits((uint32_t)bg_numWeapons + (uint32_t)step - 1u);

            for (;;) {
                /* 0x30047a90..0x30047aa2: signed IDIV by bg_numWeapons, then
                 * convert the zero-based remainder back to a 1-based weapon index. */
                int32_t dividend = coduo_int32_from_bits((uint32_t)wrapDelta + (uint32_t)weapon);
                weapon = coduo_int32_from_bits((uint32_t)(dividend % bg_numWeapons) + 1u);
                if (weapon == stopWeapon) {
                    break;
                }

                uint32_t heldMask = 1u << ((uint32_t)weapon & 0x1f);
                int32_t heldWord = coduo_int32_sar((uint32_t)weapon, 5);
                if ((cg_predictedPlayerState.weaponBits[heldWord] & heldMask) == 0) {
                    continue;
                }

                weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
                int32_t altWeapon = weaponInfo->altWeapon;
                if (altWeapon != 0) {
                    for (;;) {
                        if (altWeapon == cg_weaponSelect_vmCvar.integer) {
                            goto next_weapon;
                        }
                        if (altWeapon == weapon) {
                            break;
                        }

                        altWeapon = bg_weaponInfos[altWeapon]->altWeapon;
                        if (altWeapon == 0) {
                            break;
                        }
                    }
                }

                if (BG_IsPlayerWeaponInSlot(ps, weapon, 1) != WEAPSLOT_NONE) {
                    continue;
                }
                if (BG_GetStackSlotForWeapon(ps, weapon, WEAPSLOT_NONE) != WEAPSLOT_NONE) {
                    continue;
                }

                if (requireAmmo != 0) {
                    weaponInfo = bg_weaponInfos[weapon];
                    if ((uint32_t)cg_predictedPlayerState.clips[weaponInfo->clipIndex] +
                            (uint32_t)cg_predictedPlayerState.ammo[weaponInfo->ammoIndex] ==
                        0u) {
                        continue;
                    }
                }

                CG_SelectWeaponIndex(weapon, cg_weaponSelect_vmCvar.integer);
                return;

            next_weapon:
                continue;
            }

            if (CG_SelectFirstWeaponInSlot(forward, requireAmmo)) {
                return;
            }
            CG_SelectFirstWeaponNotInSlot(forward, requireAmmo);
            goto reconcile_current_weapon;
        }
    }

    /* 0x300479d8..0x30047a33: walk neighboring inventory slots circularly. The
     * compiler emits (slot + step + 6) % 7 + 1; slots are 1..7 and the stop slot is
     * the wrap point, so the current slot itself is not reconsidered. */
    {
        int32_t slotDividend = coduo_int32_from_bits((uint32_t)slot + (uint32_t)step + (uint32_t)(WEAPSLOT_COUNT - 2));
        slot = coduo_int32_from_bits((uint32_t)(slotDividend % (WEAPSLOT_COUNT - 1)) + (uint32_t)WEAPSLOT_PRIMARY_FIRST);
    }
    while (slot != stopSlot) {
        int8_t slotWeaponByte = cg_predictedPlayerState.weaponSlots[slot];
        if (slotWeaponByte != 0) {
            if (requireAmmo == 0) {
                CG_SelectWeaponIndex((int32_t)(int8_t)slotWeaponByte, cg_weaponSelect_vmCvar.integer);
                return;
            }

            weaponInfo_t *weaponInfo = bg_weaponInfos[(int32_t)(int8_t)slotWeaponByte];
            if ((uint32_t)cg_predictedPlayerState.clips[weaponInfo->clipIndex] +
                    (uint32_t)cg_predictedPlayerState.ammo[weaponInfo->ammoIndex] !=
                0u) {
                CG_SelectWeaponIndex((int32_t)(int8_t)cg_predictedPlayerState.weaponSlots[slot], cg_weaponSelect_vmCvar.integer);
                return;
            }
        }

        {
            int32_t slotDividend = coduo_int32_from_bits((uint32_t)slot + (uint32_t)step + (uint32_t)(WEAPSLOT_COUNT - 2));
            slot = coduo_int32_from_bits((uint32_t)(slotDividend % (WEAPSLOT_COUNT - 1)) + (uint32_t)WEAPSLOT_PRIMARY_FIRST);
        }
    }

    if (CG_SelectFirstWeaponNotInSlot(forward, requireAmmo)) {
        return;
    }
    CG_SelectFirstWeaponInSlot(forward, requireAmmo);

reconcile_current_weapon:
    currentWeapon = cg_weaponSelect_vmCvar.integer;
    if ((cg_predictedPlayerState.weaponBits[coduo_int32_sar((uint32_t)currentWeapon, 5)] & (1u << ((uint32_t)currentWeapon & 0x1f))) != 0) {
        return;
    }

    cg_weaponSelectTime = (int32_t)cg_time;
    if (currentWeapon == 0) {
        return;
    }

    trap_Cvar_Set("cg_weaponSelect", va("%i", currentWeapon));
    trap_Cvar_Set("cl_run", "1");
}
