#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x4000d790..0x4000d7df
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d790_4000d7df.mcode
// Exact same-module PPC symbol: UI_BinaryServerInsertion.
void UI_BinaryServerInsertion(int32_t index, int32_t server)
{
    int32_t shiftIndex;

    if (index < 0 || index > ui_displayServerCount) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_displayServerCount >= UI_MAX_DISPLAY_SERVERS) {
        return;
    }

    if (index <= ui_currentServer) {
        ++ui_currentServer;
    }

    ++ui_displayServerCount;
    for (shiftIndex = ui_displayServerCount - 1; shiftIndex > index;
         --shiftIndex) {
        ui_displayServers[shiftIndex] = ui_displayServers[shiftIndex - 1];
    }
    ui_displayServers[index] = server;
}

// Source: uo_ui_mp_x86.dll 0x4000d7e0..0x4000d82a
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d7e0_4000d82a.mcode
// Exact same-module PPC symbol: UI_RemoveServerFromDisplayList.
void UI_RemoveServerFromDisplayList(int32_t server)
{
    int32_t index;

    for (index = 0; index < ui_displayServerCount; ++index) {
        if (ui_displayServers[index] == server) {
            --ui_displayServerCount;
            for (; index < ui_displayServerCount; ++index) {
                ui_displayServers[index] = ui_displayServers[index + 1];
            }
            return;
        }
    }
}

// Source: uo_ui_mp_x86.dll 0x4000d830..0x4000d8a6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d830_4000d8a6.mcode
// Exact same-module PPC symbol: UI_InsertServerIntoDisplayList.
void UI_InsertServerIntoDisplayList(int32_t server)
{
    int32_t range = ui_displayServerCount;
    int32_t offset = 0;
    int32_t comparison = 0;

    while (range > 0) {
        int32_t half = range / 2;
        int32_t middle = offset + half;

        comparison = trap_LAN_CompareServers(
            ui_netSource, ui_serverSortKey, ui_serverSortDirection, server,
            ui_displayServers[middle]);
        if (comparison == 0) {
            UI_BinaryServerInsertion(middle, server);
            return;
        }
        if (comparison > 0) {
            offset = middle;
        }
        range -= half;
        if (half <= 0) {
            break;
        }
    }

    if (comparison > 0) {
        ++offset;
    }
    UI_BinaryServerInsertion(offset, server);
}
