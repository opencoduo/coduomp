#include "../module/ui_functions.h"

enum {
    UI_KEY_BIND_TEXT_STYLE = 0,
    UI_KEY_BIND_TEXT_LIMIT = 0
};

// Source: uo_ui_mp_x86.dll 0x4000a7f0..0x4000a890
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a7f0_4000a890.mcode
// Exact same-module PPC symbol: UI_DrawKeyBindStatus.
void UI_DrawKeyBindStatus(const rectDef_t *rect, int32_t font, float scale, const vec4_t color, int32_t textStyle)
{
    const char *text = UI_SafeTranslateString(g_waitingForKey ? "EXE_KEYWAIT" : "EXE_KEYCHANGE");

    trap_R_Text_Paint(rect->x, rect->y, font, scale, color, text, UI_KEY_BIND_TEXT_STYLE, UI_KEY_BIND_TEXT_LIMIT, textStyle);
}
