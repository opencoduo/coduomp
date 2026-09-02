#include <string.h>

#include "../module/ui_functions.h"

enum {
    UI_GAMETYPE_FILE_LIST_SIZE = 4096,
    UI_GAMETYPE_SUFFIX_LENGTH = 4,
    UI_GAMETYPE_MAX_LOADABLE = UI_MAX_GAMETYPES - 1,
};

// Source: uo_ui_mp_x86.dll 0x4000f530..0x4000f765
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f530_4000f765.mcode
// Exact same-module PPC symbol: UI_GetGameTypesList.
void UI_GetGameTypesList(void)
{
    enum { UI_GAMETYPE_SUFFIX_COMPARE_LIMIT = 99999 };
    char fileList[UI_GAMETYPE_FILE_LIST_SIZE];
    char *filename = fileList;
    int32_t fileCount;
    int32_t fileIndex;

    ui_gameTypeCount = 0;
    ui_joinGameTypeCount = 0;
    ui_joinGameTypes[0].gameType = String_Alloc("All");
    ui_joinGameTypes[0].displayName = "";
    ui_joinGameTypeCount = 1;

    fileCount = trap_FS_GetFileList("maps/mp/gametypes", "gsc", fileList,
                                    sizeof(fileList));
    for (fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        size_t length = strlen(filename);

        if (filename[0] != '_') {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (length < UI_GAMETYPE_SUFFIX_LENGTH) {
                Com_Printf("WARNING: ignoring malformed gametype filename %s\n",
                           filename);
                filename += length + 1;
                continue;
            }
            char *suffix = filename + length - UI_GAMETYPE_SUFFIX_LENGTH;

            if (Q_stricmpn(".gsc", suffix,
                           UI_GAMETYPE_SUFFIX_COMPARE_LIMIT) == 0) {
                suffix[0] = '\0';
            }
            if (ui_gameTypeCount == UI_MAX_GAMETYPES ||
                ui_joinGameTypeCount == UI_MAX_GAMETYPES) {
                Com_Printf("Too many game type scripts found! Only loading "
                           "the first %i\n",
                           UI_GAMETYPE_MAX_LOADABLE);
                break;
            }

            ui_gameTypes[ui_gameTypeCount].gameType = String_Alloc(filename);
            ui_joinGameTypes[ui_joinGameTypeCount].gameType =
                ui_gameTypes[ui_gameTypeCount].gameType;
            {
                char *fileText = UI_LoadMenuTextFile(
                    va("maps/mp/gametypes/%s.txt", filename));
                const char *displayName;

                if (fileText == NULL) {
                    displayName =
                        ui_gameTypes[ui_gameTypeCount].gameType;
                } else {
                    displayName = String_Alloc(Com_Parse(&fileText));
                }
                ui_gameTypes[ui_gameTypeCount].displayName = displayName;
                ui_joinGameTypes[ui_joinGameTypeCount].displayName =
                    displayName;
            }
            ++ui_gameTypeCount;
            ++ui_joinGameTypeCount;
        }
        filename += length + 1;
    }

    if (ui_gameTypeCount == 0) {
        Com_Error(ERR_FATAL,
                  "\x15" "No game type scripts found in maps/mp/gametypes folder");
    }
}
