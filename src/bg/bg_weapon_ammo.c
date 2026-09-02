#include "bg_pmove.h"

#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"

#include <stdint.h>

/*
 * Current-clip mutation and query primitives shared by cgame and game.  The
 * original Windows modules contain instruction-identical bodies apart from
 * the bg_weaponInfos and pm global addresses:
 *
 *   uo_cgame_mp_x86.dll  0x30012350, 0x30012390, 0x300123b0
 *   uo_game_mp_x86.dll   0x20012290, 0x200122d0, 0x200122f0
 *
 * The Linux game bodies at RVAs 0x00033e90, 0x00033f16, and 0x00033f50 call
 * BG_ClipForWeapon rather than inlining its weaponInfo_t::clipIndex load, then
 * perform the same modulo-dword subtraction, signed zero clamp, count return,
 * and zero predicate.  The Mac and Linux symbol banks supply the canonical
 * PM_WeaponUseAmmo and PM_WeaponAmmoAvailable names for cgame's corresponding
 * recovered bodies.
 */

void PM_WeaponUseAmmo(int32_t weapon, int32_t amount)
{
    const int32_t clipIndex = BG_GetInfoForWeapon(weapon)->clipIndex;
    playerState_t *const ps = pm->ps;

    ps->clips[clipIndex] = coduo_int32_from_bits(
        (uint32_t)ps->clips[clipIndex] - (uint32_t)amount);
    if (ps->clips[clipIndex] < 0) {
        ps->clips[clipIndex] = 0;
    }
}

int32_t PM_WeaponAmmoAvailable(int32_t weapon)
{
    const int32_t clipIndex = BG_GetInfoForWeapon(weapon)->clipIndex;

    return pm->ps->clips[clipIndex];
}

qboolean PM_WeaponClipEmpty(int32_t weapon)
{
    const int32_t clipIndex = BG_GetInfoForWeapon(weapon)->clipIndex;

    return pm->ps->clips[clipIndex] == 0 ? qtrue : qfalse;
}
