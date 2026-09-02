#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x300478a0..0x30047956
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300478a0_30047956.mcode
//
// CG_SelectFirstWeaponNotInSlot: scan the local player's held weapons by weapon
// index (over
// bg_weaponInfos[1..bg_numWeapons], gated by the cg_predictedPlayerState.weaponBits ownership
// bitset) in one direction and select the first held weapon that is NOT already
// assigned to a numbered inventory slot, committing the choice via
// CG_SelectWeaponIndex. This is the by-index sibling of the by-slot sweep
// CG_SelectFirstWeaponInSlot (0x30047820); both live in the
// weapon-selection command cluster (0x300475f0 / 0x30047820 / 0x300478a0 /
// 0x30047960) and share the same held-bit / ammo / CG_SelectWeaponIndex idioms
// and the same cg predicted-playerState globals.
//
// Name adjudication: CG_SelectFirstWeaponNotInSlot is recovered from the Mac
// cgame traceback symbol at code-section offset 0x55720. Its complete 0xec-byte
// PPC body has the same two inputs, scan direction, ownership test,
// BG_IsPlayerWeaponInSlot/BG_GetStackSlotForWeapon gates, optional ammo test,
// selection call, and boolean result. The .mcode header's assigned
// `Damage_Falloff` is a pure
// win-size match (0xb6 == 0xb6) and is REJECTED — this function contains no
// floating point, no distance compare, and no damage attenuation. It is integer
// weapon-inventory logic: it iterates a weapon-ownership bitset, calls the two
// BG inventory-slot helpers, tests ammo, and calls CG_SelectWeaponIndex. No exact
// same-module Windows symbol is present; the independent PPC body proves the
// exact shared-source name.
//
// Non-default register ABI (proven from the two callers at 0x30047a40 and
// 0x30047b65 in FUN_30047960, each of which sets EAX = a direction flag and
// PUSHes the require-ammo flag as one cdecl stack slot — caller does `ADD ESP,4`
// after the CALL — before the plain CALL; the function ends in a plain RET):
//   EAX          = forward     : nonzero -> scan weapon indices 1..bg_numWeapons
//                                ascending; zero -> scan bg_numWeapons..1
//                                descending (0x300478a6 JZ selects the branch).
//   [ESP+4]      = requireAmmo : when nonzero, skip a weapon unless it has ammo.
// Returns qtrue in EAX if a weapon was selected, qfalse if none qualified.
//
// Globals consumed:
//   cg_predictedPlayerState.weaponBits[]                (0x304836f8) — 128-bit "weapon N held" mask
//   cg_predictedPlayerState.clips[clipIndex] (0x304834f8) — in-clip ammo
//   cg_predictedPlayerState.ammo[ammoIndex]  (0x304832f8) — reserve ammo
//   bg_weaponInfos[weapon]              (0x30134cd8) — weaponInfo_t*, for clip/ammo index
//   bg_numWeapons                   (0x30134cd4) — registered weapon-info count
//   cg_weaponSelect_vmCvar.integer                   (0x3044034c) — currently-selected weapon index
//   &cg_predictedPlayerState           (0x304831c4) — playerState_t passed to the BG helpers
//
// Selection predicate (per candidate weapon `w`): the weapon must be held
// (cg_predictedPlayerState.weaponBits bit set), BG_IsPlayerWeaponInSlot(ps, w, checkAlt=1) must
// return 0 (the weapon is not currently placed in a tracked inventory slot), and
// BG_GetStackSlotForWeapon(ps, w, slot=0) must return 0 (it does not stack into a
// slot either). Such a "loose" held weapon is the selection target. When
// requireAmmo is set, it additionally requires clips[clipIndex] + ammo[ammoIndex]
// to be nonzero.

