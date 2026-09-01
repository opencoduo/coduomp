#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40007a00..0x40007a5f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007a00_40007a5f.mcode
// Role name: UI_DrawNamedPic; shader registration precedes a full-UV scaled
// stretch-picture draw.
void UI_DrawNamedPic(float x, float y, float width, float height,
                     const char *name, int32_t loadMode)
{
    qhandle_t shader = trap_R_RegisterShaderNoMip(name, loadMode);

    trap_R_DrawStretchPic(x * ui_displayContextStorage.context.xscale,
                          y * ui_displayContextStorage.context.yscale,
                          width * ui_displayContextStorage.context.xscale,
                          height * ui_displayContextStorage.context.yscale,
                          0.0f, 0.0f, 1.0f, 1.0f, shader);
}
