#include <string.h>

#include "../module/ui_functions.h"

enum {
    UI_FIND_PLAYER_RESULT_LIMIT = UI_MAX_FOUND_PLAYER_SERVERS - 1,
    UI_FIND_PLAYER_TEXT_SIZE = UI_FOUND_PLAYER_SERVER_TEXT_SIZE,
    UI_FIND_PLAYER_NAME_COPY_SIZE = 33,
    UI_FIND_PLAYER_NAME_LAST_BYTE = 33,
    UI_FIND_PLAYER_RETRY_MILLISECONDS = 25,
    UI_FIND_PLAYER_MINIMUM_RESEND = 50,
    UI_FIND_PLAYER_RESEND_BIAS = 10,
    UI_FIND_PLAYER_FEEDER = 14,
    UI_SERVER_INFO_SIZE = 1024
};

// Source: uo_ui_mp_x86.dll 0x4000e3e0..0x4000e8ed
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000e3e0_4000e8ed.mcode
// Exact same-module PPC symbol: UI_BuildFindPlayerList.
void UI_BuildFindPlayerList(qboolean force)
{
    int32_t serverCount;
    int32_t slotIndex;

    if (!force) {
        if (ui_findPlayerNextRefresh == 0) {
            return;
        }
        if (ui_findPlayerNextRefresh > ui_displayContextStorage.context.realTime) {
            return;
        }
    } else {
        int32_t resendTime;

        ui_findPlayerServerIndex = 0;
        memset(ui_pendingServerStatus, 0, sizeof(ui_pendingServerStatus));
        ui_foundPlayerServerCount = 0;
        ui_foundPlayerServerIndex = 0;
        trap_Cvar_VariableStringBuffer("ui_findPlayer", ui_findPlayerName, sizeof(ui_findPlayerName));
        Q_CleanStr(ui_findPlayerName);
        if (ui_findPlayerName[0] == '\0') {
            ui_findPlayerNextRefresh = 0;
            return;
        }

        resendTime = ui_serverStatusTimeOut.integer / 2 - UI_FIND_PLAYER_RESEND_BIAS;
        if (resendTime < UI_FIND_PLAYER_MINIMUM_RESEND) {
            resendTime = UI_FIND_PLAYER_MINIMUM_RESEND;
        }
        trap_Cvar_Set("cl_serverStatusResendTime", va("%d", resendTime));
        trap_LAN_ServerStatus(NULL, NULL, 0);
        ui_foundPlayerServerCount = 1;
        /* Retail deliberately reserves slot 0: results are written at [count]
         * with count starting at 1 and all readers use [index + 1]. This
         * status text therefore goes to slot 1 (0x4023983c). */
        Com_sprintf(ui_foundPlayerServerNames[1], UI_FIND_PLAYER_TEXT_SIZE, "searching %d...", ui_findPlayerServerIndex);
        ui_findPlayerCompletedCount = 0;
        ++ui_findPlayerRequestCount;
    }

    serverCount = trap_LAN_GetServerCount((int32_t)ui_netSource);
    if (serverCount != ui_serverCount) {
        ui_serverCount = serverCount;
        if (ui_displayServerCount != 0) {
            ui_currentServer = -1;
            UI_BuildServerDisplayList(qtrue);
        }
    }

    for (slotIndex = 0; slotIndex < UI_MAX_FOUND_PLAYER_SERVERS; ++slotIndex) {
        uiPendingServerStatus_t *pending = &ui_pendingServerStatus[slotIndex];

        if (pending->active) {
            uiServerStatusInfo_t statusInfo;

            if (UI_GetServerStatusInfo(pending->address, &statusInfo)) {
                int32_t lineIndex;

                ++ui_findPlayerCompletedCount;
                for (lineIndex = 0; lineIndex < statusInfo.numLines; ++lineIndex) {
                    const char *ping = statusInfo.lines[lineIndex].column[2];
                    const char *name;
                    char cleanName[UI_FIND_PLAYER_NAME_COPY_SIZE + 1];

                    if (ping == NULL || ping[0] == '\0') {
                        continue;
                    }
                    name = statusInfo.lines[lineIndex].column[3];
                    strncpy(cleanName, name, UI_FIND_PLAYER_NAME_COPY_SIZE);
                    cleanName[UI_FIND_PLAYER_NAME_LAST_BYTE] = '\0';
                    Q_CleanStr(cleanName);
                    if (stristr(cleanName, ui_findPlayerName) == NULL) {
                        continue;
                    }

                    if (ui_foundPlayerServerCount < UI_FIND_PLAYER_RESULT_LIMIT) {
                        int32_t result = ui_foundPlayerServerCount;
                        strncpy(ui_foundPlayerServerAddresses[result], pending->address, UI_FOUND_PLAYER_SERVER_TEXT_SIZE - 1);
                        ui_foundPlayerServerAddresses[result][UI_FOUND_PLAYER_SERVER_TEXT_SIZE - 1] = '\0';
                        strncpy(ui_foundPlayerServerNames[result], pending->name, UI_FOUND_PLAYER_SERVER_TEXT_SIZE - 1);
                        ui_foundPlayerServerNames[result][UI_FOUND_PLAYER_SERVER_TEXT_SIZE - 1] = '\0';
                        ++ui_foundPlayerServerCount;
                    } else {
                        ui_findPlayerServerIndex = ui_displayServerCount;
                    }
                }

                Com_sprintf(ui_foundPlayerServerNames[ui_foundPlayerServerCount], UI_FIND_PLAYER_TEXT_SIZE, "searching %d/%d...",
                            ui_findPlayerServerIndex, ui_findPlayerCompletedCount);
                pending->active = qfalse;
            }
        }

        if (pending->active && pending->startTime >= (int32_t)((uint32_t)ui_displayContextStorage.context.realTime -
                                                               (uint32_t)ui_serverStatusTimeOut.integer)) {
            continue;
        }
        if (pending->active) {
            ++ui_findPlayerRequestCount;
        }

        trap_LAN_ServerStatus(pending->address, NULL, 0);
        pending->active = qfalse;

        serverCount = trap_LAN_GetServerCount((int32_t)ui_netSource);
        if (serverCount != ui_serverCount) {
            ui_serverCount = serverCount;
            if (ui_displayServerCount != 0) {
                ui_currentServer = -1;
                UI_BuildServerDisplayList(qtrue);
            }
        }
        if (ui_findPlayerServerIndex >= ui_displayServerCount) {
            continue;
        }

        pending->startTime = ui_displayContextStorage.context.realTime;
        ui_compat_lan_get_server_address((int32_t)ui_netSource, ui_displayServers[ui_findPlayerServerIndex], pending->address,
                                         sizeof(pending->address));
        {
            char serverInfo[UI_SERVER_INFO_SIZE];
            const char *hostname;

            ui_compat_lan_get_server_info((int32_t)ui_netSource, ui_displayServers[ui_findPlayerServerIndex], serverInfo,
                                          sizeof(serverInfo));
            hostname = Info_ValueForKey(serverInfo, "hostname");
            strncpy(pending->name, hostname, sizeof(pending->name) - 1);
            pending->name[sizeof(pending->name) - 1] = '\0';
        }
        pending->active = qtrue;
        Com_sprintf(ui_foundPlayerServerNames[ui_foundPlayerServerCount], UI_FIND_PLAYER_TEXT_SIZE, "searching %d/%d...",
                    ui_findPlayerServerIndex + 1, ui_findPlayerCompletedCount);
        ++ui_findPlayerServerIndex;
    }

    for (slotIndex = 0; slotIndex < UI_MAX_FOUND_PLAYER_SERVERS; ++slotIndex) {
        if (ui_pendingServerStatus[slotIndex].active) {
            ui_findPlayerNextRefresh =
                (int32_t)((uint32_t)ui_displayContextStorage.context.realTime + (uint32_t)UI_FIND_PLAYER_RETRY_MILLISECONDS);
            return;
        }
    }

    if (ui_foundPlayerServerCount == 0) {
        Com_sprintf(ui_foundPlayerServerNames[0], UI_FIND_PLAYER_TEXT_SIZE, "no servers found");
    } else {
        int32_t foundCount = ui_foundPlayerServerCount - 1;
        const char *plural = ui_foundPlayerServerCount == 2 ? "" : "s";

        /* Final status goes to [count] (count<<6 + 0x402397fc), one slot
         * past the last result, matching the reserved-slot-0 read scheme. */
        Com_sprintf(ui_foundPlayerServerNames[ui_foundPlayerServerCount], UI_FIND_PLAYER_TEXT_SIZE, "%d server%s found with player %s",
                    foundCount, plural, ui_findPlayerName);
    }
    ui_findPlayerNextRefresh = 0;
    UI_FeederSelection(UI_FIND_PLAYER_FEEDER, ui_foundPlayerServerIndex);
}
