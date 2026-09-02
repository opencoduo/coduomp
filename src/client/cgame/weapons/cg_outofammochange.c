#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x300475f0..0x30047746
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300475f0_30047746.mcode
//
// Role: the local player's current weapon has just run dry; pick the best
// replacement weapon and commit it. Part of the cgame weapon-selection cluster
// (siblings 0x30047820 CG_SelectFirstWeaponInSlot, 0x300478a0, the
// wrap/cycle selector 0x30047960, and the commit primitive 0x30047390
// CG_SelectWeaponIndex), all of which share the same held-bit / slot / ammo
// idioms and the same predicted-playerState globals. The sole caller
// (0x30022ef9) invokes it with no arguments after gating on cg_snap flags and
// the local client's predicted state, i.e. once per predicted frame when the
// active weapon is exhausted -- the classic CG_OutOfAmmoChange trigger.
//
// Name adjudication: the .mcode header's assigned GScr_PrecacheVehicle is a
// pure size-match (win size 0x156) and is REJECTED. That server name is a
// script builtin in vehicle.c; this function touches no script VM, no vehicle
// state, and no precache path. It reads cg_snap, cg_currentWeaponInfo, the
// weapon-held bitset, the predicted clip/ammo arrays and the weapon-slot table,
// then commits a choice through CG_SelectWeaponIndex -- weapon-select behavior,
// not precache. The same-module (cgame_mp.dll) PPC bank has CG_OutOfAmmoChange,
// whose documented behavior (switch away from an empty weapon to the next one
// that still has ammo) matches this body exactly; adopted behaviorally.
//
// ABI: takes no parameters (the caller at 0x30022ef9 sets up nothing before the
// plain CALL) and returns nothing meaningful (callers ignore EAX). The i386
// prologue reserves 0x1c bytes of stack (SUB ESP,0x1c) for the slot-priority
// scratch array; that and the EBX/ESI/EDI save/restore are calling-convention
// detail, not source behavior.
//
// Globals consumed (all in the cgame predicted-playerState cluster at 0x304831c4
// unless noted):
//   cg_snap (0x30459160)                       -- current client snapshot pointer
//   cg_currentWeaponInfo (0x30487980)          -- weaponInfo_t* of the active weapon
//   bg_weaponInfos[] (0x30134cd8)               -- weaponInfo_t* per registered weapon
//   bg_numWeapons (0x30134cd4)              -- count of registered weapons
//   cg_predictedPlayerState.weaponBits[] (0x304836f8)           -- "is weapon N held" bitset
//   cg_predictedPlayerState.weaponSlots[] (0x30483708) -- weapon index in each slot
//   cg_predictedPlayerState.clips[] (0x304834f8)     -- in-clip ammo, keyed by clipIndex
//   cg_predictedPlayerState.ammo[] (0x304832f8)      -- reserve ammo, keyed by ammoIndex
//   cg_predictedPlayerState (0x304831c4)       -- &cg.predictedPlayerState
//   cg_weaponSelect_vmCvar.integer (0x3044034c)              -- currently-selected weapon index

/* Default slot-scan priority order (0x300476a3..0x300476d3): the seven weapon
 * inventory slots are searched 1,2,3,4,5,7,6 -- slot 7 is preferred over slot 6
 * as the fallback. Written to the stack scratch array one dword per slot, then
 * indexed 0..6. */
enum { CG_OUT_OF_AMMO_SLOT_SCAN_COUNT = 7 };

