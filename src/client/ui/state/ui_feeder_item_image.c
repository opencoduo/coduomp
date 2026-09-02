#include "../module/ui_functions.h"

#define UI_FEEDER_TEAMS 0.0f
#define UI_FEEDER_ACTIVE_MAPS 1.0f
#define UI_FEEDER_ACTIVE_MAPS_ALT 4.0f

enum {
    UI_SHADER_UNREGISTERED = -1
};

// Source: uo_ui_mp_x86.dll 0x4000f060..0x4000f136
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f060_4000f136.mcode
// Exact same-module PPC symbol: UI_FeederItemImage.
qhandle_t UI_FeederItemImage(float feeder, int32_t index)
{
    if (feeder == UI_FEEDER_TEAMS) {
        uiTeamInfo_t *team;

        if (index < 0 || index >= ui_teamCount) {
            return 0;
        }
        team = &ui_teams[index];
        if (team->imageShader == UI_SHADER_UNREGISTERED) {
            team->imageShader = trap_R_RegisterShaderNoMip(team->imageName, R_IMAGE_TRACK_UI);
        }
        return team->imageShader;
    }

    if (feeder == UI_FEEDER_ACTIVE_MAPS || feeder == UI_FEEDER_ACTIVE_MAPS_ALT) {
        int32_t mapIndex;
        uiMapInfo_t *map;

        UI_SelectedMap(index, &mapIndex);
        if (mapIndex < 0 || mapIndex >= ui_mapCount) {
            return 0;
        }
        map = &ui_maps[mapIndex];
        if (map->imageShader == UI_SHADER_UNREGISTERED) {
            map->imageShader = trap_R_RegisterShaderNoMip(map->imageName, R_IMAGE_TRACK_UI);
        }
        return map->imageShader;
    }
    return 0;
}
