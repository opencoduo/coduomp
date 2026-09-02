#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40009aa0..0x40009b43
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009aa0_40009b43.mcode
// Exact same-module PPC symbol: UI_DrawGameType.
void UI_DrawGameType(const rectDef_t *rect, int32_t font, float scale, const vec4_t color, int32_t textStyle)
{
    const char *displayName = "All";

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_gameTypeCount > 0) {
        if (ui_gameType < 0 || ui_gameType >= ui_gameTypeCount) {
            ui_gameType = 0;
            trap_Cvar_Set("ui_gameType", "0");
        }
        displayName = ui_gameTypes[ui_gameType].displayName;
    }

    if (displayName[0] == '\0') {
        displayName = "All";
    }
    trap_R_Text_Paint(rect->x, rect->y, font, scale, color, displayName, 0, 0, textStyle);
}