void CG_OutOfAmmoChange(void)
{
    /* 0x300475f0 MOV EAX,[cg_snap] / TEST / JZ 0x30047725: if no snapshot is
     * installed there is no predicted player to change weapons for -- return. */
    if (cg_snap == 0) {
        return;
    }

    /* 0x30047600 MOV EDX,[cg_currentWeaponInfo]; 0x30047606 EAX =
     * currentWeaponInfo->stackable (+0x88). When the current weapon is
     * stackable (JZ 0x3004768d skips this whole loop otherwise), first look for
     * another held, stackable weapon in the SAME inventory slot that still has
     * ammo, and switch to it. */
    weaponInfo_t *currentWeaponInfo = cg_currentWeaponInfo;
    if (currentWeaponInfo->stackable != 0) {
        int32_t numWeapons = bg_numWeapons; /* 0x30047619 EDI */

        /* 0x3004761f MOV EAX,1 / CMP EDI,EAX / JL 0x3004768d: weapon indices run
         * 1..bg_numWeapons. */
        for (int32_t weapon = 1; weapon <= numWeapons; weapon++) {
            /* 0x30047630..0x30047648: skip weapons the player does not hold
             * (cg_predictedPlayerState.weaponBits[weapon>>5] & (1u << (weapon & 31))). */
            uint32_t heldWord = cg_predictedPlayerState.weaponBits[(uint32_t)weapon >> 5];
            uint32_t heldMask = 1u << ((uint32_t)weapon & 31u);
            if ((heldWord & heldMask) == 0) {
                continue;
            }

            /* 0x3004764a..0x30047655: skip weapons that are not themselves
             * stackable (weaponInfo_t::stackable +0x88 == 0). */
            weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
            if (weaponInfo->stackable == 0) {
                continue;
            }

            /* 0x30047657..0x30047663: only consider weapons whose inventory slot
             * (+0x84) matches the current weapon's slot. */
            if (weaponInfo->slot != currentWeaponInfo->slot) {
                continue;
            }

            /* 0x30047665..0x30047682: total = clips[clipIndex] + ammo[ammoIndex]
             * (weaponInfo reloaded from bg_weaponInfos[weapon]). If nonzero the
             * candidate has usable ammo -> commit it (JNZ 0x30047734). */
            weaponInfo = bg_weaponInfos[weapon];
            uint32_t totalBits =
                (uint32_t)cg_predictedPlayerState.clips[weaponInfo->clipIndex] +
                (uint32_t)cg_predictedPlayerState.ammo[weaponInfo->ammoIndex];
            if (totalBits != 0u) {
                /* 0x30047734 MOV ECX,[cg_weaponSelect_vmCvar.integer]; 0x3004773a CALL
                 * CG_SelectWeaponIndex(weapon=EAX, currentWeapon=ECX). */
                CG_SelectWeaponIndex(weapon, cg_weaponSelect_vmCvar.integer);
                return;
            }
        }
    }

    /* 0x3004768d..0x300476a1: ask BG_IsPlayerWeaponInSlot which inventory slot
     * currently holds the (exhausted) current weapon, checking alt-weapons too
     * (checkAlt=1). EDX = currentWeaponInfo->weaponIndex (+0x00). If the weapon
     * is not in any tracked slot (result 0), fall straight through to the
     * wrap/cycle fallback. */
    int32_t currentSlot = BG_IsPlayerWeaponInSlot(
        &cg_predictedPlayerState,
        currentWeaponInfo->weaponIndex,
        1);
    if (currentSlot != 0) {
        /* 0x300476a3..0x300476d3: build the default slot-scan priority order
         * {1,2,3,4,5,7,6}. */
        int32_t slotOrder[CG_OUT_OF_AMMO_SLOT_SCAN_COUNT];
        slotOrder[0] = 1;
        slotOrder[1] = 2;
        slotOrder[2] = 3;
        slotOrder[3] = 4;
        slotOrder[4] = 5;
        slotOrder[5] = 7;
        slotOrder[6] = 6;

        /* 0x300476db..0x30047714: scan the slots in priority order; select the
         * first slot whose weapon has clip+reserve ammo. */
        for (int32_t k = 0; k < CG_OUT_OF_AMMO_SLOT_SCAN_COUNT; k++) {
            /* 0x300476e0..0x300476ec: read the weapon index in this slot (a
             * byte). An empty slot (0) is skipped. */
            int8_t weaponInSlot = cg_predictedPlayerState.weaponSlots[slotOrder[k]];
            if (weaponInSlot == 0) {
                continue;
            }

            /* 0x300476ee..0x3004770e: the slot byte is sign-extended (MOVSX)
             * into a weapon index; total = clips[clipIndex] + ammo[ammoIndex].
             * If nonzero the slot's weapon has ammo -> commit it. */
            weaponInfo_t *weaponInfo = bg_weaponInfos[(int32_t)(int8_t)weaponInSlot];
            uint32_t totalBits =
                (uint32_t)cg_predictedPlayerState.clips[weaponInfo->clipIndex] +
                (uint32_t)cg_predictedPlayerState.ammo[weaponInfo->ammoIndex];
            if (totalBits != 0u) {
                /* 0x30047729..0x30047745: re-read the slot's weapon index
                 * (sign-extended) and commit via CG_SelectWeaponIndex. */
                CG_SelectWeaponIndex(
                    (int32_t)(int8_t)cg_predictedPlayerState.weaponSlots[slotOrder[k]],
                    cg_weaponSelect_vmCvar.integer);
                return;
            }
        }
    }

    /* 0x30047716..0x3004771f: no held/slotted weapon with ammo was found. Fall
     * back to the wrap/cycle selector, requesting a forward sweep that requires
     * ammo (PUSH 1, PUSH 1). */
    CG_CycleWeap(1, 1);
}
