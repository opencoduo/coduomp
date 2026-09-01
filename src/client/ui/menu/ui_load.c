#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

enum {
    UI_SAVED_MENU_NAME_SIZE = 1024,
    UI_MENU_LOAD_ALL = 2
};

// Source: uo_ui_mp_x86.dll 0x400098e0..0x400099de
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400098e0_400099de.mcode
// Exact same-module PPC symbol and call graph: UI_Load.
void UI_Load(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    char savedMenuName[UI_SAVED_MENU_NAME_SIZE] = {0};
    qboolean reopenFocusedMenu = qfalse;
    menuDef_t *focusedMenu = Menu_GetFocused();
    const char *menuFile = UI_Cvar_VariableString("ui_menuFiles");

    if (focusedMenu != NULL && focusedMenu->window.name != NULL) {
        coduo_client_crt_strcpy(savedMenuName, focusedMenu->window.name);
        reopenFocusedMenu = qtrue;
    }
    if (*menuFile == '\0') {
        menuFile = "ui_mp/menus.txt";
    }

    String_Init();
    UI_GetGameTypesList();
    UI_LoadArenas();
    UI_LoadMenus(menuFile, qtrue, UI_MENU_LOAD_ALL);
    Menus_CloseAll();
    if (reopenFocusedMenu != qfalse) {
        Menus_OpenByName(savedMenuName);
    }
}
