// Source: uo_cgame_mp_x86.dll weapon console handlers at the RVAs shown.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include <stdint.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

static qboolean CG_WeaponCommandReady(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical command
     * gates present in the four original handlers below. */
    return cg.snap != NULL &&
           (cg.predictedPlayerState.playerStateFlags & PMF_FOLLOW) == 0 &&
           (cg.snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0 &&
           (int32_t)((uint32_t)cg.time - (uint32_t)cg.weaponSelectTime) >= cg_weaponCycleDelay.integer;
}

void CG_AltWeapon_f(void) /* 0x30047400 */
{
    if (!CG_WeaponCommandReady()) return;
    if ((cg.predictedPlayerState.entityStateFlags & 0x106000) != 0 &&
        !(cg.predictedPlayerState.vehicleType == 1 &&
          cg.predictedPlayerState.vehiclePosition == 3)) return;

    int32_t current = cg_weaponSelect_vmCvar.integer;
    int32_t alternate = bg_weaponInfos[current]->altWeapon;
    if (alternate == 0) {
        const char *message = (const char *)(intptr_t)cgame_syscall(
            CG_SE_LOCALIZE_MESSAGE,
            (intptr_t)"CGAME_THIS_WEAPON_HAS_NO_ALTERNATE",
            (intptr_t)"game message");
        CG_GameMessage(message);
    } else if (CG_IsWeaponHeld(alternate)) {
        CG_FinishWeaponChange(alternate, current);
    }
}

void CG_NextWeapon_f(void) /* 0x300474c0 */
{
    if (cg.showScores) {
        if (cg.scoreboardScrollPos > 0) {
            cg.scoreboardScrollPos = coduo_int32_from_bits(
                (uint32_t)cg.scoreboardScrollPos -
                (uint32_t)cg_scoreboardScrollStep_vmCvar.integer);
            if (cg.scoreboardScrollPos < 0) cg.scoreboardScrollPos = 0;
        }
        return;
    }
    /* The binary SPLITS CG_WeaponCommandReady's gates around the vehicle branch: only
     * cg.snap!=NULL (0x300474f1) and !PMF_FOLLOW (0x300474fc test ch,0x40) gate the
     * "nextvehslot" command; PSF_ACTIVE_PLAYER (0x30047522) and the
     * weaponCycleDelay time check (0x30047539) come AFTER the vehicle branch and gate
     * ONLY CG_CycleWeap. A prior pass called the bundled CG_WeaponCommandReady() before
     * the vehicle branch, so the vehicle-slot command wrongly required PSF + the cycle
     * delay too. (CG_AltWeapon_f/CG_WeaponSlot_f keep the bundled helper -- their binary
     * gates all four before the vehicle test.) */
    if (cg.snap == NULL) return;
    if ((cg.predictedPlayerState.playerStateFlags & PMF_FOLLOW) != 0) return;
    if (cg.predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) {
        cgame_syscall(CG_SEND_CLIENT_COMMAND, (intptr_t)"nextvehslot");
        return;
    }
    if ((cg.snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) return;
    if ((int32_t)((uint32_t)cg.time - (uint32_t)cg.weaponSelectTime) <
        cg_weaponCycleDelay.integer) return;
    cg.weaponSelectTime = cg.time;
    CG_CycleWeap(1, 0);
}

void CG_PrevWeapon_f(void) /* 0x30047550 */
{
    if (cg.showScores) {
        if (cg.scoreboardOverflowed) {
            cg.scoreboardScrollPos = coduo_int32_from_bits(
                (uint32_t)cg.scoreboardScrollPos +
                (uint32_t)cg_scoreboardScrollStep_vmCvar.integer);
            const int32_t limit = coduo_int32_from_bits(
                (uint32_t)cg.numScores - 1u);
            if (cg.scoreboardScrollPos > limit)
                cg.scoreboardScrollPos = limit;
        }
        return;
    }
    /* Same split as CG_NextWeapon_f: cg.snap!=NULL (0x30047588) and !PMF_FOLLOW
     * (0x30047597) gate "prevvehslot"; PSF_ACTIVE_PLAYER (0x300475b9) and the
     * weaponCycleDelay time check (0x300475d1) come AFTER the vehicle branch and gate
     * only CG_CycleWeap. A prior pass used the bundled CG_WeaponCommandReady() first. */
    if (cg.snap == NULL) return;
    if ((cg.predictedPlayerState.playerStateFlags & PMF_FOLLOW) != 0) return;
    if (cg.predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) {
        cgame_syscall(CG_SEND_CLIENT_COMMAND, (intptr_t)"prevvehslot");
        return;
    }
    if ((cg.snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) return;
    if ((int32_t)((uint32_t)cg.time - (uint32_t)cg.weaponSelectTime) <
        cg_weaponCycleDelay.integer) return;
    cg.weaponSelectTime = cg.time;
    CG_CycleWeap(0, 0);
}

void CG_WeaponSlot_f(void) /* 0x30047750 */
{
    if (!CG_WeaponCommandReady()) return;
    if ((cg.predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) &&
        !(cg.snap->ps.vehicleType == 1 && cg.snap->ps.vehiclePosition == 3)) return;

    cg.weaponSelectTime = cg.time;
    const char *argument = CG_Argv(1);
    int32_t slot = BG_GetWeaponSlotForName(argument);
    if (slot == 0) slot = coduo_crt_atoi(CG_Argv(1));
    if (slot <= 0 || slot >= WEAPSLOT_COUNT) return;

    int32_t weapon = (int32_t)(int8_t)cg.predictedPlayerState.weaponSlots[slot];
    if (weapon != 0 && CG_IsWeaponHeld(weapon))
        CG_FinishWeaponChange(weapon, cg_weaponSelect_vmCvar.integer);
}
