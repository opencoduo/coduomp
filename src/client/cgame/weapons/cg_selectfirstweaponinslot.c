#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30047820..0x30047898
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30047820_30047898.mcode
//
// Role: scan the local player's weapon inventory slots in one direction and
// select the first slot that holds a usable weapon, committing the choice via
// CG_SelectWeaponIndex. Part of the weapon-selection command cluster
// (0x300475f0 / 0x300478a0 / 0x30047960 all share the same slot/ammo idioms and
// the same globals). This helper is the plain linear slot sweep: forward selects
// slots 1..7 in order, backward selects slots 7..1.
//
// Name adjudication: the .mcode header's assigned MenuParse_execKey is a
// size-only match (win size 0x78) and is REJECTED — this function does no menu
// parsing; it reads the weapon-slot table, tests ammo, and calls
// CG_SelectWeaponIndex. The Mac CG_SelectFirstWeaponInSlot is called by
// CG_CycleWeap in the same position where Windows CG_CycleWeap calls this
// directional slot sweep, resolving the source name despite the inlined Mac
// weapon-selection tail.
//
// Non-default register ABI (proven from the caller at 0x30047a52, which sets
// EAX=ESI = the forward/backward direction flag and EDI=[ESP+0x20] = the
// require-ammo flag before the plain CALL; the function ends in a plain RET and
// takes no stack arguments):
//   EAX = forward   : nonzero -> sweep slots 1..7; zero -> sweep slots 7..1
//   EDI = requireAmmo: when nonzero, skip a slot's weapon unless it has ammo
// Returns qtrue in EAX if a weapon was selected, qfalse if no slot qualified.
//
// Globals consumed (all fields of the cgame predicted playerState at 0x304831c4):
//   cg_predictedPlayerState.weaponSlots[slot] (0x30483708) — weapon index in each slot
//   cg_predictedPlayerState.clips[clipIndex]  (0x304834f8) — in-clip ammo
//   cg_predictedPlayerState.ammo[ammoIndex]   (0x304832f8) — reserve ammo
//   bg_weaponInfos[weapon] (0x30134cd8) — weaponInfo_t*, for clipIndex/ammoIndex
//   cg_weaponSelect_vmCvar.integer (0x3044034c) — the currently-selected weapon index
qboolean CG_SelectFirstWeaponInSlot(int32_t forward, int32_t requireAmmo)
{
    /* 0x30047820..0x30047837: pick the sweep direction and starting slot.
     * forward != 0 -> step=+1, slot=1 (0x30047826/0x3004782b).
     * forward == 0 -> step=-1, slot=7 (0x3004782f OR ESI,-1 / 0x30047832 MOV
     * ECX,7). */
    int32_t step;
    int32_t slot;
    if (forward != 0) {
        step = 1;
        slot = 1;
    } else {
        step = -1;
        slot = 7;
    }

    for (;;) {
        /* 0x30047840 MOV AL,byte[ECX + cg_predictedPlayerState.weaponSlots]: read the
         * weapon assigned to this slot (byte). 0x30047846 TEST AL,AL / JZ
         * 0x30047870: an empty slot (weapon 0) is skipped. */
        int8_t weaponInSlot = cg_predictedPlayerState.weaponSlots[slot];
        if (weaponInSlot != 0) {
            /* 0x3004784a TEST EDI,EDI / JZ 0x3004787e: when ammo is not required,
             * select this slot's weapon immediately. */
            qboolean qualifies;
            if (requireAmmo == 0) {
                qualifies = qtrue;
            } else {
                /* 0x3004784e..0x3004786e: total = clips[weaponInfo->clipIndex]
                 * + ammo[weaponInfo->ammoIndex]. The slot byte is sign-extended
                 * (MOVSX) into the weapon index. Select only if total != 0
                 * (0x3004786e JNZ -> select). */
                weaponInfo_t *weaponInfo = bg_weaponInfos[(int32_t)(int8_t)weaponInSlot];
                /* ADD EBX,[ammo] is modulo 2^32; only ZF is consumed. */
                uint32_t totalBits = (uint32_t)cg_predictedPlayerState.clips[weaponInfo->clipIndex] +
                                     (uint32_t)cg_predictedPlayerState.ammo[weaponInfo->ammoIndex];
                qualifies = (totalBits != 0u) ? qtrue : qfalse;
            }

            if (qualifies) {
                /* 0x3004787e MOVSX EAX,byte[ECX + cg_predictedPlayerState.weaponSlots]:
                 * re-read the slot's weapon index (sign-extended).
                 * 0x30047885 MOV ECX,cg_weaponSelect_vmCvar.integer.
                 * 0x3004788b CALL CG_SelectWeaponIndex(weapon, currentWeapon).
                 * 0x30047891 MOV EAX,1: return qtrue. */
                CG_SelectWeaponIndex((int32_t)(int8_t)cg_predictedPlayerState.weaponSlots[slot], cg_weaponSelect_vmCvar.integer);
                return qtrue;
            }
        }

        /* 0x30047870 ADD ECX,ESI: advance to the next slot in the sweep
         * direction. 0x30047872 JZ 0x30047879: stepping below slot 1 wraps to 0
         * -> stop (backward sweep exhausted). 0x30047874 CMP ECX,8 / JNZ
         * 0x30047840: slot 8 means the forward sweep passed slot 7 -> stop; any
         * other slot re-enters the loop. */
        slot += step;
        if (slot == 0 || slot == WEAPSLOT_COUNT) {
            /* 0x30047879..0x3004787d: return qfalse (XOR EAX,EAX). */
            return qfalse;
        }
    }
}
