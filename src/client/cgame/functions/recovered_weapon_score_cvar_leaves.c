// Complete weapon-table, scoreboard, and view-size leaves recovered from the
// exact uo_cgame_mp_x86.dll instruction records named at each function.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_SizeUp_f(void) /* 0x30017270 */
{
    int32_t value =
        coduo_int32_from_bits((uint32_t)cg_viewSizeCvar.integer + 10u);
    trap_Cvar_Set("cg_viewsize", va("%i", value));
}

void CG_SizeDown_f(void) /* 0x300172a0 */
{
    int32_t value =
        coduo_int32_from_bits((uint32_t)cg_viewSizeCvar.integer - 10u);
    trap_Cvar_Set("cg_viewsize", va("%i", value));
}

// Source RVA: 0x30012260
int32_t AmmoPlusClip(const playerState_t *ps, int32_t weapon)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
    return coduo_int32_from_bits(
        (uint32_t)ps->clips[weaponInfo->clipIndex] +
        (uint32_t)ps->ammo[weaponInfo->ammoIndex]);
}

void CG_ScoresUp_f(void) /* 0x30017310: release of the +scores command */
{
    if (cg_scoreboardShowing) {
        cg_scoreboardShowing = qfalse;
        cg_scoreboardShowTime = (int32_t)cg_time;
    }
}

// Source RVA: 0x30037e20
qboolean IsScoreboardShowing(void)
{
    return cg_scoreboardShowing != qfalse ? qtrue : qfalse;
}
