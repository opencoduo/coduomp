#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40007a60..0x40007b26
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007a60_40007b26.mcode
// Same-module PPC role: UI_DrawHandlePic.
void UI_DrawHandlePic(float x, float y, float width, float height,
                      qhandle_t shader)
{
    float s0;
    float s1;
    float t0;
    float t1;

    if (width < 0.0f) {
        width = -width;
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }
    if (height < 0.0f) {
        height = -height;
        t0 = 1.0f;
        t1 = 0.0f;
    } else {
        t0 = 0.0f;
        t1 = 1.0f;
    }

    trap_R_DrawStretchPic(x * ui_displayContextStorage.context.xscale,
                          y * ui_displayContextStorage.context.yscale,
                          width * ui_displayContextStorage.context.xscale,
                          height * ui_displayContextStorage.context.yscale,
                          s0, t0, s1, t1, shader);
}
