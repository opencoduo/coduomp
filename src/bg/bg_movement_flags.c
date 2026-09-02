#include "bg_pmove.h"

#include "qcommon/q_bits.h"

#include <stdint.h>

/*
 * The authoritative Windows cgame/game bodies agree instruction for
 * instruction apart from relocated globals and the Com_BitCheck call:
 *
 *   uo_cgame_mp_x86.dll  0x3000d7a0, 0x3000d800
 *   uo_game_mp_x86.dll   0x2000d550, 0x2000d5b0
 *
 * Linux game retains the same tests and updates at RVAs 0x0002c622 and
 * 0x0002c74a.  Its FUCOM comparisons have the same source-level behavior as
 * the Windows FCOMP branches: a NaN fatigue value does not take either
 * ordered less-than-or-equal rejection path.
 */

void PM_UpdatePlayerWalkingFlag(void)
{
    playerState_t *const ps = pm->ps;

    ps->playerStateFlags &= ~(uint32_t)PMF_WALKING;

    if (ps->pmType >= PM_TYPE_DEAD) {
        return;
    }
    if ((pm->command.buttons & PM_BUTTON_ADS) == 0) {
        return;
    }
    if ((ps->playerStateFlags & PMF_PRONE) != 0) {
        return;
    }
    if ((ps->playerStateFlags & PMF_ADS) == 0) {
        return;
    }
    if ((ps->entityStateFlags & EF_IN_VEHICLE) != 0) {
        return;
    }

    switch (ps->weaponState) {
    case WEAPON_STATE_RELOADING:
    case WEAPON_STATE_RELOAD_START:
    case WEAPON_STATE_RELOAD_END:
    case WEAPON_STATE_RELOAD_START_INTERRUPT:
    case WEAPON_STATE_RELOADING_INTERRUPT:
        return;
    default:
        break;
    }

    ps->playerStateFlags |= PMF_WALKING;
}

void PM_UpdatePlayerSprintingFlag(void)
{
    playerState_t *const ps = pm->ps;
    const qboolean wasSprinting =
        (ps->playerStateFlags & PMF_SPRINTING) != 0 ? qtrue : qfalse;
    const uint8_t buttons = pm->command.buttons;
    uint32_t flags;

    ps->playerStateFlags &= ~(uint32_t)PMF_SPRINTING;

    if (ps->pmType >= PM_TYPE_SPECTATOR) {
        return;
    }
    if ((buttons & PM_BUTTON_SPRINT) == 0) {
        return;
    }
    if (ps->fatigueScale <= 0.0f) {
        return;
    }
    if (wasSprinting == qfalse && ps->fatigueScale <= 0.25f) {
        return;
    }

    flags = ps->playerStateFlags;
    if ((flags & (PMF_PRONE | PMF_DUCKED | PMF_ADS)) != 0) {
        return;
    }
    if (wasSprinting == qfalse &&
        ps->groundEntityNum == ENTITYNUM_NONE) {
        return;
    }
    if (pm->command.forwardmove == 0 && pm->command.rightmove == 0) {
        return;
    }
    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0) {
        return;
    }
    if (ps->weaponState == WEAPON_STATE_BREAKING_DOWN) {
        return;
    }

    if (pml.weaponInfo->weaponType == WEAPTYPE_GRENADE) {
        if (pml.weaponInfo->specialTimeEnabled != 0 &&
            ps->grenadeTimeLeft < pml.weaponInfo->specialTimeThreshold &&
            ps->grenadeTimeLeft != 0 &&
            Com_BitCheck(ps->weaponBits, ps->currentWeapon) != 0) {
            return;
        }
        if ((buttons & PM_BUTTON_FIRE) != 0) {
            return;
        }
        if (ps->weaponState == WEAPON_STATE_FIRING) {
            return;
        }
    }

    if ((flags & PMF_LADDER) != 0) {
        return;
    }

    ps->playerStateFlags |= PMF_SPRINTING;
}
