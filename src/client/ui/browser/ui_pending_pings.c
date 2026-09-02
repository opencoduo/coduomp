#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_PENDING_PING_RETRY_MILLISECONDS = 1000
};

// Source: uo_ui_mp_x86.dll 0x4000eb50..0x4000eb7e
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000eb50_4000eb7e.mcode
// The same-module PPC symbol and unique ResetPings/state-write sequence identify
// this function as UI_UpdatePendingPings.
void UI_UpdatePendingPings(void)
{
    trap_LAN_ResetPings(ui_netSource);
    ui_serverRefreshActive = qtrue;
    ui_serverRefreshTime =
        ui_displayContextStorage.context.realTime +
        UI_PENDING_PING_RETRY_MILLISECONDS;
}
