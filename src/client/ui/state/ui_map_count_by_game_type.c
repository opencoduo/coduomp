#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000d640..0x4000d68b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d640_4000d68b.mcode
// Exact same-module PPC symbol: UI_MapCountByGameType.
int32_t UI_MapCountByGameType(void)
{
    int32_t mapIndex;
    int32_t count = 0;
    uint32_t gameTypeBit = 1u << ((uint32_t)ui_netGameType & 31u);

    for (mapIndex = 0; mapIndex < ui_mapCount; ++mapIndex) {
        uiMapInfo_t *map = &ui_maps[mapIndex];

        map->active = qfalse;
        if ((map->typeBits & gameTypeBit) != 0) {
            ++count;
            map->active = qtrue;
        }
    }

    return count;
}
