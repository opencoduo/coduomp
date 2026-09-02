#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

#include <string.h>

enum {
    UI_SERVERINFO_CONFIG_STRING = 0,
    UI_SERVERINFO_BUFFER_SIZE = 1024,
    UI_MAP_FEEDER = 4
};

// Source: uo_ui_mp_x86.dll 0x4000d690..0x4000d78b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d690_4000d78b.mcode
// Exact same-module PPC symbol: UI_SelectCurrentMap.
void UI_SelectCurrentMap(void)
{
    enum { UI_MAP_NAME_COMPARE_LIMIT = 99999 };
    uiClientState_t state;
    char serverInfo[UI_SERVERINFO_BUFFER_SIZE];
    char mapName[UI_SERVERINFO_BUFFER_SIZE];
    int32_t mapCount;
    int32_t mapIndex;
    int32_t activeIndex = 0;

    trap_GetClientState(&state);
    if (state.connState != CA_ACTIVE) {
        return;
    }

    serverInfo[0] = '\0';
    if (!trap_GetConfigString(UI_SERVERINFO_CONFIG_STRING, serverInfo,
                              (int32_t)sizeof(serverInfo))) {
        return;
    }

    strcpy(mapName, Info_ValueForKey(serverInfo, "mapname"));
    mapCount = ui_mapCount;
    for (mapIndex = 0; mapIndex < mapCount; ++mapIndex) {
        uiMapInfo_t *map = &ui_maps[mapIndex];

        if (!map->active) {
            continue;
        }
        if (map->displayName != NULL &&
            Q_stricmpn(map->displayName, mapName,
                       UI_MAP_NAME_COMPARE_LIMIT) == 0) {
            Menu_SetFeederSelection(NULL, NULL, UI_MAP_FEEDER, activeIndex);
            return;
        }
        ++activeIndex;
    }
}
