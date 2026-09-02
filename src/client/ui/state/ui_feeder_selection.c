#include <string.h>

#include "../module/ui_functions.h"

#define UI_FEEDER_TEAMS 0.0f
#define UI_FEEDER_ACTIVE_MAPS 1.0f
#define UI_FEEDER_SERVERS 2.0f
#define UI_FEEDER_ACTIVE_MAPS_ALT 4.0f
#define UI_FEEDER_PLAYERS 7.0f
#define UI_FEEDER_TEAM_PLAYERS 8.0f
#define UI_FEEDER_MODS 9.0f
#define UI_FEEDER_DEMOS 10.0f
#define UI_FEEDER_SERVER_STATUS 13.0f
#define UI_FEEDER_FOUND_PLAYER_SERVERS 14.0f
#define UI_FEEDER_MOVIES 15.0f

enum {
    UI_SERVER_INFO_SIZE = 1024,
    UI_SERVER_STATUS_FEEDER = 13,
    UI_CINEMATIC_UNINITIALIZED = -1,
    UI_SERVER_ADDRESS_COPY_SIZE = 63
};

// Source: uo_ui_mp_x86.dll 0x4000f1c0..0x4000f530
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f1c0_4000f530.mcode
// Exact same-module PPC symbol: UI_FeederSelection.
void UI_FeederSelection(float feeder, int32_t index)
{
    if (feeder == UI_FEEDER_TEAMS) {
        uiTeamInfo_t *team;

        if (index < 0 || index >= ui_teamCount) {
            return;
        }
        team = &ui_teams[index];
        trap_Cvar_Set("team_model", team->selected ? "janet" : "james");
        trap_Cvar_Set("team_headmodel", va("*%s", team->name));
        return;
    }

    if (feeder == UI_FEEDER_SERVERS) {
        const char *mapName;

        ui_currentServer = index;
        ui_compat_lan_get_server_info((int32_t)ui_netSource, ui_displayServers[index], ui_selectedServerInfo, UI_SERVER_INFO_SIZE);
        mapName = Info_ValueForKey(ui_selectedServerInfo, "mapname");
        ui_serverMapPreviewShader = trap_R_RegisterShaderNoMip(va("levelshots/%s", mapName), R_IMAGE_TRACK_UI);
        if (ui_serverMapCinematic >= 0) {
            trap_CIN_StopCinematic(ui_serverMapCinematic);
            ui_serverMapCinematic = UI_CINEMATIC_UNINITIALIZED;
        }
        return;
    }

    if (feeder == UI_FEEDER_SERVER_STATUS) {
        return;
    }
    if (feeder == UI_FEEDER_FOUND_PLAYER_SERVERS) {
        ui_foundPlayerServerIndex = index;
        if (index >= ui_foundPlayerServerCount - 1) {
            return;
        }
        /* Retail reserves slot 0: found-player results are written at [count]
         * with count starting at 1, so readers use the next 64-byte slot
         * (+0x40 base 0x4023943c). */
        strncpy(ui_serverStatusAddress, ui_foundPlayerServerAddresses[index + 1], UI_SERVER_ADDRESS_COPY_SIZE);
        ui_serverStatusAddress[UI_SERVER_ADDRESS_COPY_SIZE] = '\0';
        Menu_SetFeederSelection(NULL, NULL, UI_SERVER_STATUS_FEEDER, 0);
        UI_BuildServerStatus(qtrue);
        return;
    }
    if (feeder == UI_FEEDER_PLAYERS) {
        ui_playerIndex = index;
        return;
    }
    if (feeder == UI_FEEDER_TEAM_PLAYERS) {
        ui_teamPlayerIndex = index;
        return;
    }
    if (feeder == UI_FEEDER_MODS) {
        ui_modIndex = index;
        return;
    }
    if (feeder == UI_FEEDER_MOVIES) {
        ui_movieIndex = index;
        if (ui_previewMovie >= 0) {
            trap_CIN_StopCinematic(ui_previewMovie);
        }
        ui_previewMovie = UI_CINEMATIC_UNINITIALIZED;
        return;
    }
    if (feeder == UI_FEEDER_DEMOS) {
        ui_demoIndex = index;
        return;
    }

    if (feeder == UI_FEEDER_ACTIVE_MAPS || feeder == UI_FEEDER_ACTIVE_MAPS_ALT) {
        int32_t oldMapIndex = feeder == UI_FEEDER_ACTIVE_MAPS_ALT ? ui_currentNetMap : ui_currentMap;
        int32_t actualMapIndex;

        if (ui_maps[oldMapIndex].cinematic >= 0) {
            trap_CIN_StopCinematic(ui_maps[oldMapIndex].cinematic);
            ui_maps[oldMapIndex].cinematic = UI_CINEMATIC_UNINITIALIZED;
        }
        UI_SelectedMap(index, &actualMapIndex);
        trap_Cvar_Set("ui_mapIndex", va("%d", index));
        // The original also refreshes the vmCvar mirror directly (store to
        // 0x401fbf6c at 0x4000f4a7), so same-frame readers see the new index.
        ui_mapIndexCvar.integer = index;

        ui_currentMap = actualMapIndex;
        trap_Cvar_Set("ui_currentMap", va("%d", actualMapIndex));
        if (feeder == UI_FEEDER_ACTIVE_MAPS_ALT) {
            ui_currentNetMap = actualMapIndex;
            trap_Cvar_Set("ui_currentNetMap", va("%d", actualMapIndex));
        }
    }
}
