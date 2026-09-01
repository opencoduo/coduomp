#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x4000eb00..0x4000eb42
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000eb00_4000eb42.mcode
// Exact same-module PPC symbol: UI_SelectedMap.
const char *UI_SelectedMap(int32_t index, int32_t *actual)
{
    int32_t mapCount;
    int32_t mapIndex;
    int32_t activeIndex = 0;

    *actual = 0;
    mapCount = ui_mapCount;
    for (mapIndex = 0; mapIndex < mapCount; ++mapIndex) {
        if (!ui_maps[mapIndex].active) {
            continue;
        }
        if (activeIndex == index) {
            *actual = mapIndex;
            return ui_maps[mapIndex].displayName;
        }
        ++activeIndex;
    }

    return "";
}
