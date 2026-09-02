#include "../module/ui_functions.h"

enum {
    UI_SERVER_STATUS_FEEDER = 13,
    UI_SERVER_STATUS_RETRY_MILLISECONDS = 500
};

// Source: uo_ui_mp_x86.dll 0x4000e8f0..0x4000e9df
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000e8f0_4000e9df.mcode
// Exact same-module PPC symbol: UI_BuildServerStatus.
void UI_BuildServerStatus(qboolean force)
{
    int32_t serverCount;

    if (ui_findPlayerNextRefresh != 0) {
        return;
    }
    if (!force) {
        if (ui_serverStatusNextRefresh == 0) {
            return;
        }
        if (ui_serverStatusNextRefresh > ui_displayContextStorage.context.realTime) {
            return;
        }
    } else {
        Menu_SetFeederSelection(NULL, NULL, UI_SERVER_STATUS_FEEDER, 0);
        ui_serverStatusInfo.numLines = 0;
        trap_LAN_ServerStatus(NULL, NULL, 0);
    }

    serverCount = trap_LAN_GetServerCount((int32_t)ui_netSource);
    if (serverCount != ui_serverCount) {
        ui_serverCount = serverCount;
        if (ui_displayServerCount != 0) {
            ui_currentServer = -1;
            UI_BuildServerDisplayList(qtrue);
        }
    }

    if (ui_currentServer < 0 || ui_currentServer > ui_displayServerCount || ui_displayServerCount == 0) {
        return;
    }
    if (UI_GetServerStatusInfo(ui_serverStatusAddress, &ui_serverStatusInfo)) {
        ui_serverStatusNextRefresh = 0;
        trap_LAN_ServerStatus(ui_serverStatusAddress, NULL, 0);
        return;
    }
    ui_serverStatusNextRefresh =
        (int32_t)((uint32_t)ui_displayContextStorage.context.realTime + (uint32_t)UI_SERVER_STATUS_RETRY_MILLISECONDS);
}
