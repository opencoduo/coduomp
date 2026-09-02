#include "ui_functions.h"
#include "ui_globals.h"

#include <stddef.h>

enum {
    UI_VIRTUAL_WIDTH = 640,
    UI_VIRTUAL_HEIGHT = 480
};

// Source: uo_ui_mp_x86.dll 0x4000fec0..0x4000ff21
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000fec0_4000ff21.mcode
// Same-module PPC symbol/call graph: _UI_MouseEvent.
void UI_MouseEvent(int32_t deltaX, int32_t deltaY)
{
    ui_displayContextStorage.context.cursorx += deltaX;
    if (ui_displayContextStorage.context.cursorx < 0) {
        ui_displayContextStorage.context.cursorx = 0;
    } else if (ui_displayContextStorage.context.cursorx > UI_VIRTUAL_WIDTH) {
        ui_displayContextStorage.context.cursorx = UI_VIRTUAL_WIDTH;
    }

    ui_displayContextStorage.context.cursory += deltaY;
    if (ui_displayContextStorage.context.cursory < 0) {
        ui_displayContextStorage.context.cursory = 0;
    } else if (ui_displayContextStorage.context.cursory > UI_VIRTUAL_HEIGHT) {
        ui_displayContextStorage.context.cursory = UI_VIRTUAL_HEIGHT;
    }

    if (Menu_Count() > 0) {
        Display_MouseMove(NULL, ui_displayContextStorage.context.cursorx,
                          ui_displayContextStorage.context.cursory);
    }
}
