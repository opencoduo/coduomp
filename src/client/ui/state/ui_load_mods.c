#include "../module/ui_functions.h"

#include <string.h>

enum {
    UI_BASEGAME_BUFFER_SIZE = 64,
    UI_BASEGAME_COPY_SIZE = 63,
    UI_MOD_LIST_BUFFER_SIZE = 2048
};

// Source: uo_ui_mp_x86.dll 0x4000b720..0x4000b858
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b720_4000b858.mcode
// Exact same-module PPC symbol: UI_LoadMods.
void UI_LoadMods(void)
{
    char basegame[UI_BASEGAME_BUFFER_SIZE];
    char list[UI_MOD_LIST_BUFFER_SIZE];
    char *cursor = list;
    int32_t fileCount;
    int32_t fileIndex;

    /* strncpy(dst, src, 0x3f) + dst[0x3f] = 0: 63 characters survive. */
    Q_strncpyz(basegame, UI_Cvar_VariableString("fs_basegame"), UI_BASEGAME_COPY_SIZE + 1);
    ui_modCount = 0;
    fileCount = trap_FS_GetFileList("$modlist", "", list, sizeof(list));

    for (fileIndex = 0; fileIndex < fileCount && ui_modCount < UI_MAX_MODS; ++fileIndex) {
        char *directory = cursor;
        char *description = directory + strlen(directory) + 1;

        if (strcmp(directory, "main") != 0 && strcmp(directory, basegame) != 0) {
            ui_mods[ui_modCount].directory = String_Alloc(directory);
            ui_mods[ui_modCount].description = String_Alloc(description);
            ++ui_modCount;
        }
        cursor = description + strlen(description) + 1;
    }
}
