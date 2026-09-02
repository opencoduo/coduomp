#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

#include <string.h>

enum {
    UI_SERVER_REFRESH_SHORT_MILLISECONDS = 1000,
    UI_SERVER_REFRESH_LONG_MILLISECONDS = 5000,
    UI_ALL_SERVERS = -1,
    UI_DEFAULT_PROTOCOL = 22,
    UI_CALENDAR_YEAR_BASE = 1900
};

/* Source: uo_ui_mp_x86.dll data 0x4003fddc..0x4003fe0b.
 * PE_RELOCATION_VALUES_VERIFIED: all 12 pointers target the listed month
 * reference strings in source order. */
static const char *const ui_monthReferences[12] = {"EXE_MONTH_ABV_JANUARY", "EXE_MONTH_ABV_FEBRUARY", "EXE_MONTH_ABV_MARCH",
                                                   "EXE_MONTH_ABV_APRIL",   "EXE_MONTH_ABV_MAY",      "EXE_MONTH_ABV_JUN",
                                                   "EXE_MONTH_ABV_JULY",    "EXE_MONTH_ABV_AUGUST",   "EXE_MONTH_ABV_SEPTEMBER",
                                                   "EXE_MONTH_ABV_OCTOBER", "EXE_MONTH_ABV_NOVEMBER", "EXE_MONTH_ABV_DECEMBER"};

// Source: uo_ui_mp_x86.dll 0x40011570..0x4001171d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40011570_4001171d.mcode
// Exact same-module PPC symbol: UI_StartServerRefresh.
void UI_StartServerRefresh(qboolean full)
{
    qtime_t time;
    const char *command;
    const char *refreshDate;
    const char *refreshCvar;

    trap_RealTime(&time);
    refreshDate = va("%s %i, %i   %i:%02i", UI_SafeTranslateString(ui_monthReferences[time.tm_mon]), time.tm_mday,
                     time.tm_year + UI_CALENDAR_YEAR_BASE, time.tm_hour, time.tm_min);
    refreshCvar = va("ui_lastServerRefresh_%i", ui_netSource);
    trap_Cvar_Set(refreshCvar, refreshDate);

    if (!full) {
        trap_LAN_ResetPings(ui_netSource);
        ui_serverRefreshActive = qtrue;
        ui_serverRefreshTime = ui_displayContextStorage.context.realTime + UI_SERVER_REFRESH_SHORT_MILLISECONDS;
        return;
    }

    ui_serverRefreshActive = qtrue;
    ui_nextDisplayRefresh = ui_displayContextStorage.context.realTime + UI_SERVER_REFRESH_SHORT_MILLISECONDS;
    ui_displayServerCount = 0;
    ui_numPlayers = 0;
    ui_serverCount = trap_LAN_GetServerCount(ui_netSource);
    trap_LAN_MarkServerDirty(ui_netSource, UI_ALL_SERVERS, qtrue);
    trap_LAN_ResetPings(ui_netSource);

    if (ui_netSource == LAN_SERVER_SOURCE_LOCAL) {
        trap_Cmd_ExecuteText(EXEC_NOW, "localservers\n");
        ui_serverRefreshTime = ui_displayContextStorage.context.realTime + UI_SERVER_REFRESH_SHORT_MILLISECONDS;
        return;
    }

    ui_serverRefreshTime = ui_displayContextStorage.context.realTime + UI_SERVER_REFRESH_LONG_MILLISECONDS;
    if (ui_netSource != LAN_SERVER_SOURCE_GLOBAL) {
        return;
    }

    const char *debugProtocol = UI_Cvar_VariableString("debug_protocol");

    if (strlen(debugProtocol) != 0) {
        command = va("globalservers %d %s full empty\n", 0, debugProtocol);
    } else {
        command = va("globalservers %d %d full empty\n", 0, UI_DEFAULT_PROTOCOL);
    }
    trap_Cmd_ExecuteText(EXEC_NOW, command);
}
