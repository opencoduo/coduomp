#include "../module/ui_functions.h"

enum {
    UI_MAP_SHADER_UNREGISTERED = -1
};

// Source: uo_ui_mp_x86.dll 0x40009d90..0x40009e59
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009d90_40009e59.mcode
// Exact same-module PPC symbol: UI_DrawMapPreview.
void UI_DrawMapPreview(const rectDef_t *rect, qboolean netMap)
{
    int32_t mapIndex = netMap ? ui_currentNetMap : ui_currentMap;
    uiMapInfo_t *map;
    qhandle_t shader;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_mapCount <= 0) {
        shader = trap_R_RegisterShaderNoMip("menu/art/unknownmap",
                                           R_IMAGE_TRACK_UI);
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, shader);
        return;
    }

    if (mapIndex < 0 || mapIndex >= ui_mapCount) {
        if (netMap) {
            ui_currentNetMap = 0;
            trap_Cvar_Set("ui_currentNetMap", "0");
        } else {
            ui_currentMap = 0;
            trap_Cvar_Set("ui_currentMap", "0");
        }
        mapIndex = 0;
    }

    map = &ui_maps[mapIndex];
    if (map->imageShader == UI_MAP_SHADER_UNREGISTERED) {
        map->imageShader = trap_R_RegisterShaderNoMip(
            map->imageName, R_IMAGE_TRACK_UI);
    }

    shader = map->imageShader;
    if (shader <= 0) {
        shader = trap_R_RegisterShaderNoMip("menu/art/unknownmap",
                                       R_IMAGE_TRACK_UI);
    }
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, shader);
}
