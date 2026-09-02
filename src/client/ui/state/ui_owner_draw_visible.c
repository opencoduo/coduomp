#include "../module/ui_functions.h"

enum {
    UI_SHOW_LEADER = 0x0001,
    UI_SHOW_NOTLEADER = 0x0002,
    UI_SHOW_FAVORITESERVERS = 0x0004,
    UI_SHOW_NEWHIGHSCORE = 0x0020,
    UI_SHOW_DEMOAVAILABLE = 0x0040,
    UI_SHOW_NEWBESTTIME = 0x0080,
    UI_SHOW_FFA = 0x0100,
    UI_SHOW_NOTFFA = 0x0200,
    UI_SHOW_NOTFAVORITESERVERS = 0x1000,
    UI_SHOW_HANDLED_FLAGS = UI_SHOW_LEADER | UI_SHOW_NOTLEADER | UI_SHOW_FAVORITESERVERS | UI_SHOW_NEWHIGHSCORE | UI_SHOW_DEMOAVAILABLE |
                            UI_SHOW_NEWBESTTIME | UI_SHOW_FFA | UI_SHOW_NOTFFA | UI_SHOW_NOTFAVORITESERVERS,
    UI_GAMETYPE_COMPARE_LIMIT = 99999
};

// Source: uo_ui_mp_x86.dll 0x4000b050..0x4000b208
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b050_4000b208.mcode
// Exact same-module PPC symbol: UI_OwnerDrawVisible.
qboolean UI_OwnerDrawVisible(int32_t ownerDrawFlags)
{
    qboolean visible = qtrue;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ownerDrawFlags &= UI_SHOW_HANDLED_FLAGS;
    while (ownerDrawFlags != 0) {
        if ((ownerDrawFlags & UI_SHOW_FFA) != 0) {
            if (Q_stricmpn("dm", UI_Cvar_VariableString("g_gametype"), UI_GAMETYPE_COMPARE_LIMIT) != 0) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_FFA;
        }
        if ((ownerDrawFlags & UI_SHOW_NOTFFA) != 0) {
            if (Q_stricmpn("dm", UI_Cvar_VariableString("g_gametype"), UI_GAMETYPE_COMPARE_LIMIT) == 0) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_NOTFFA;
        }
        if ((ownerDrawFlags & UI_SHOW_LEADER) != 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (ui_teamLeader == 0 || (cg_selectedPlayerCvar.integer >= 0 && cg_selectedPlayerCvar.integer < ui_teamPlayerCount &&
                                       ui_playerNumbers[cg_selectedPlayerCvar.integer] == ui_myClientNum)) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_LEADER;
        }
        if ((ownerDrawFlags & UI_SHOW_NOTLEADER) != 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (ui_teamLeader != 0 && (cg_selectedPlayerCvar.integer < 0 || cg_selectedPlayerCvar.integer >= ui_teamPlayerCount ||
                                       ui_playerNumbers[cg_selectedPlayerCvar.integer] != ui_myClientNum)) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_NOTLEADER;
        }
        if ((ownerDrawFlags & UI_SHOW_FAVORITESERVERS) != 0) {
            if (ui_netSource != LAN_SERVER_SOURCE_FAVORITES) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_FAVORITESERVERS;
        }
        if ((ownerDrawFlags & UI_SHOW_NOTFAVORITESERVERS) != 0) {
            if (ui_netSource == LAN_SERVER_SOURCE_FAVORITES) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_NOTFAVORITESERVERS;
        }
        if ((ownerDrawFlags & UI_SHOW_NEWHIGHSCORE) != 0) {
            if (ui_displayContextStorage.newHighScoreTime < ui_displayContextStorage.context.realTime) {
                visible = qfalse;
            } else if (ui_displayContextStorage.soundHighScore && trap_Cvar_VariableValue("sv_killserver") == 0.0f) {
                trap_MSS_PlayLocalSoundAlias(ui_newHighScoreSound);
                ui_displayContextStorage.soundHighScore = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_NEWHIGHSCORE;
        }
        if ((ownerDrawFlags & UI_SHOW_NEWBESTTIME) != 0) {
            if (ui_displayContextStorage.newBestTime < ui_displayContextStorage.context.realTime) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_NEWBESTTIME;
        }
        if ((ownerDrawFlags & UI_SHOW_DEMOAVAILABLE) != 0) {
            if (!ui_displayContextStorage.demoAvailable) {
                visible = qfalse;
            }
            ownerDrawFlags &= ~UI_SHOW_DEMOAVAILABLE;
        }
    }

    return visible;
}
