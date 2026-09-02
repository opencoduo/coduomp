#include "../module/ui_functions.h"

enum {
    UI_SERVER_REBUILD_FORCE = 1
};

// Source: uo_ui_mp_x86.dll 0x4000b4f0..0x4000b58f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b4f0_4000b58f.mcode
// Exact same-module PPC symbol: UI_NetSource_HandleKey.
qboolean UI_NetSource_HandleKey(int32_t flags, float *special, int32_t key)
{
    (void)flags;
    (void)special;

    if (key != K_MOUSE1 && key != K_MOUSE2 &&
        key != K_ENTER && key != K_KP_ENTER) {
        return qfalse;
    }

    ui_netSource += key == K_MOUSE2 ? -1 : 1;
    if (ui_netSource >= LAN_SERVER_SOURCE_COUNT) {
        ui_netSource = LAN_SERVER_SOURCE_LOCAL;
    } else if (ui_netSource < LAN_SERVER_SOURCE_LOCAL) {
        ui_netSource = LAN_SERVER_SOURCE_FAVORITES;
    }

    UI_BuildServerDisplayList(UI_SERVER_REBUILD_FORCE);
    if (ui_netSource != LAN_SERVER_SOURCE_GLOBAL) {
        UI_StartServerRefresh(qtrue);
    }
    trap_Cvar_Set("ui_netSource", va("%d", ui_netSource));
    return qtrue;
}
