#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"
#include "client/common/client_format_validation.h"

enum {
    UI_REFRESH_TOTALS_TEXT_STYLE = 0,
    UI_REFRESH_TOTALS_TEXT_LIMIT = 0
};

// Source: uo_ui_mp_x86.dll 0x4000a6f0..0x4000a7e8
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a6f0_4000a7e8.mcode
// Exact same-module PPC symbol: UI_DrawServerRefreshTotals.
void UI_DrawServerRefreshTotals(const rectDef_t *rect, int32_t font,
                                float scale, const vec4_t color,
                                int32_t textStyle)
{
    int32_t servers = coduo_crt_atoi(UI_Cvar_VariableString(
        va("ui_lastServerRefreshServers_%i", ui_netSource)));
    int32_t players = coduo_crt_atoi(UI_Cvar_VariableString(
        va("ui_lastServerRefreshPlayers_%i", ui_netSource)));
    const char *text = "";

    if (servers != 0) {
        const char *const format =
            UI_SafeTranslateString("GMI_EXE_REFRESHTOTALS");
        /* NOT_FROM_ORIGINAL_SOURCE: accept only the two-integer conversion
         * contract supplied by this call site. */
        if (client_compat_validate_format_signature(format, "ii") == qfalse) {
            Com_Printf("WARNING: rejected invalid server-total format\n");
            text = format;
        } else {
            text = va(format, players, servers);
        }
    }

    trap_R_Text_Paint(rect->x, rect->y, font, scale, color, text,
                      UI_REFRESH_TOTALS_TEXT_STYLE,
                      UI_REFRESH_TOTALS_TEXT_LIMIT, textStyle);
}
