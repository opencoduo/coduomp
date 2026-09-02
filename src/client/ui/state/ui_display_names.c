#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x4000a100..0x4000a14f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a100_4000a14f.mcode
// Same-module PPC symbol: UI_GetMapDisplayName.
const char *UI_GetMapDisplayName(const char *mapName)
{
    enum {
        UI_DISPLAY_NAME_COMPARE_LIMIT = 99999
    };

    for (int32_t index = 0; index < ui_mapCount; ++index) {
        const char *candidate = ui_maps[index].mapName;

        if (mapName != NULL && candidate != NULL && Q_stricmpn(candidate, mapName, UI_DISPLAY_NAME_COMPARE_LIMIT) == 0) {
            return ui_maps[index].displayName;
        }
    }

    return mapName;
}

// Source: uo_ui_mp_x86.dll 0x4000a150..0x4000a193
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a150_4000a193.mcode
// Same-module PPC symbol: UI_GetGameTypeDisplayName.
const char *UI_GetGameTypeDisplayName(const char *gameType)
{
    enum {
        UI_DISPLAY_NAME_COMPARE_LIMIT = 99999
    };

    for (int32_t index = 0; index < ui_gameTypeCount; ++index) {
        const char *candidate = ui_gameTypes[index].gameType;

        if (gameType != NULL && candidate != NULL && Q_stricmpn(candidate, gameType, UI_DISPLAY_NAME_COMPARE_LIMIT) == 0) {
            return ui_gameTypes[index].displayName;
        }
    }

    return gameType;
}
