#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    UI_OWNERDRAW_HANDICAP = 200,
    UI_OWNERDRAW_GAMETYPE = 205,
    UI_OWNERDRAW_NETSOURCE = 220,
    UI_OWNERDRAW_SERVERFILTER = 222,
    UI_OWNERDRAW_REFRESH_DATE = 247,
    UI_OWNERDRAW_KEY_STATUS = 250,
    UI_OWNERDRAW_REFRESH_TOTALS = 251,
    UI_OWNERDRAW_TEXT_LIMIT = 0,
    UI_HANDICAP_MINIMUM = 5,
    UI_HANDICAP_MAXIMUM = 100,
    UI_HANDICAP_STEP = 5,
    UI_HANDICAP_LABEL_LAST_INDEX = 21
};

/* The same three localized source labels used by UI_DrawNetSource. */
static const char *const ui_netSourceWidthLabels[] = {"EXE_LOCAL", "EXE_INTERNET", "EXE_FAVORITES"};

// Source: uo_ui_mp_x86.dll 0x4000a1a0..0x4000a392
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a1a0_4000a392.mcode
// Exact same-module PPC symbol: UI_OwnerDrawWidth.
int32_t UI_OwnerDrawWidth(int32_t ownerDraw, int32_t font, float scale)
{
    const char *text = NULL;

    switch (ownerDraw) {
    case UI_OWNERDRAW_HANDICAP: {
        /* The DLL calls Com_ClampFloat (0x40006100) here, not the byte-identical
         * sibling Com_Clamp (0x40007820): CALL 0x40006100 at 0x4000a1d8. Both have
         * the same body/signature so the value is unchanged; this is a call-target
         * (machine-code parity) fix, not a behavior change. */
        float handicap = Com_ClampFloat((float)UI_HANDICAP_MINIMUM, (float)UI_HANDICAP_MAXIMUM, trap_Cvar_VariableValue("handicap"));
        text = ui_handicapLabels[UI_HANDICAP_LABEL_LAST_INDEX - coduo_fp_to_i32_extended((long double)handicap) / UI_HANDICAP_STEP];
        break;
    }
    case UI_OWNERDRAW_GAMETYPE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_gameType < 0 || ui_gameType >= ui_gameTypeCount) {
            ui_gameType = 0;
            trap_Cvar_Set("ui_gameType", "0");
        }
        text = ui_gameTypes[ui_gameType].displayName;
        break;
    case UI_OWNERDRAW_NETSOURCE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_netSource < LAN_SERVER_SOURCE_LOCAL || ui_netSource >= LAN_SERVER_SOURCE_COUNT) {
            ui_netSource = LAN_SERVER_SOURCE_LOCAL;
        }
        text = trap_SE_LocalizeMessage(va("EXE_NETSOURCE\x14%s", ui_netSourceWidthLabels[ui_netSource]), "net source");
        break;
    case UI_OWNERDRAW_SERVERFILTER:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_serverFilterType < 0 || ui_serverFilterType >= UI_SERVER_FILTER_COUNT) {
            ui_serverFilterType = 0;
        }
        text = trap_SE_LocalizeMessage(va("EXE_SERVERFILTER\x14%s", ui_serverFilters[ui_serverFilterType].label), "server filter");
        break;
    case UI_OWNERDRAW_REFRESH_DATE:
        text = UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource));
        break;
    case UI_OWNERDRAW_KEY_STATUS:
        text = UI_SafeTranslateString(g_waitingForKey ? "EXE_KEYWAIT" : "EXE_KEYCHANGE");
        break;
    case UI_OWNERDRAW_REFRESH_TOTALS: {
        int32_t servers = coduo_crt_atoi(UI_Cvar_VariableString(va("ui_lastServerRefreshServers_%i", ui_netSource)));
        int32_t players = coduo_crt_atoi(UI_Cvar_VariableString(va("ui_lastServerRefreshPlayers_%i", ui_netSource)));
        Com_sprintf(ui_ownerDrawWidthBuffer, sizeof(ui_ownerDrawWidthBuffer), UI_SafeTranslateString("GMI_EXE_REFRESHTOTALS"), players,
                    servers);
        text = ui_ownerDrawWidthBuffer;
        break;
    }
    default:
        break;
    }

    if (text == NULL)
        return 0;
    return trap_R_Text_Width(text, font, scale, UI_OWNERDRAW_TEXT_LIMIT);
}
