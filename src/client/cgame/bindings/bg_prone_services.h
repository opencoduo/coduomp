#ifndef CGAME_BG_PRONE_SERVICES_H
#define CGAME_BG_PRONE_SERVICES_H

#include "client/cgame/client_recovered.h"
#include "bg/bg_prone_debug.h"

/* NOT_FROM_ORIGINAL_SOURCE: map the shared prone debug identities to the
 * cgame-owned original color objects. */
static inline const float *bg_compat_prone_debug_color(bg_prone_debug_color_t color)
{
    switch (color) {
    case BG_PRONE_DEBUG_RED:
        return bg_proneColorRed;
    case BG_PRONE_DEBUG_GREEN:
        return bg_proneColorGreen;
    case BG_PRONE_DEBUG_YELLOW:
        return bg_proneColorYellow;
    case BG_PRONE_DEBUG_MAGENTA:
        return bg_proneColorMagenta;
    case BG_PRONE_DEBUG_CYAN:
        return bg_proneColorCyan;
    case BG_PRONE_DEBUG_MEDIUM_CYAN:
        return bg_proneColorMediumCyan;
    }
    return bg_proneColorRed;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the cgame-owned diagnostic cvars to the
 * shared original body without moving target-only UI state into shared BG. */
static inline qboolean bg_compat_prone_debug_enabled(void)
{
    return cg_debugProneCheck.integer != 0 ? qtrue : qfalse;
}

static inline void bg_compat_prone_debug_line(const vec3_t start, const vec3_t end, bg_prone_debug_color_t color)
{
    if (bg_compat_prone_debug_enabled() != qfalse) {
        cgame_syscall(CG_ADD_DEBUG_LINE, (intptr_t)start, (intptr_t)end, (intptr_t)bg_compat_prone_debug_color(color),
                      cg_debugProneCheckDepthCheck.integer, qtrue);
    }
}

static inline void bg_compat_prone_debug_box(const vec3_t mins, const vec3_t maxs, bg_prone_debug_color_t color)
{
    CG_DebugBox(mins, maxs, bg_compat_prone_debug_color(color), cg_debugProneCheckDepthCheck.integer);
}

static inline void bg_compat_prone_debug_circle(const vec3_t center, float radius, const vec3_t normal, bg_prone_debug_color_t color)
{
    CG_DebugCircle(normal, center, radius, bg_compat_prone_debug_color(color), cg_debugProneCheckDepthCheck.integer, qtrue);
}

#endif
