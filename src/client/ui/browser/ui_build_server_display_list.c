#include <stdint.h>
#include <string.h>

#include "../module/ui_functions.h"
#include "../module/ui_globals.h"
#include "client/common/client_branding.h"
#include "client/common/client_legacy_crt.h"

enum {
    UI_SERVER_INFO_SIZE = 1024,
    UI_SERVER_FEEDER = 2,
    UI_SERVER_REBUILD_FINAL = 2,
    UI_ALL_SERVERS = -1,
    UI_SERVER_LIST_RETRY_MILLISECONDS = 500
};

// Source: uo_ui_mp_x86.dll 0x4000d8e0..0x4000deca
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d8e0_4000deca.mcode
// Exact same-module PPC symbol: UI_BuildServerDisplayList.
void UI_BuildServerDisplayList(int32_t force)
{
    char serverInfo[UI_SERVER_INFO_SIZE];
    int32_t serverCount;
    int32_t server;

    if (force == 0 && ui_displayContextStorage.context.realTime <= ui_nextDisplayRefresh) {
        return;
    }
    if (force == UI_SERVER_REBUILD_FINAL) {
        force = 0;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_joinGameType < 0 || ui_joinGameType >= ui_joinGameTypeCount) {
        ui_joinGameType = 0;
        trap_Cvar_Set("ui_joinGameType", "0");
    }
    if (ui_serverFilterType < 0 || ui_serverFilterType >= UI_SERVER_FILTER_COUNT) {
        ui_serverFilterType = 0;
    }

    trap_Cvar_VariableStringBuffer("cl_motdString", ui_motd, sizeof(ui_motd));
    if (ui_motd[0] == '\0') {
        strcpy(ui_motd, va("%s - %s", UI_SafeTranslateString("EXE_COD_MULTIPLAYER"), "1.51"));
    }
    if ((int32_t)strlen(ui_motd) != ui_motdLength) {
        ui_motdLength = (int32_t)strlen(ui_motd);
        ui_motdOffset = -1;
    }

    if (force != 0) {
        ui_filteredServerCount = 0;
        ui_displayServerCount = 0;
        ui_numPlayers = 0;
        ui_serverCount = trap_LAN_GetServerCount((int32_t)ui_netSource);
        if (ui_currentServer >= 0) {
            Menu_SetFeederSelection(NULL, NULL, UI_SERVER_FEEDER, 0);
        }
        trap_LAN_MarkServerDirty((int32_t)ui_netSource, UI_ALL_SERVERS, qtrue);
    }

    serverCount = trap_LAN_GetServerCount((int32_t)ui_netSource);
    if (trap_LAN_WaitServerResponse((int32_t)ui_netSource) || (ui_netSource == LAN_SERVER_SOURCE_LOCAL && serverCount == 0)) {
        ui_displayServerCount = 0;
        ui_numPlayers = 0;
        ui_serverCount = trap_LAN_GetServerCount((int32_t)ui_netSource);
        ui_nextDisplayRefresh = ui_displayContextStorage.context.realTime + UI_SERVER_LIST_RETRY_MILLISECONDS;
        return;
    }

    for (server = 0; server < serverCount; ++server) {
        int32_t clients;
        int32_t ping;
        qboolean filtered = qfalse;

        if (!trap_LAN_ServerIsDirty((int32_t)ui_netSource, server)) {
            continue;
        }

        ping = trap_LAN_GetServerPing((int32_t)ui_netSource, server);
        if (ping <= 0 && ui_netSource != LAN_SERVER_SOURCE_FAVORITES) {
            continue;
        }

        ui_compat_lan_get_server_info((int32_t)ui_netSource, server, serverInfo, sizeof(serverInfo));
        clients = coduo_crt_atoi(Info_ValueForKey(serverInfo, "clients"));
        ui_numPlayers += clients;

        if (Q_stricmpn(Info_ValueForKey(serverInfo, "addr"), "000.000.000.000", 15) == 0) {
            filtered = qtrue;
        } else if (!ui_browserShowEmpty.integer && clients == 0) {
            filtered = qtrue;
        } else if (!ui_browserShowFull.integer && clients == coduo_crt_atoi(Info_ValueForKey(serverInfo, "sv_maxclients"))) {
            filtered = qtrue;
        } else if (!ui_browserShowPassword.integer && coduo_crt_atoi(Info_ValueForKey(serverInfo, "pswrd")) != 0) {
            filtered = qtrue;
        } else if (!ui_browserShowNoPassword.integer && coduo_crt_atoi(Info_ValueForKey(serverInfo, "pswrd")) == 0) {
            filtered = qtrue;
        } else if (ui_browserShowPure.integer && coduo_crt_atoi(Info_ValueForKey(serverInfo, "pure")) == 0) {
            filtered = qtrue;
        } else if (ui_browserShowDedicated.integer) {
            int32_t hardware = coduo_crt_atoi(Info_ValueForKey(serverInfo, "hw"));
            if (hardware != 1 && hardware != 2 && hardware != 3) {
                filtered = qtrue;
            }
        }

        if (!filtered && ui_browserMod.integer >= 0 && coduo_crt_atoi(Info_ValueForKey(serverInfo, "mod")) != ui_browserMod.integer) {
            filtered = qtrue;
        }
        if (!filtered && ui_browserFriendlyfire.integer >= 0 &&
            coduo_crt_atoi(Info_ValueForKey(serverInfo, "ff")) != ui_browserFriendlyfire.integer) {
            filtered = qtrue;
        }
        if (!filtered && ui_browserKillcam.integer >= 0 &&
            coduo_crt_atoi(Info_ValueForKey(serverInfo, "kc")) != ui_browserKillcam.integer) {
            filtered = qtrue;
        }
        if (!filtered && ui_browserShowPunkBuster.integer >= 0 &&
            coduo_crt_atoi(Info_ValueForKey(serverInfo, "pb")) != ui_browserShowPunkBuster.integer) {
            filtered = qtrue;
        }
        if (!filtered && ui_joinGameTypes[ui_joinGameType].displayName[0] &&
            Q_stricmp(ui_joinGameTypes[ui_joinGameType].gameType, Info_ValueForKey(serverInfo, "gametype")) != 0) {
            filtered = qtrue;
        }
        if (!filtered && ui_serverFilterType > 0 &&
            Q_stricmp(ui_serverFilters[ui_serverFilterType].gameName, Info_ValueForKey(serverInfo, "game")) != 0) {
            filtered = qtrue;
        }
        if (!filtered && ui_browserShowJeeps.integer > 0 &&
            coduo_crt_atoi(Info_ValueForKey(serverInfo, "jps")) != ui_browserShowJeeps.integer) {
            filtered = qtrue;
        }
        if (!filtered && ui_browserShowTanks.integer > 0 &&
            coduo_crt_atoi(Info_ValueForKey(serverInfo, "tnk")) != ui_browserShowTanks.integer) {
            filtered = qtrue;
        }

        if (filtered) {
            trap_LAN_MarkServerDirty((int32_t)ui_netSource, server, qfalse);
            continue;
        }

        if (ui_netSource == LAN_SERVER_SOURCE_FAVORITES) {
            UI_RemoveServerFromDisplayList(server);
        }
        UI_InsertServerIntoDisplayList(server);
        if (ping > 0) {
            trap_LAN_MarkServerDirty((int32_t)ui_netSource, server, qfalse);
            ++ui_filteredServerCount;
        }
    }

    ui_serverRefreshTime = ui_displayContextStorage.context.realTime;
}
