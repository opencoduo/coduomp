#include "../module/ui_functions.h"

enum {
    UI_ARENA_FILE_BUFFER_SIZE = 8192
};

// Source: uo_ui_mp_x86.dll 0x400080e0..0x400081ea
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400080e0_400081ea.mcode
// Same-module PPC role: UI_LoadArenasFromFile.
void UI_LoadArenasFromFile(const char *filename)
{
    char buffer[UI_ARENA_FILE_BUFFER_SIZE];
    int32_t handle;
    int32_t length = trap_FS_FOpenFile(filename, &handle, FS_READ);

    if (handle == 0) {
        trap_Print(va("^1file not found: %s\n", filename));
        return;
    }
    if (length >= UI_ARENA_FILE_BUFFER_SIZE) {
        trap_Print(va("^1file too large: %s is %i, max allowed is %i",
                      filename, length, UI_ARENA_FILE_BUFFER_SIZE));
        trap_FS_FCloseFile(handle);
        return;
    }

    trap_FS_Read(buffer, length, handle);
    buffer[length] = '\0';
    trap_FS_FCloseFile(handle);
    ui_arenaInfoCount += UI_ParseInfos(
        buffer, UI_MAX_ARENA_INFOS - ui_arenaInfoCount,
        &ui_arenaInfos[ui_arenaInfoCount]);
}
