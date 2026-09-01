#include "bg_pmove.h"

/*
 * Low-level weapon-animation state mutation shared by cgame and game.
 * The two original Windows bodies are instruction-identical apart from the
 * pm/pml global addresses:
 *
 *   uo_cgame_mp_x86.dll  PM_StartWeaponAnim    0x300123e0
 *   uo_game_mp_x86.dll   PM_StartWeaponAnim    0x20012320
 *   uo_cgame_mp_x86.dll  PM_ContinueWeaponAnim 0x30012430
 *   uo_game_mp_x86.dll   PM_ContinueWeaponAnim 0x20012370
 *
 * The Linux game module bodies at RVAs 0x00033f9c and 0x00034024 retain the
 * same signed pmType gate, command-weapon gate, gas-weapon overwrite, restart
 * bit toggle, and masked comparison against the PM_TYPE_DEAD boundary.
 */

void PM_StartWeaponAnim(pmWeaponAnim_t weaponAnim)
{
    playerState_t *const ps = pm->ps;

    if (ps->pmType >= PM_TYPE_DEAD ||
        pm->command.weapon == 0) {
        return;
    }

    if (pml.weaponInfo->weaponType == WEAPTYPE_GAS) {
        ps->weaponAnim = (uint32_t)weaponAnim;
        return;
    }

    ps->weaponAnim =
        ((~ps->weaponAnim) & (uint32_t)ANIM_TOGGLEBIT) |
        (uint32_t)weaponAnim;
}

void PM_ContinueWeaponAnim(pmWeaponAnim_t weaponAnim)
{
    if (pm->command.weapon == 0 ||
        (pm->ps->weaponAnim & ~(uint32_t)ANIM_TOGGLEBIT) ==
            (uint32_t)weaponAnim) {
        return;
    }

    PM_StartWeaponAnim(weaponAnim);
}
