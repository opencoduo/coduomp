#ifndef GAME_BG_PRONE_SERVICES_H
#define GAME_BG_PRONE_SERVICES_H

#include "server/game/recovered_game.h"
#include "server/game/game_functions.h"
#include "bg/bg_prone_debug.h"

extern const vec4_t colorRed;
extern const vec4_t colorGreen;
extern const vec4_t colorYellow;
extern const vec4_t colorMagenta;
extern const vec4_t colorCyan;
extern const vec4_t colorMdCyan;

/* NOT_FROM_ORIGINAL_SOURCE: map the shared prone debug identities to the
 * game-owned original color objects. */
static inline const float *bg_compat_prone_debug_color(bg_prone_debug_color_t color)
{
    switch (color) {
    case BG_PRONE_DEBUG_RED:
        return colorRed;
    case BG_PRONE_DEBUG_GREEN:
        return colorGreen;
    case BG_PRONE_DEBUG_YELLOW:
        return colorYellow;
    case BG_PRONE_DEBUG_MAGENTA:
        return colorMagenta;
    case BG_PRONE_DEBUG_CYAN:
        return colorCyan;
    case BG_PRONE_DEBUG_MEDIUM_CYAN:
        return colorMdCyan;
    }
    return colorRed;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the game-owned diagnostic cvar and draw
 * functions at the module boundary. */
static inline qboolean bg_compat_prone_debug_enabled(void)
{
    return g_debugProneCheck.integer != 0 ? qtrue : qfalse;
}

static inline void bg_compat_prone_debug_line(const vec3_t start, const vec3_t end, bg_prone_debug_color_t color)
{
    if (bg_compat_prone_debug_enabled() != qfalse) {
        G_DebugLine(start, end, bg_compat_prone_debug_color(color), g_debugProneCheckDepthCheck.integer, 1);
    }
}

static inline void bg_compat_prone_debug_box(const vec3_t mins, const vec3_t maxs, bg_prone_debug_color_t color)
{
    G_DebugBox(mins, maxs, bg_compat_prone_debug_color(color), g_debugProneCheckDepthCheck.integer, 1);
}

static inline void bg_compat_prone_debug_circle(const vec3_t center, float radius, const vec3_t normal, bg_prone_debug_color_t color)
{
    G_DebugCircleEx(center, radius, normal, bg_compat_prone_debug_color(color), g_debugProneCheckDepthCheck.integer, 1);
}

#endif
