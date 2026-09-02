#include "../module/ui_functions.h"

enum {
    UI_SERVER_REBUILD_FORCE = 1
};

// Source: uo_ui_mp_x86.dll 0x4000b470..0x4000b4ee
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b470_4000b4ee.mcode
// Exact same-module PPC symbol: UI_JoinGameType_HandleKey.
qboolean UI_JoinGameType_HandleKey(int32_t flags, float *special,
                                   int32_t key)
{
    (void)flags;
    (void)special;

    if (key != K_MOUSE1 && key != K_MOUSE2 &&
        key != K_ENTER && key != K_KP_ENTER) {
        return qfalse;
    }

    ui_joinGameType += key == K_MOUSE2 ? -1 : 1;
    if (ui_joinGameType < 0) {
        ui_joinGameType = ui_joinGameTypeCount - 1;
    } else if (ui_joinGameType >= ui_joinGameTypeCount) {
        ui_joinGameType = 0;
    }

    trap_Cvar_Set("ui_joinGameType", va("%d", ui_joinGameType));
    UI_BuildServerDisplayList(UI_SERVER_REBUILD_FORCE);
    return qtrue;
}
