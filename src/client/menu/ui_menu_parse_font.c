// Source: uo_cgame_mp_x86.dll 0x3005a4f0..0x3005a54c;
//         uo_ui_mp_x86.dll    0x4001c110..0x4001c16c (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3005a4f0_3005a54c.mcode

#include "ui_parse.h"

enum {
    MENU_FONT_POINT_SIZE = 48,
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    MENU_FONT_ONKEY_INDEX = '\\'
};

qboolean MenuParse_font(menuDef_t *menu, int handle)
{
    displayContextDef_t *display;

    if (!PC_String_Parse(handle, &menu->font))
        return qfalse;

    display = DC;
    if (display->textFontRegistered == 0) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const intptr_t fontContext = (intptr_t)menu->onKey[MENU_FONT_ONKEY_INDEX];
        const char *fontName = menu->font;

        display->registerFont(fontName, MENU_FONT_POINT_SIZE, &display->textFont, fontContext);
        /* 0x3005a531 reloads the published display pointer after the callback. */
        display = DC;
        display->textFontRegistered = 1;
    }
    return qtrue;
}
