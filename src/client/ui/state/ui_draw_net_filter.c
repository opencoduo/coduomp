#include "../module/ui_functions.h"

#include <stdint.h>

enum {
    UI_SERVER_FILTER_TEXT_STYLE = 0,
    UI_SERVER_FILTER_TEXT_LIMIT = 0
};

// Source: uo_ui_mp_x86.dll 0x4000a070..0x4000a0f7
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a070_4000a0f7.mcode
// Exact same-module PPC symbol: UI_DrawNetFilter.
void UI_DrawNetFilter(const rectDef_t *rect, int32_t font, float scale, const vec4_t color, int32_t textStyle)
{
    const char *message;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_serverFilterType < 0 || ui_serverFilterType >= UI_SERVER_FILTER_COUNT) {
        ui_serverFilterType = 0;
    }

    message = trap_SE_LocalizeMessage(va("EXE_SERVERFILTER\x14%s", ui_serverFilters[ui_serverFilterType].label), "server filter");
    trap_R_Text_Paint(rect->x, rect->y, font, scale, color, message, UI_SERVER_FILTER_TEXT_STYLE, UI_SERVER_FILTER_TEXT_LIMIT, textStyle);
}
