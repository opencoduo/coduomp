#include "../module/ui_functions.h"

#include <string.h>

enum {
    UI_DEMO_PROTOCOL = 3,
    UI_DEMO_EXTENSION_BUFFER_SIZE = 32,
    UI_DEMO_LIST_BUFFER_SIZE = 4096
};

// Source: uo_ui_mp_x86.dll 0x4000b950..0x4000ba8f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b950_4000ba8f.mcode
// Exact same-module PPC symbol: UI_LoadDemos.
void UI_LoadDemos(void)
{
    enum {
        UI_DEMO_SUFFIX_COMPARE_LIMIT = 99999
    };
    char extension[UI_DEMO_EXTENSION_BUFFER_SIZE];
    char suffix[UI_DEMO_EXTENSION_BUFFER_SIZE];
    char list[UI_DEMO_LIST_BUFFER_SIZE];
    char *filename = list;
    int32_t demoIndex;

    Com_sprintf(extension, sizeof(extension), "dm_%d", UI_DEMO_PROTOCOL);
    ui_demoCount = trap_FS_GetFileList("demos", extension, list, sizeof(list));
    Com_sprintf(suffix, sizeof(suffix), ".dm_%d", UI_DEMO_PROTOCOL);
    if (ui_demoCount > UI_MAX_DEMOS) {
        ui_demoCount = UI_MAX_DEMOS;
    }

    for (demoIndex = 0; demoIndex < ui_demoCount; ++demoIndex) {
        size_t filenameLength = strlen(filename);
        size_t suffixLength = strlen(suffix);
        char *filenameSuffix = NULL;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (filenameLength >= suffixLength) {
            filenameSuffix = filename + filenameLength - suffixLength;
        }
        if (filenameSuffix != NULL && Q_stricmpn(suffix, filenameSuffix, UI_DEMO_SUFFIX_COMPARE_LIMIT) == 0) {
            *filenameSuffix = '\0';
        }
        Q_strupr(filename);
        ui_demoNames[demoIndex] = String_Alloc(filename);
        filename += filenameLength + 1;
    }
}
