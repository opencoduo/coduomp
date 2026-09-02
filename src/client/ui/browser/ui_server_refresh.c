#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_SERVER_REFRESH_RETRY_MILLISECONDS = 1000,
    UI_SERVER_REBUILD_INCREMENTAL = 0,
    UI_SERVER_REBUILD_COUNT_CHANGED = 1,
    UI_SERVER_REBUILD_FINAL = 2,
    UI_SERVER_LIST_FEEDER = 2,
    UI_NO_CURRENT_SERVER = -1
};


// Source: uo_ui_mp_x86.dll 0x40011480..0x40011562
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40011480_40011562.mcode
void UI_DoServerRefresh(void)
{
    qboolean waiting = qfalse;
    int32_t serverCount;

    if (!ui_serverRefreshActive) {
        return;
    }

    if (ui_netSource != LAN_SERVER_SOURCE_FAVORITES) {
        if (ui_netSource == LAN_SERVER_SOURCE_LOCAL) {
            if (trap_LAN_GetServerCount(LAN_SERVER_SOURCE_LOCAL) == 0) {
                waiting = qtrue;
            }
        } else if (trap_LAN_WaitServerResponse(ui_netSource)) {
            waiting = qtrue;
        }
    }

    if (ui_displayContextStorage.context.realTime < ui_serverRefreshTime && waiting) {
        return;
    }

    serverCount = trap_LAN_GetServerCount(ui_netSource);
    if (serverCount != ui_serverCount) {
        ui_serverCount = serverCount;
        if (ui_displayServerCount != 0) {
            ui_currentServer = UI_NO_CURRENT_SERVER;
            UI_BuildServerDisplayList(UI_SERVER_REBUILD_COUNT_CHANGED);
        }
    }

    if (trap_LAN_UpdateDirtyPings(ui_netSource)) {
        ui_serverRefreshTime = ui_displayContextStorage.context.realTime + UI_SERVER_REFRESH_RETRY_MILLISECONDS;
        UI_BuildServerDisplayList(UI_SERVER_REBUILD_INCREMENTAL);
        return;
    }

    if (!waiting) {
        UI_BuildServerDisplayList(UI_SERVER_REBUILD_FINAL);
        UI_StopServerRefresh();
    }
    UI_BuildServerDisplayList(UI_SERVER_REBUILD_INCREMENTAL);
}
