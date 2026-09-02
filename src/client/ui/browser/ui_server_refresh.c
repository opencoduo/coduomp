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

/* NOT_FROM_ORIGINAL_SOURCE: rebuild the compatibility server list without
 * resetting its listbox state. Clearing ui_currentServer during insertion also
 * prevents UI_BinaryServerInsertion from advancing the selection to the end of
 * the newly sorted list. Restore the same display row, clamped only when the
 * filtered list became shorter, so reordering does not move the viewport. */
static void ui_compat_rebuild_server_list_preserving_selection(void)
{
    int32_t savedDisplayRow = ui_currentServer;

    ui_currentServer = UI_NO_CURRENT_SERVER;
    UI_BuildServerDisplayList(UI_SERVER_REBUILD_COUNT_CHANGED);

    if (savedDisplayRow >= 0 && ui_displayServerCount > 0) {
        if (savedDisplayRow >= ui_displayServerCount) {
            savedDisplayRow = ui_displayServerCount - 1;
        }
        UI_FeederSelection((float)UI_SERVER_LIST_FEEDER, savedDisplayRow);
    }
}

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
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): automatic status
         * queries may replace aggregate counts after their rows were first
         * inserted. Rebuild once so display order, totals, and filters all
         * consume the final count with 999-ping entries removed. */
        ui_compat_rebuild_server_list_preserving_selection();
        UI_StopServerRefresh();
    }
    UI_BuildServerDisplayList(UI_SERVER_REBUILD_INCREMENTAL);
}
