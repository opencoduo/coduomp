#ifndef GAME_BG_PMOVE_SERVICES_H
#define GAME_BG_PMOVE_SERVICES_H

/* The game modules retain the original trap_SnapVector veneer. */
#include "server/game/game_functions.h"
#include "server/game/game_globals.h"
#include "server/game/g_syscalls.h"

#define bg_debugProneCheck g_debugProneCheck
#if defined(WINDOWS_BEHAVIOR)
#define BG_PM_WEAPON_INFO(weaponIndex) (bg_weaponInfos[(weaponIndex)])
#else
#define BG_PM_WEAPON_INFO(weaponIndex) BG_GetInfoForWeapon((weaponIndex))
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE: both original game modules enable the additional
 * PM_CheckDuck prone trace only for the exact debug-cvar value 2.
 */
static inline qboolean bg_compat_pmove_prone_debug_trace_enabled(void)
{
    return g_debugProneCheck.integer == 2 ? qtrue : qfalse;
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: both game authorities use print-target value 2
 * at this module boundary.
 */
static inline int32_t bg_compat_pmove_weapon_debug_target_none(void)
{
    return 2;
}

void G_DebugArc(const float *center, float radius, float startAngle, float endAngle, const float *color, int depthTest, int duration);
extern const vec4_t colorWhite;

/* NOT_FROM_ORIGINAL_SOURCE: local side of the shared PM debug-draw boundary. */
static inline void bg_compat_pmove_debug_line(const vec3_t start, const vec3_t end)
{
    G_DebugLine(start, end, colorWhite, qtrue, 1);
}

static inline void bg_compat_pmove_debug_arc(const vec3_t center, float startAngle, float endAngle)
{
    G_DebugArc(center, 16.0f, startAngle, endAngle, colorWhite, qtrue, 1);
}

#endif
