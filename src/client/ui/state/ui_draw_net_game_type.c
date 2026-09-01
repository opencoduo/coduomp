#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40009b50..0x40009c06
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009b50_40009c06.mcode
// Exact same-module PPC symbol: UI_DrawNetGameType.
void UI_DrawNetGameType(const rectDef_t *rect, int32_t font, float scale,
                        const vec4_t color, int32_t textStyle)
{
    const char *displayName;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_gameTypeCount <= 0) {
        displayName = "EXE_ALL";
        goto draw;
    }
    if (ui_netGameType < 0 || ui_netGameType >= ui_gameTypeCount) {
        ui_netGameType = 0;
        trap_Cvar_Set("ui_netGameType", "0");
        trap_Cvar_Set("ui_netGameTypeName", ui_gameTypes[0].gameType);
    }

    displayName = ui_gameTypes[ui_netGameType].displayName;
    if (displayName[0] == '\0') {
        displayName = "EXE_ALL";
    }
draw:
    displayName = UI_SafeTranslateString(displayName);
    trap_R_Text_Paint(rect->x, rect->y, font, scale, color,
                      displayName, 0, 0, textStyle);
}
