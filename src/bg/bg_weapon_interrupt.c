#include "bg_pmove.h"

#include "bg_weapon.h"

/*
 * Weapon-state interruption predicates shared by cgame and game. The retained
 * Windows bodies are instruction-identical apart from relocated globals and
 * calls:
 *
 *   uo_cgame_mp_x86.dll  0x30012120, 0x300121b0
 *   uo_game_mp_x86.dll   0x20012060, 0x200120f0
 *
 * Linux game implements the same state matrices at RVAs 0x000338b0 and
 * 0x00033a44. Its explicit BG_GetInfoForWeapon and PM_ContinueWeaponAnim calls
 * are inlined by the Windows compiler where their guards are visible. The Mac
 * cgame/game symbol banks independently preserve both canonical names,
 * including the original `Interupt` spelling.
 */

qboolean PM_InteruptWeaponWithProneMove(void)
{
    playerState_t *ps = pm->ps;

    if ((ps->playerStateFlags & PMF_ADS) != 0 && BG_GetInfoForWeapon(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG) {
        return qfalse;
    }

    switch (ps->weaponState) {
    case WEAPON_STATE_IDLE:
    case WEAPON_STATE_RAISING:
    case WEAPON_STATE_DROPPING:
    case WEAPON_STATE_RECHAMBERING:
    case WEAPON_STATE_RELOADING:
    case WEAPON_STATE_RELOADING_INTERRUPT:
    case WEAPON_STATE_RELOAD_START:
    case WEAPON_STATE_RELOAD_START_INTERRUPT:
    case WEAPON_STATE_RELOAD_END:
        return qtrue;

    case WEAPON_STATE_FIRING:
    case WEAPON_STATE_MELEE_RELAX:
        return qfalse;

    default:
        ps->weaponTime = 0;
        ps->weaponDelay = 0;
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_ContinueWeaponAnim(PM_WEAPON_ANIM_IDLE);
        return qtrue;
    }
}

qboolean PM_InteruptWeaponWithSprintMove(void)
{
    playerState_t *ps = pm->ps;

    switch (ps->weaponState) {
    case WEAPON_STATE_IDLE:
    case WEAPON_STATE_RAISING:
    case WEAPON_STATE_DROPPING:
    case WEAPON_STATE_RECHAMBERING:
    case WEAPON_STATE_RELOADING:
    case WEAPON_STATE_RELOADING_INTERRUPT:
    case WEAPON_STATE_RELOAD_START:
    case WEAPON_STATE_RELOAD_START_INTERRUPT:
    case WEAPON_STATE_RELOAD_END:
        return qtrue;

    case WEAPON_STATE_FIRING:
    case WEAPON_STATE_MELEE_WINDUP:
    case WEAPON_STATE_MELEE_RELAX:
        return qfalse;

    default:
        ps->weaponTime = 0;
        ps->weaponDelay = 0;
        ps->weaponState = WEAPON_STATE_IDLE;
        PM_ContinueWeaponAnim(PM_WEAPON_ANIM_SWITCH_RAISE);
        return qtrue;
    }
}
