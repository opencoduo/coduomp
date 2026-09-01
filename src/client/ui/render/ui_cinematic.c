#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_CINEMATIC_PLAY_FLAGS = 10,
    UI_CURRENT_MAP_CINEMATIC_HANDLE = -244,
    UI_SERVER_MAP_CINEMATIC_HANDLE = -246,
    UI_NO_CINEMATIC = -1
};

// Source: uo_ui_mp_x86.dll 0x4000f7d0..0x4000f80b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f7d0_4000f80b.mcode
// Exact same-module PPC symbol: UI_PlayCinematic.
int32_t UI_PlayCinematic(const char *name, float x, float y, float width,
                         float height)
{
    return trap_CIN_PlayCinematic(name, (int32_t)x, (int32_t)y,
                                  (int32_t)width, (int32_t)height,
                                  UI_CINEMATIC_PLAY_FLAGS);
}

// Source: uo_ui_mp_x86.dll 0x4000f810..0x4000f890
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f810_4000f890.mcode
// Exact same-module PPC symbol: UI_StopCinematic.
void UI_StopCinematic(int32_t handle)
{
    if (handle >= 0) {
        trap_CIN_StopCinematic(handle);
        return;
    }

    if (handle == UI_CURRENT_MAP_CINEMATIC_HANDLE) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (ui_currentMap < 0 || ui_currentMap >= ui_mapCount) {
            return;
        }
        int32_t cinematic = ui_maps[ui_currentMap].cinematic;

        if (cinematic >= 0) {
            trap_CIN_StopCinematic(cinematic);
            /* Retail reloads ui_currentMap after the callback. Preserve that
             * ownership while revalidating the reloaded selection before the
             * second access. */
            if (ui_currentMap >= 0 && ui_currentMap < ui_mapCount) {
                ui_maps[ui_currentMap].cinematic = UI_NO_CINEMATIC;
            }
        }
        return;
    }
    if (handle == UI_SERVER_MAP_CINEMATIC_HANDLE) {
        if (ui_serverMapCinematic >= 0) {
            trap_CIN_StopCinematic(ui_serverMapCinematic);
            ui_serverMapCinematic = UI_NO_CINEMATIC;
        }
    }
}

// Source: uo_ui_mp_x86.dll 0x4000f890..0x4000f8d4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f890_4000f8d4.mcode
// Exact same-module PPC symbol: UI_DrawCinematic.
void UI_DrawCinematic(int32_t handle, float x, float y, float width,
                      float height)
{
    trap_CIN_SetExtents(handle, (int32_t)x, (int32_t)y, (int32_t)width,
                        (int32_t)height);
    trap_CIN_DrawCinematic(handle);
}

// Source: uo_ui_mp_x86.dll 0x4000f8e0..0x4000f8f1
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f8e0_4000f8f1.mcode
// Exact same-module PPC symbol: UI_RunCinematicFrame.
void UI_RunCinematicFrame(int32_t handle)
{
    trap_CIN_RunCinematic(handle);
}
