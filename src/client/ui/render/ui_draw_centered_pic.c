#include "../module/ui_functions.h"

enum {
    UI_VIRTUAL_SCREEN_WIDTH = 640,
    UI_VIRTUAL_SCREEN_HEIGHT = 480
};

// Source: uo_ui_mp_x86.dll 0x40008a30..0x40008a84
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008a30_40008a84.mcode
// Role name: UI_DrawCenteredPic; exact virtual-screen constants, signed midpoint
// arithmetic, integer-to-float conversions, and UI_DrawHandlePic call prove role.
void UI_DrawCenteredPic(qhandle_t shader, int32_t width, int32_t height)
{
    int32_t x = (UI_VIRTUAL_SCREEN_WIDTH - width) / 2;
    int32_t y = (UI_VIRTUAL_SCREEN_HEIGHT - height) / 2;

    UI_DrawHandlePic((float)x, (float)y, (float)width, (float)height, shader);
}
