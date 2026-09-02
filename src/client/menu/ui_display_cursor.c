// Sources: uo_cgame_mp_x86.dll 0x3005b250..0x3005b2fe and
//          uo_ui_mp_x86.dll    0x4001cf70..0x4001d01e
//
// Display_CursorType — select the normal or resize cursor for the registered
// menu set.  The size-only mcode name is rejected: this function touches only
// menu rectangles and cursor coordinates.

#include "ui_runtime.h"

#include "ui_menu_globals.h"

uiCursorType_t Display_CursorType(int32_t cursorX, int32_t cursorY)
{
    const int32_t count = menuCount;
    const float pointY = (float)cursorY; /* 0x3005b264: y spills first. */
    const float pointX = (float)cursorX;

    for (int32_t i = 0; i < count; ++i) {
        /* Asymmetric spill: resizeTop is stored to a float slot (0x3005b291 FSTP)
         * and reloaded, but resizeLeft stays 80-bit in an x87 register across both
         * x comparisons (0x3005b299 FCOMP against cursorX, 0x3005b2a8 FADD ST0,ST1
         * for resizeLeft+7, 0x3005b2ae FCOMPP) and is never stored -- so keep it
         * wide, or the field round would flip a boundary hit-test. */
        const long double resizeLeft = Menus[i].window.rect.x - 3.0f;
        const float resizeTop = Menus[i].window.rect.y - 3.0f;

        /* 0x3005b280..0x3005b2db uses inclusive x87 comparisons against a
         * 7-by-7 square whose upper edges are left/top + 7.0f. */
        if (pointX >= resizeLeft &&
            pointX <= resizeLeft + 7.0f &&
            pointY >= resizeTop &&
            pointY <= resizeTop + 7.0f) {
            return UI_CURSOR_SIZER;
        }
    }

    return UI_CURSOR_ARROW;
}
