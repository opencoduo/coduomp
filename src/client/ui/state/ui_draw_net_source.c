#include "../module/ui_functions.h"

/* Source: uo_ui_mp_x86.dll data 0x4003fe0c..0x4003fe17.
 * PE_RELOCATION_VALUES_VERIFIED: the three pointers target EXE_LOCAL,
 * EXE_INTERNET, and EXE_FAVORITES in source order. */
static const char *const ui_netSourceNames[] = {"EXE_LOCAL", "EXE_INTERNET", "EXE_FAVORITES"};

// Source: uo_ui_mp_x86.dll 0x40009ec0..0x40009f47
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009ec0_40009f47.mcode
// Exact same-module PPC symbol: UI_DrawNetSource.
void UI_DrawNetSource(const rectDef_t *rect, int32_t font, float scale, const vec4_t color, int32_t textStyle)
{
    const char *sourceName;
    const char *message;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_netSource < LAN_SERVER_SOURCE_LOCAL || ui_netSource >= LAN_SERVER_SOURCE_COUNT) {
        ui_netSource = LAN_SERVER_SOURCE_LOCAL;
    }

    sourceName = ui_netSourceNames[ui_netSource];

    message = trap_SE_LocalizeMessage(va("EXE_NETSOURCE\x14%s", sourceName), "net source");
    trap_R_Text_Paint(rect->x, rect->y, font, scale, color, message, 0, 0, textStyle);
}
