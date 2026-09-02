#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

#include <string.h>

#define UI_SCRIPT_MENU_DIRECTORY "ui_mp/scriptmenus/"
#define UI_SCRIPT_MENU_EXTENSION ".menu"

enum {
    UI_SCRIPT_MENU_PATH_SIZE = 256,
    UI_SCRIPT_MENU_NAME_MAX = UI_SCRIPT_MENU_PATH_SIZE - (sizeof(UI_SCRIPT_MENU_DIRECTORY) - 1) - sizeof(UI_SCRIPT_MENU_EXTENSION)
};

// Source: uo_ui_mp_x86.dll 0x40009400..0x40009577
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009400_40009577.mcode
// Exact same-module PPC symbol: Load_ScriptMenu.
qboolean Load_ScriptMenu(const char *menuName, int32_t loadMode)
{
    char menuPath[UI_SCRIPT_MENU_PATH_SIZE] = UI_SCRIPT_MENU_DIRECTORY;
    int32_t languageIndex;

    /* NOT_FROM_ORIGINAL_SOURCE: the directory, menu name, extension, and
     * terminator must fit the fixed path buffer. Reject an over-domain name
     * through the UI error boundary before either append. */
    if (strlen(menuName) > UI_SCRIPT_MENU_NAME_MAX) {
        trap_Error("Load_ScriptMenu: script menu name exceeds path capacity");
        return qfalse;
    }

    strcat(menuPath, menuName);
    strcat(menuPath, UI_SCRIPT_MENU_EXTENSION);

    languageIndex = coduo_crt_atoi(UI_Cvar_VariableString("cl_language"));
    if (languageIndex != 0) {
        char directory[UI_SCRIPT_MENU_PATH_SIZE];
        const char *basename = menuPath;
        const char *cursor;
        const char *localizedDirectory;
        const char *localizedFilename;

        Com_StripFilename(menuPath, directory);
        for (cursor = menuPath; *cursor != '\0'; ++cursor) {
            if (*cursor == '/') {
                basename = cursor + 1;
            }
        }

        localizedDirectory = va("%s%s/", directory, trap_GetLanguagename(languageIndex));
        localizedFilename = va("%s%s", localizedDirectory, basename);
        if (UI_ParseMenu(localizedFilename, loadMode)) {
            return qtrue;
        }
    }

    return UI_ParseMenu(menuPath, loadMode) != 0;
}

#undef UI_SCRIPT_MENU_EXTENSION
#undef UI_SCRIPT_MENU_DIRECTORY
