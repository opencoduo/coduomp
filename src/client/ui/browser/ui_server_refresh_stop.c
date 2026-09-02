#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x40011420..0x40011475
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40011420_40011475.mcode
void UI_StopServerRefresh(void)
{
    int32_t filteredServerCount;

    if (!ui_serverRefreshActive) {
        return;
    }

    ui_serverRefreshActive = qfalse;
    Com_Printf("%d servers listed in browser with %d players.\n", ui_displayServerCount, ui_numPlayers);

    filteredServerCount = trap_LAN_GetServerCount(ui_netSource) - ui_displayServerCount;
    if (filteredServerCount > 0) {
        Com_Printf("%d servers not listed (filtered out by game browser "
                   "settings)\n",
                   filteredServerCount);
    }
}
