#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40009f50..0x40009f9a
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009f50_40009f9a.mcode
// Exact same-module PPC symbol: UI_DrawNetMapPreview.
void UI_DrawNetMapPreview(const rectDef_t *rect)
{
    qhandle_t shader = ui_serverMapPreviewShader;

    if (shader <= 0) {
        shader = trap_R_RegisterShaderNoMip("menu/art/unknownmap", R_IMAGE_TRACK_UI);
    }
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, shader);
}
