#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x4000d580..0x4000d63d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d580_4000d63d.mcode
// Exact same-module PPC symbol: UI_SelectCurrentGameType.
void UI_SelectCurrentGameType(void)
{
    enum {
        UI_GAMETYPE_NAME_COMPARE_LIMIT = 99999
    };
    char gameType[MAX_STRING_CHARS];
    int32_t gameTypeCount;
    int32_t index;

    trap_Cvar_VariableStringBuffer("g_gametype", gameType, (int32_t)sizeof(gameType));
    gameTypeCount = ui_gameTypeCount;

    for (index = 0; index < gameTypeCount; ++index) {
        const char *candidate = ui_gameTypes[index].gameType;

        if (candidate != NULL && Q_stricmpn(candidate, gameType, UI_GAMETYPE_NAME_COMPARE_LIMIT) == 0) {
            trap_Cvar_Set("ui_netGameType", va("%d", index));
            /* 0x4000d610 reloads the table cell after the first cvar call. */
            trap_Cvar_Set("ui_netGameTypeName", ui_gameTypes[index].gameType);
            return;
        }
    }
}
