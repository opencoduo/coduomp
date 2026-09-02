#include "../module/ui_functions.h"

enum {
    UI_CREATE_SERVER_MAP_FEEDER = 4
};

// Source: uo_ui_mp_x86.dll 0x4000b3a0..0x4000b466
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b3a0_4000b466.mcode
// Exact same-module PPC symbol: UI_NetGameType_HandleKey.
qboolean UI_NetGameType_HandleKey(int32_t flags, float *special, int32_t key)
{
    (void)flags;
    (void)special;

    if (key != K_MOUSE1 && key != K_MOUSE2 && key != K_ENTER && key != K_KP_ENTER) {
        return qfalse;
    }

    ui_netGameType += key == K_MOUSE2 ? -1 : 1;
    if (ui_netGameType < 0) {
        ui_netGameType = ui_gameTypeCount - 1;
    } else if (ui_netGameType >= ui_gameTypeCount) {
        ui_netGameType = 0;
    }

    trap_Cvar_Set("ui_netGameType", va("%d", ui_netGameType));
    trap_Cvar_Set("ui_netGameTypeName", ui_gameTypes[ui_netGameType].gameType);
    trap_Cvar_Set("ui_currentNetMap", "0");
    UI_MapCountByGameType();
    UI_FeederSelection((float)UI_CREATE_SERVER_MAP_FEEDER, 0);
    Menu_SetFeederSelection(NULL, "createserver_maps", UI_CREATE_SERVER_MAP_FEEDER, 0);
    UI_SelectCurrentMap();
    return qtrue;
}
