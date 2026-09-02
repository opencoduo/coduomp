#include "../module/ui_functions.h"

enum {
    UI_GAMETYPE_WRAP_INDEX = 1,
    UI_GAMETYPE_SKIPPED_INDEX = 2,
    UI_GAMETYPE_FORWARD_AFTER_SKIP = 3,
    UI_MAP_FEEDER = 1
};

// Source: uo_ui_mp_x86.dll 0x4000b2d0..0x4000b39d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b2d0_4000b39d.mcode
// Exact same-module PPC symbol: UI_GameType_HandleKey.
qboolean UI_GameType_HandleKey(int32_t flags, float *special, int32_t key)
{
    int32_t oldMapCount;

    (void)special;

    if (key != K_MOUSE1 && key != K_MOUSE2 && key != K_ENTER && key != K_KP_ENTER) {
        return qfalse;
    }

    oldMapCount = UI_MapCountByGameType();
    if (key == K_MOUSE2) {
        --ui_gameType;
        if (ui_gameType == UI_GAMETYPE_SKIPPED_INDEX) {
            ui_gameType = UI_GAMETYPE_WRAP_INDEX;
        } else if (ui_gameType < UI_GAMETYPE_SKIPPED_INDEX) {
            /* Any decremented value below the skipped slot (1, 0, or
             * negative) wraps to the last game type. */
            ui_gameType = ui_gameTypeCount - 1;
        }
    } else {
        ++ui_gameType;
        if (ui_gameType >= ui_gameTypeCount) {
            ui_gameType = UI_GAMETYPE_WRAP_INDEX;
        } else if (ui_gameType == UI_GAMETYPE_SKIPPED_INDEX) {
            ui_gameType = UI_GAMETYPE_FORWARD_AFTER_SKIP;
        }
    }

    trap_Cvar_Set("ui_gameType", va("%d", ui_gameType));
    if (flags != 0 && oldMapCount != UI_MapCountByGameType()) {
        trap_Cvar_Set("ui_currentMap", "0");
        Menu_SetFeederSelection(NULL, NULL, UI_MAP_FEEDER, 0);
    }
    return qtrue;
}
