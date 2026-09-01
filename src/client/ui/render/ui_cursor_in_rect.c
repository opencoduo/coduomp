#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40007e30..0x40007e62
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007e30_40007e62.mcode
// Role name: UI_CursorInRect; global cursor coordinates and inclusive rectangle
// comparisons establish the behavior.
qboolean UI_CursorInRect(int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (ui_displayContextStorage.context.cursorx < x ||
        ui_displayContextStorage.context.cursory < y) {
        return qfalse;
    }
    if (ui_displayContextStorage.context.cursorx > x + width ||
        ui_displayContextStorage.context.cursory > y + height) {
        return qfalse;
    }
    return qtrue;
}