qboolean CG_SelectFirstWeaponNotInSlot(int32_t forward, int32_t requireAmmo)
{
    // 0x300478a0..0x300478bf: choose scan direction and starting weapon index.
    //   forward != 0 -> step=+1, weapon=1              (0x300478a8/0x300478ad)
    //   forward == 0 -> weapon=bg_numWeapons, step=-1 (0x300478b1/0x300478b7)
    int32_t step;
    int32_t weapon;
    if (forward != 0) {
        step = 1;
        weapon = 1;
    } else {
        weapon = bg_numWeapons;
        step = -1;
    }

    // ESI/EDI in the machine code hold &cg_predictedPlayerState (0x304831c4),
    // reloaded each time it is passed to a BG helper; expressed here as the
    // playerState_t base pointer.
    playerState_t *ps = &cg_predictedPlayerState;

    for (;;) {
        // 0x300478c0..0x300478d8: is weapon `w` held? word = w >> 5 (SAR, signed),
        // bit = 1u << (w & 0x1f). TEST cg_predictedPlayerState.weaponBits[word], bit / JZ -> advance.
        uint32_t bit = 1u << ((uint32_t)weapon & 0x1f);
        int32_t word = coduo_int32_sar((uint32_t)weapon, 5);
        if ((cg_predictedPlayerState.weaponBits[word] & bit) != 0) {
            // 0x300478da..0x300478ec: BG_IsPlayerWeaponInSlot(ps, weapon, checkAlt=1).
            // Nonzero means the weapon already occupies a tracked inventory slot;
            // such a weapon is skipped (JNZ -> advance).
            if (BG_IsPlayerWeaponInSlot(ps, weapon, 1) == 0) {
                // 0x300478ee..0x300478f9: BG_GetStackSlotForWeapon(ps, weapon, slot=0).
                // (EAX==0 here from the prior helper's result -> slot arg is 0.)
                // Nonzero means the weapon stacks into a slot; skip it (JNZ -> advance).
                if (BG_GetStackSlotForWeapon(ps, weapon, 0) == 0) {
                    // 0x300478fb..0x30047901: when requireAmmo is clear, accept
                    // this weapon immediately.
                    qboolean qualifies;
                    if (requireAmmo == 0) {
                        qualifies = qtrue;
                    } else {
                        // 0x30047903..0x30047926: wi = bg_weaponInfos[weapon];
                        // total = clips[wi->clipIndex] + ammo[wi->ammoIndex].
                        // The ADD sets ZF; total == 0 means the weapon has no ammo.
                        weaponInfo_t *wi = bg_weaponInfos[weapon];
                        int32_t total = coduo_int32_from_bits((uint32_t)cg_predictedPlayerState.clips[wi->clipIndex] +
                                                              (uint32_t)cg_predictedPlayerState.ammo[wi->ammoIndex]);
                        qualifies = (total != 0) ? qtrue : qfalse;
                    }

                    if (qualifies) {
                        // 0x30047928..0x3004793e: commit the selection and return
                        // qtrue. ECX=cg_weaponSelect_vmCvar.integer, EAX=weapon before the CALL.
                        CG_SelectWeaponIndex(weapon, cg_weaponSelect_vmCvar.integer);
                        return qtrue;
                    }
                    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
                }
            }
        }

        // 0x3004793f..0x30047955: advance to the next weapon index in the scan
        // direction. weapon += step; if it reached 0 (ZF from ADD), stop and
        // return qfalse. Otherwise if it equals bg_numWeapons, keep looping;
        // if it does NOT equal bg_numWeapons, also keep looping (JNZ loop_top).
        // Only the weapon==0 case (0x30047941 JZ) and reaching the count without a
        // match fall through to return qfalse.
        weapon = coduo_int32_from_bits((uint32_t)weapon + (uint32_t)step);
        if (weapon == 0) {
            // 0x30047941 JZ 0x3004794f: XOR EAX,EAX -> qfalse.
            return qfalse;
        }
        if (weapon == bg_numWeapons) {
            // 0x30047943 CMP EBX,bg_numWeapons / 0x30047949 JNZ 0x300478c0:
            // when weapon EQUALS the count, the JNZ is not taken and control
            // falls through to 0x3004794f -> qfalse. (Forward scan started at 1
            // and ends when it wraps to the count; backward scan ends when it
            // decrements to 0, handled above.)
            return qfalse;
        }
        // 0x30047949 JNZ 0x300478c0: continue scanning.
    }
}
