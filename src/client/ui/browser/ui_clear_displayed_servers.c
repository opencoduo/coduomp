#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x4000d8b0..0x4000d8d3
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d8b0_4000d8d3.mcode
// The same-module PPC symbol and unique displayed/player-count reset identify
// this function as UI_ClearDisplayedServers.
void UI_ClearDisplayedServers(void)
{
    ui_displayServerCount = 0;
    ui_numPlayers = 0;
    ui_serverCount = trap_LAN_GetServerCount(ui_netSource);
}
