#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

#include <string.h>

enum {
    UI_MENU_DIRECTORY_SIZE = 256
};

// Source: uo_ui_mp_x86.dll 0x40009580..0x40009750
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009580_40009750.mcode
// Exact same-module PPC symbol and call graph: Load_Menu.
qboolean Load_Menu(int32_t sourceHandle, int32_t loadMode)
{
    pc_token_t token;

    if (!trap_PC_ReadToken(sourceHandle, &token) || token.string[0] != '{') {
        return qfalse;
    }

    while (trap_PC_ReadToken(sourceHandle, &token)) {
        int32_t languageIndex;

        if (token.string[0] == '\0') {
            return qfalse;
        }
        if (token.string[0] == '}') {
            return qtrue;
        }

        languageIndex =
            coduo_crt_atoi(UI_Cvar_VariableString("cl_language"));
        if (languageIndex != 0) {
            char directory[UI_MENU_DIRECTORY_SIZE];
            const char *basename = token.string;
            char *directoryEnd = directory;
            char *cursor;
            size_t filenameLength = strlen(token.string);
            const char *localizedDirectory;
            const char *localizedFilename;

            /* NOT_FROM_ORIGINAL_SOURCE: the filename length supplies both the
             * copy count and terminator index. Validate that operation against
             * the fixed directory buffer before publishing the path. */
            if (filenameLength > sizeof(directory)) {
                PC_SourceError(
                    sourceHandle,
                    "menu filename exceeds localized path capacity\n");
                return qfalse;
            }

            strncpy(directory, token.string, filenameLength - 1);
            directory[filenameLength - 1] = '\0';
            for (cursor = directory; *cursor != '\0'; ++cursor) {
                if (*cursor == '/') {
                    directoryEnd = cursor + 1;
                }
            }
            *directoryEnd = '\0';
            for (cursor = token.string; *cursor != '\0'; ++cursor) {
                if (*cursor == '/') {
                    basename = cursor + 1;
                }
            }

            localizedDirectory = va("%s%s/", directory,
                                    trap_GetLanguagename(languageIndex));
            localizedFilename = va("%s%s", localizedDirectory, basename);
            if (UI_ParseMenu(localizedFilename, loadMode)) {
                continue;
            }
        }
        UI_ParseMenu(token.string, loadMode);
    }
    return qfalse;
}
