#include "../module/ui_functions.h"

#define UI_FEEDER_ZERO_LIST 0.0f
#define UI_FEEDER_ACTIVE_MAPS 1.0f
#define UI_FEEDER_SERVERS 2.0f
#define UI_FEEDER_ACTIVE_MAPS_ALT 4.0f
#define UI_FEEDER_PLAYERS 7.0f
#define UI_FEEDER_MODS 9.0f
#define UI_FEEDER_DEMOS 10.0f
#define UI_FEEDER_SERVER_STATUS 13.0f
#define UI_FEEDER_FOUND_PLAYER_SERVERS 14.0f
#define UI_FEEDER_MOVIES 15.0f

enum {
    UI_PLAYER_REFRESH_MILLISECONDS = 3000
};

// Source: uo_ui_mp_x86.dll 0x4000e9e0..0x4000eaff
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000e9e0_4000eaff.mcode
// Exact same-module PPC symbol: UI_FeederCount.
int32_t UI_FeederCount(float feeder)
{
    if (feeder == UI_FEEDER_ZERO_LIST) {
        return ui_teamCount;
    }
    if (feeder == UI_FEEDER_MOVIES) {
        return ui_movieCount;
    }
    if (feeder == UI_FEEDER_ACTIVE_MAPS ||
        feeder == UI_FEEDER_ACTIVE_MAPS_ALT) {
        return UI_MapCountByGameType();
    }
    if (feeder == UI_FEEDER_SERVERS) {
        UI_UpdateDisplayServers();
        return ui_displayServerCount;
    }
    if (feeder == UI_FEEDER_SERVER_STATUS) {
        return ui_serverStatusInfo.numLines;
    }
    if (feeder == UI_FEEDER_FOUND_PLAYER_SERVERS) {
        return ui_foundPlayerServerCount;
    }
    if (feeder == UI_FEEDER_PLAYERS) {
        if (ui_displayContextStorage.context.realTime >
            ui_playerRefreshDeadline) {
            ui_playerRefreshDeadline = (int32_t)(
                (uint32_t)ui_displayContextStorage.context.realTime +
                (uint32_t)UI_PLAYER_REFRESH_MILLISECONDS);
            UI_BuildPlayerList();
        }
        return ui_playerCount;
    }
    if (feeder == UI_FEEDER_MODS) {
        return ui_modCount;
    }
    if (feeder == UI_FEEDER_DEMOS) {
        return ui_demoCount;
    }
    return 0;
}
