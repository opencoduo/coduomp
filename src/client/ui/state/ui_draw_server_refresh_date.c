#include "../module/ui_functions.h"
#include "client/common/client_format_validation.h"

#include <math.h>

enum {
    UI_REFRESH_PULSE_DIVISOR = 75,
    UI_REFRESH_DATE_BUFFER_SIZE = 64,
    UI_REFRESH_DATE_COPY_SIZE = 63,
    UI_REFRESH_TEXT_STYLE = 0,
    UI_REFRESH_TEXT_LIMIT = 0
};

// Source: uo_ui_mp_x86.dll 0x4000a4d0..0x4000a6e2
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a4d0_4000a6e2.mcode
// Exact same-module PPC symbol: UI_DrawServerRefreshDate.
void UI_DrawServerRefreshDate(const rectDef_t *rect, int32_t font, float scale, const vec4_t color, int32_t textStyle)
{
    const char *text;
    const vec_t *paintColor = color;
    vec4_t dimmedColor;
    vec4_t pulseColor;
    char refreshDate[UI_REFRESH_DATE_BUFFER_SIZE];

    if (ui_serverRefreshActive) {
        int32_t component;
        int32_t phase = ui_displayContextStorage.context.realTime / UI_REFRESH_PULSE_DIVISOR;
        float fraction = (float)((sin((double)phase) + 1.0) * 0.5);

        for (component = 0; component < 4; ++component) {
            /* FMUL double ptr [0x400359c0] at 0x4000a4f9: the scale is a
             * double 0.8 (0x3fe999999999999a), not a float 0.8f. */
            dimmedColor[component] = color[component] * 0.8;
        }
        LerpColor(pulseColor, color, dimmedColor, fraction);
        paintColor = pulseColor;

        trap_LAN_GetServerCount(ui_netSource);
        if (trap_LAN_WaitServerResponse(ui_netSource)) {
            text = UI_SafeTranslateString("EXE_WAITINGFORMASTERSERVERRESPONSE");
        } else {
            int32_t serverCount = trap_LAN_GetServerCount(ui_netSource);
            const char *const format = UI_SafeTranslateString("EXE_GETTINGINFOFORSERVERS");
            /* NOT_FROM_ORIGINAL_SOURCE: accept the zero-or-one integer
             * localization contract; otherwise render the text literally. */
            if (client_compat_validate_format_signature(format, "i") == qfalse) {
                Com_Printf("WARNING: rejected invalid server-refresh format\n");
                text = format;
            } else {
                text = va(format, serverCount);
            }
        }
    } else {
        /* strncpy(dst, src, 0x3f) + dst[0x3f] = 0: 63 characters survive. */
        Q_strncpyz(refreshDate, UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource)), UI_REFRESH_DATE_COPY_SIZE + 1);
        const char *const format = UI_SafeTranslateString("EXE_REFRESHTIME");
        if (client_compat_validate_format_signature(format, "s") == qfalse) {
            Com_Printf("WARNING: rejected invalid refresh-date format\n");
            text = format;
        } else {
            text = va(format, refreshDate);
        }
    }

    trap_R_Text_Paint(rect->x, rect->y, font, scale, paintColor, text, UI_REFRESH_TEXT_STYLE, UI_REFRESH_TEXT_LIMIT, textStyle);
}
