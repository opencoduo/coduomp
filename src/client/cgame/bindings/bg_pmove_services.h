#ifndef CGAME_BG_PMOVE_SERVICES_H
#define CGAME_BG_PMOVE_SERVICES_H

#include "client/cgame/client_recovered.h"

/* The original cgame PmoveSingle body inlines its trap_SnapVector boundary as
 * cgame syscall 102. Keep the canonical shared-source spelling while retaining
 * that exact direct call in this module. */
#define trap_SnapVector(vector) ((void)cgame_syscall(CG_PM_NOTIFY_VELOCITY, (vector)))

/* The common BG source uses one role name for the module-owned prone-debug
 * cvar.  The cgame binary's original registration keeps its cg_ spelling. */
#define bg_debugProneCheck cg_debugProneCheck
#define BG_PM_WEAPON_INFO(weaponIndex) (bg_weaponInfos[(weaponIndex)])

/*
 * NOT_FROM_ORIGINAL_SOURCE: the otherwise common Windows PM_CheckDuck body
 * enables this diagnostic for cgame values >= 2, while the game modules test
 * for exactly 2.  Keep the proven module-owned cvar contract at the edge.
 */
static inline qboolean bg_compat_pmove_prone_debug_trace_enabled(void)
{
    return cg_debugProneCheck.integer >= 2 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: the client uses print-target value 3. */
static inline int32_t bg_compat_pmove_weapon_debug_target_none(void)
{
    return 3;
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: PM_UpdateViewAngles is otherwise instruction-
 * identical in the two Windows modules, but cgame reaches the renderer through
 * its syscall boundary while game calls its G_Debug* wrappers.  Keep that
 * module boundary local and inline it into the shared original body.
 */
static inline void bg_compat_pmove_debug_line(const vec3_t start, const vec3_t end)
{
    cgame_syscall(CG_ADD_DEBUG_LINE, (intptr_t)start, (intptr_t)end, (intptr_t)&cg_colorWhite, qtrue, qtrue);
}

static inline void bg_compat_pmove_debug_arc(const vec3_t center, float startAngle, float endAngle)
{
    CG_DebugCircleEx(center, qtrue, 16.0f, startAngle, endAngle, cg_colorWhite, qtrue);
}

#endif
