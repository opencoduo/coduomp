#include <stdlib.h>

#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

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
    UI_SERVER_INFO_REFRESH_WINDOW = 5000,
    UI_SERVER_COLUMN_PASSWORD = 0,
    UI_SERVER_COLUMN_HARDWARE = 1,
    UI_SERVER_COLUMN_NAME = 2,
    UI_SERVER_COLUMN_MAP = 3,
    UI_SERVER_COLUMN_CLIENTS = 4,
    UI_SERVER_COLUMN_GAMETYPE = 5,
    UI_SERVER_COLUMN_PUNKBUSTER = 6,
    UI_SERVER_COLUMN_MOD = 7,
    UI_SERVER_COLUMN_PING = 8,
    UI_SERVER_COLUMN_COUNT = 9,
    UI_STATUS_COLUMN_COUNT = 4
};

// Source: uo_ui_mp_x86.dll 0x4000ebf0..0x4000f033
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000ebf0_4000f033.mcode
// Exact same-module PPC symbol: UI_FeederItemText.
const char *UI_FeederItemText(float feeder, int32_t index, int32_t column,
                              int32_t *imageHandle)
{
    static const char empty[] = "";

    *imageHandle = UI_FEEDER_IMAGE_HANDLE_NONE;

    if (feeder == UI_FEEDER_TEAMS) {
        if (index >= 0 && index < ui_teamCount) {
            return ui_teams[index].name;
        }
        return empty;
    }

    if (feeder == UI_FEEDER_ACTIVE_MAPS ||
        feeder == UI_FEEDER_ACTIVE_MAPS_ALT) {
        int32_t mapIndex;
        return UI_SelectedMap(index, &mapIndex);
    }

    if (feeder == UI_FEEDER_SERVERS) {
        const char *value;
        int32_t ping;

        UI_UpdateDisplayServers();
        if (index < 0 || index >= ui_displayServerCount) {
            return empty;
        }
        if (ui_cachedServerInfoColumn != column ||
            ui_cachedServerInfoTime >
                (int32_t)((uint32_t)ui_displayContextStorage.context.realTime +
                          (uint32_t)UI_SERVER_INFO_REFRESH_WINDOW)) {
            ui_compat_lan_get_server_info(
                (int32_t)ui_netSource, ui_displayServers[index],
                ui_cachedServerInfo, UI_SERVER_INFO_SIZE);
            ui_cachedServerInfoColumn = column;
            ui_cachedServerInfoTime =
                ui_displayContextStorage.context.realTime;
        }

        ping = coduo_crt_atoi(
            Info_ValueForKey(ui_cachedServerInfo, "ping"));
        if (column < 0 || column >= UI_SERVER_COLUMN_COUNT) {
            return empty;
        }
        switch (column) {
        case UI_SERVER_COLUMN_PASSWORD:
            return coduo_crt_atoi(
                       Info_ValueForKey(ui_cachedServerInfo, "pswrd")) != 0
                       ? "X"
                       : empty;

        case UI_SERVER_COLUMN_HARDWARE: {
            int32_t hardware = coduo_crt_atoi(
                Info_ValueForKey(ui_cachedServerInfo, "hw"));
            if (hardware >= 0 &&
                hardware < UI_SERVER_HARDWARE_SHADER_COUNT) {
                *imageHandle = ui_serverHardwareShaders[hardware];
            }
            return empty;
        }

        case UI_SERVER_COLUMN_NAME:
            return Info_ValueForKey(ui_cachedServerInfo,
                                    ping > 0 ? "hostname" : "addr");

        case UI_SERVER_COLUMN_MAP:
            return Info_ValueForKey(ui_cachedServerInfo, "mapname");

        case UI_SERVER_COLUMN_CLIENTS: {
            const char *maximum =
                Info_ValueForKey(ui_cachedServerInfo, "sv_maxclients");
            const char *clients =
                Info_ValueForKey(ui_cachedServerInfo, "clients");
            Com_sprintf(ui_serverClientText, sizeof(ui_serverClientText),
                        "%s (%s)", clients, maximum);
            return ui_serverClientText;
        }

        case UI_SERVER_COLUMN_GAMETYPE:
            value = Info_ValueForKey(ui_cachedServerInfo, "gametype");
            return value != NULL && value[0] != '\0' ? value : "?";

        case UI_SERVER_COLUMN_PUNKBUSTER:
            if (coduo_crt_atoi(
                    Info_ValueForKey(ui_cachedServerInfo, "pb")) != 0) {
                *imageHandle = ui_punkbusterShader;
            }
            return empty;

        case UI_SERVER_COLUMN_MOD:
            return coduo_crt_atoi(
                       Info_ValueForKey(ui_cachedServerInfo, "mod")) != 0
                       ? "X"
                       : empty;

        case UI_SERVER_COLUMN_PING:
            return ping > 0
                       ? Info_ValueForKey(ui_cachedServerInfo, "ping")
                       : "...";
        }
    }

    if (feeder == UI_FEEDER_SERVER_STATUS) {
        const char *text;

        if (index < 0 || index >= ui_serverStatusInfo.numLines ||
            column < 0 || column >= UI_STATUS_COLUMN_COUNT) {
            return empty;
        }
        text = ui_serverStatusInfo.lines[index].column[column];
        if (text[0] == '@') {
            return UI_SafeTranslateString(text + 1);
        }
        return text;
    }

    if (feeder == UI_FEEDER_FOUND_PLAYER_SERVERS) {
        if (index >= 0 && index < ui_foundPlayerServerCount) {
            /* Retail reserves slot 0: found-player results are written at
             * [count] with count starting at 1, so readers use the next
             * 64-byte slot (+0x40 base 0x4023983c). */
            return ui_foundPlayerServerNames[index + 1];
        }
        return empty;
    }
    if (feeder == UI_FEEDER_PLAYERS) {
        if (index >= 0 && index < ui_playerCount) {
            return ui_playerNames[index];
        }
        return empty;
    }
    if (feeder == UI_FEEDER_TEAM_PLAYERS) {
        if (index >= 0 && index < ui_teamPlayerCount) {
            return ui_teamPlayerNames[index];
        }
        return empty;
    }
    if (feeder == UI_FEEDER_MODS) {
        if (index >= 0 && index < ui_modCount) {
            const char *description = ui_mods[index].description;
            if (description != NULL && description[0] != '\0') {
                return description;
            }
            return ui_mods[index].directory;
        }
        return empty;
    }
    if (feeder == UI_FEEDER_MOVIES) {
        if (index >= 0 && index < ui_movieCount) {
            return ui_movieNames[index];
        }
        return empty;
    }
    if (feeder == UI_FEEDER_DEMOS) {
        if (index >= 0 && index < ui_demoCount) {
            return ui_demoNames[index];
        }
        return empty;
    }
    return empty;
}
