#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40009fa0..0x4000a070
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009fa0_4000a070.mcode
// Exact same-module PPC symbol: UI_DrawNetMapCinematic.
void UI_DrawNetMapCinematic(const rectDef_t *rect)
{
    qhandle_t shader;

    if (ui_currentNetMap < 0 || ui_currentNetMap > ui_mapCount) {
        ui_currentNetMap = 0;
        trap_Cvar_Set("ui_currentNetMap", "0");
    }

    if (ui_serverMapCinematic >= 0) {
        trap_CIN_RunCinematic(ui_serverMapCinematic);
        trap_CIN_SetExtents(ui_serverMapCinematic, (int32_t)rect->x,
                            (int32_t)rect->y, (int32_t)rect->w,
                            (int32_t)rect->h);
        trap_CIN_DrawCinematic(ui_serverMapCinematic);
        return;
    }

    shader = ui_serverMapPreviewShader;
    if (shader <= 0) {
        shader = trap_R_RegisterShaderNoMip("menu/art/unknownmap",
                                       R_IMAGE_TRACK_UI);
    }
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, shader);
}
