// Source: uo_cgame_mp_x86.dll 0x30034a00..0x30034abf
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034a00_30034abf.mcode
//
// CG_CheckAmmo — per-frame check that plays the "player_out_of_ammo" local
// warning sound when the local player runs low on / out of ammunition, using a
// persistent state so the sound fires only on the transition into the low state.
//
// Name evidence: the sound it plays is the registered "player_out_of_ammo" handle
// (cg_soundOutOfAmmo, 0x3044bbb8 — see globals.c: the registration pass 0x3002b560
// stores syscall-0xc3("player_out_of_ammo") there). The Mac cgame symbol table has
// CG_CheckAmmo for this per-frame warning check; the separate CG_OutOfAmmoChange is
// already recovered at 0x300475f0. The mechanical size-guess "QuatEigenTrace" is
// rejected outright: this function does integer weapon-bit / ammo-sum logic and a
// local-sound call — no quaternion or eigen math whatsoever.
//
// Machine-code notes (every statement is traced to the .mcode):
//   - cg_snap == *(snapshot_t **)0x30459160 (current client snapshot).
//   - The embedded playerState begins at snapshot+0x0c, so snapshot+0x140 is
//     ps.ammo[] (cg_snap->ps.ammo) and snapshot+0x540 is ps.weaponBits[4]
//     (cg_snap->ps.weaponBits) — corroborated by server playerState_s
//     (ammo[128] @ ps+0x134, weaponBits[4] @ ps+0x534).
//   - bg_numWeapons == *(int *)0x30134cd4, bg_weaponInfos == *(weaponInfo_t ***)0x30134cd8.
//   - The per-weapon ammo index is bg_weaponInfos[i]->ammoIndex (weaponInfo_t +0x1e8).
//   - All four psWeaponBits dwords are loaded up front; psWeaponBits[2]/[3] are
//     stored into two stack locals that the rest of the body never re-reads
//     (dead stores preserved as a plain local copy for machine-code fidelity).
//   - The scaling constant is IMUL ...,0x3e8 == *1000; the threshold is
//     CMP ...,0x1388 == 5000 (signed JGE). Both are ordinary integer quantities.
//   - The final state value comes from SETZ CL / INC ECX: (total == 0) ? 2 : 1.
//   - Takes no arguments and cleans its own stack (RET, no imm); void return.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

void CG_CheckAmmo(void)
{
    snapshot_t *snap = cg_snap;

    /* Load the 128-bit owned-weapon mask. Only weaponBits[0] is scanned by the
     * loop below; weaponBits[1] participates in the early-out guard, and
     * weaponBits[2]/[3] are copied into locals but not otherwise consumed (the
     * machine code stores them to [ESP+0x10]/[ESP+0x14] and never reads them). */
    uint32_t weaponBits0 = snap->ps.weaponBits[0];
    uint32_t weaponBits1 = snap->ps.weaponBits[1];
    uint32_t weaponBits2 = snap->ps.weaponBits[2]; /* dead store, kept for fidelity */
    uint32_t weaponBits3 = snap->ps.weaponBits[3]; /* dead store, kept for fidelity */
    (void)weaponBits2;
    (void)weaponBits3;

    /* If the player owns no weapons in the first 64 bits of the mask, do nothing
     * (this early return does not touch cg_outOfAmmoState). */
    if (weaponBits0 == 0 && weaponBits1 == 0) {
        return;
    }

    uint32_t totalBits = 0u;
    for (int32_t i = 1; i < bg_numWeapons; i++) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (weaponBits0 & (1u << ((uint32_t)i & 31u))) {
            int32_t ammoIndex = bg_weaponInfos[i]->ammoIndex;
            /* IMUL and ADD wrap modulo 2^32, then CMP/JGE interprets the
             * accumulated bits as a signed value. */
            totalBits += 1000u * (uint32_t)snap->ps.ammo[ammoIndex];
            int32_t signedTotal;
            memcpy(&signedTotal, &totalBits, sizeof(signedTotal));
            if (signedTotal >= 5000) {
                /* Enough ammo across owned weapons: clear the warning state. */
                cg_outOfAmmoState = 0;
                return;
            }
        }
    }

    /* Below threshold. Fire the warning sound only on the transition from the
     * "not warned" state (0). CG_PlaySoundAliasByName receives &cg_snap->ps.psOrigin
     * as the channel object, the registered sound identifier, and the local
     * client number. */
    if (cg_outOfAmmoState == 0) {
        CG_PlaySoundAliasByName(snap->ps.psClientNum, &snap->ps.psOrigin,
                                cg_soundOutOfAmmo);
    }

    /* Record the warned state: 2 when completely out of ammo, 1 otherwise. */
    cg_outOfAmmoState = (totalBits == 0u) ? 2 : 1;
}
