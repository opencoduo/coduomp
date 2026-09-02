#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40009c10..0x40009ce4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009c10_40009ce4.mcode
// Exact same-module PPC symbol: UI_DrawJoinGameType.
void UI_DrawJoinGameType(const rectDef_t *rect, int32_t font, float scale,
                         const vec4_t color, int32_t textStyle)
{
    const char *displayName;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_joinGameType < 0 ||
        ui_joinGameType >= ui_joinGameTypeCount) {
        ui_joinGameType = 0;
        trap_Cvar_Set("ui_joinGameType", "0");
    }

    displayName = ui_joinGameTypes[ui_joinGameType].displayName;
    if (displayName[0] == '\0') {
        displayName = "EXE_ALL";
    }
    displayName = UI_SafeTranslateString(displayName);
    trap_R_Text_Paint(rect->x, rect->y, font, scale, color,
                      displayName, 0, 0, textStyle);
}
