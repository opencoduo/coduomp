#include "bg_pmove.h"

void Com_Printf(const char *format, ...);

/*
 * Complete PM weapon-debug print cluster shared by cgame and game.
 *
 * The two authoritative Windows bodies agree instruction for instruction
 * apart from relocations and module-owned globals:
 *
 *   uo_cgame_mp_x86.dll  PM_Weapon_PrintWeaponState 0x30014a80
 *   uo_game_mp_x86.dll   PM_Weapon_PrintWeaponState 0x200149c0
 *   uo_cgame_mp_x86.dll  PM_Weapon_PrintWeaponAnim  0x30014bd0
 *   uo_game_mp_x86.dll   PM_Weapon_PrintWeaponAnim  0x20014b10
 *
 * The Linux game module retains the same guard, stores, print order, and
 * switches at RVAs 0x000377cc and 0x0003793a.  The supporting Mac cgame and
 * game modules export these exact PM_ names.  PMDebugPrefix and the two cache
 * words remain module-owned because their load-state values differ: cgame
 * uses the "CG" prefix and -1 caches, while game uses the "pm" prefix and
 * zero-initialized caches.
 */

void PM_Weapon_PrintWeaponState(void)
{
    const int32_t weaponState = pm->ps->weaponState;

    if (PMDebugLastWeaponState == weaponState) {
        return;
    }

    Com_Printf(" %i %s_", pm->command.commandTime, PMDebugPrefix);
    PMDebugLastWeaponState = weaponState;
    Com_Printf("WEAP_STATE -- ");

    switch ((uint32_t)weaponState) {
    case WEAPON_STATE_IDLE:
        Com_Printf("WEAPON_READY\n");
        break;
    case WEAPON_STATE_RAISING:
        Com_Printf("WEAPON_RAISING\n");
        break;
    case WEAPON_STATE_DROPPING:
        Com_Printf("WEAPON_DROPPING\n");
        break;
    case WEAPON_STATE_FIRING:
        Com_Printf("WEAPON_FIRING\n");
        break;
    case WEAPON_STATE_RECHAMBERING:
        Com_Printf("WEAPON_RECHAMBERING\n");
        break;
    case WEAPON_STATE_RELOADING:
        Com_Printf("WEAPON_RELOADING\n");
        break;
    case WEAPON_STATE_RELOADING_INTERRUPT:
        Com_Printf("WEAPON_RELOADING_INTERUPT\n");
        break;
    case WEAPON_STATE_RELOAD_START:
        Com_Printf("WEAPON_RELOAD_START\n");
        break;
    case WEAPON_STATE_RELOAD_START_INTERRUPT:
        Com_Printf("WEAPON_RELOAD_START_INTERUPT\n");
        break;
    case WEAPON_STATE_RELOAD_END:
        Com_Printf("WEAPON_RELOAD_END\n");
        break;
    case WEAPON_STATE_MELEE_WINDUP:
        Com_Printf("WEAPON_MELEE_WINDUP\n");
        break;
    case WEAPON_STATE_MELEE_RELAX:
        Com_Printf("WEAPON_MELEE_RELAX\n");
        break;
    default:
        Com_Printf("UNKNOWN\n");
        break;
    }
}

void PM_Weapon_PrintWeaponAnim(void)
{
    const uint32_t weaponAnim = pm->ps->weaponAnim & ~(uint32_t)ANIM_TOGGLEBIT;

    Com_Printf(" %i %s_", pm->command.commandTime, PMDebugPrefix);
    PMDebugLastWeaponAnim = weaponAnim;
    Com_Printf("WEAP_ANIM -- ");

    switch (weaponAnim) {
    case PM_WEAPON_ANIM_IDLE:
        Com_Printf("WEAP_IDLE\n");
        break;
    case PM_WEAPON_ANIM_FIRE:
        Com_Printf("WEAP_ATTACK\n");
        break;
    case PM_WEAPON_ANIM_FIRE_LASTSHOT:
        Com_Printf("WEAP_ATTACK_LASTSHOT\n");
        break;
    case PM_WEAPON_ANIM_RECHAMBER:
        Com_Printf("WEAP_RECHAMBER\n");
        break;
    case PM_WEAPON_ANIM_ADS_FIRE:
        Com_Printf("WEAP_ADS_ATTACK\n");
        break;
    case PM_WEAPON_ANIM_ADS_FIRE_LASTSHOT:
        Com_Printf("WEAP_ADS_ATTACK_LASTSHOT\n");
        break;
    case PM_WEAPON_ANIM_ADS_RECHAMBER:
        Com_Printf("WEAP_ADS_RECHAMBER\n");
        break;
    case PM_WEAPON_ANIM_MELEE:
        Com_Printf("WEAP_MELEE_ATTACK\n");
        break;
    case PM_WEAPON_ANIM_LOWER:
        Com_Printf("WEAP_DROP\n");
        break;
    case PM_WEAPON_ANIM_SWITCH_RAISE:
        Com_Printf("WEAP_RAISE\n");
        break;
    case PM_WEAPON_ANIM_RELOAD:
        Com_Printf("WEAP_RELOAD\n");
        break;
    case PM_WEAPON_ANIM_RELOAD_EMPTY:
        Com_Printf("WEAP_RELOAD_EMPTY\n");
        break;
    case PM_WEAPON_ANIM_RELOAD_START:
        Com_Printf("WEAP_RELOAD_START\n");
        break;
    case PM_WEAPON_ANIM_RELOAD_END:
        Com_Printf("WEAP_RELOAD_END\n");
        break;
    case PM_WEAPON_ANIM_ALT_SWITCH_LOWER:
        Com_Printf("WEAP_ALTSWITCHFROM\n");
        break;
    case PM_WEAPON_ANIM_ALT_SWITCH_RAISE:
        Com_Printf("WEAP_ALTSWITCHTO\n");
        break;
    case PM_WEAPON_ANIM_DEPLOYED:
        Com_Printf("WEAP_DEPLOYED\n");
        break;
    case PM_WEAPON_ANIM_ADS_IN:
        Com_Printf("WEAP_DEPLOY\n");
        break;
    case PM_WEAPON_ANIM_ADS_OUT:
        Com_Printf("WEAP_BREAKDOWN\n");
        break;
    default:
        Com_Printf("UNKNOWN\n");
        break;
    }
}
