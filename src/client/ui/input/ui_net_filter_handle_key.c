#include "../module/ui_functions.h"

enum { UI_SERVER_REBUILD_FORCE = 1 };

// Source: uo_ui_mp_x86.dll 0x4000b590..0x4000b5ea
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b590_4000b5ea.mcode
// Exact same-module PPC symbol: UI_NetFilter_HandleKey.
qboolean UI_NetFilter_HandleKey(int32_t flags, float *special, int32_t key)
{
    (void)flags;
    (void)special;

    if (key != K_MOUSE1 && key != K_MOUSE2 &&
        key != K_ENTER && key != K_KP_ENTER) {
        return qfalse;
    }

    ui_serverFilterType += key == K_MOUSE2 ? -1 : 1;
    if (ui_serverFilterType < 0 ||
        ui_serverFilterType >= UI_SERVER_FILTER_COUNT) {
        ui_serverFilterType = 0;
    }

    UI_BuildServerDisplayList(UI_SERVER_REBUILD_FORCE);
    return qtrue;
}
