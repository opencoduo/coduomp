#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_SERVER_REBUILD_COUNT_CHANGED = 1,
    UI_NO_CURRENT_SERVER = -1
};

// Source: uo_ui_mp_x86.dll 0x4000c170..0x4000c1ae
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000c170_4000c1ae.mcode
// Exact same-module PPC symbol: UI_UpdateDisplayServers.
void UI_UpdateDisplayServers(void)
{
    int32_t serverCount = trap_LAN_GetServerCount(ui_netSource);

    if (serverCount == ui_serverCount) {
        return;
    }

    ui_serverCount = serverCount;
    if (ui_displayServerCount != 0) {
        ui_currentServer = UI_NO_CURRENT_SERVER;
        UI_BuildServerDisplayList(UI_SERVER_REBUILD_COUNT_CHANGED);
    }
}
