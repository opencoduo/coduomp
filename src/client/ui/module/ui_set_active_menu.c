#include "ui_functions.h"

enum {
    UI_MENU_CVAR_BUFFER_SIZE = 256,
    UI_CURSOR_RIGHT = 639,
    UI_CURSOR_BOTTOM = 479,
    UI_CURSOR_CENTER_X = 320,
    UI_CURSOR_CENTER_Y = 240,
    UI_MENU_BUFFER_COMPARE_LIMIT = 99999
};

// Source: uo_ui_mp_x86.dll 0x4000ff40..0x400103d4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000ff40_400103d4.mcode
// Exact same-module PPC symbol: _UI_SetActiveMenu.
// Returns 1 on the handled menu paths, 0 on the early-outs
// (no menus, unhandled command, script-menu-over-open-menu bail).
int32_t UI_SetActiveMenu(int32_t menuValue)
{
    uiMenuCommand_t menu = (uiMenuCommand_t)menuValue;
    int32_t previousMenu = menuValue;
    char buffer[UI_MENU_CVAR_BUFFER_SIZE];

    if (menuCount <= 0) {
        return 0;
    }
    if (menu != UI_MENU_SCRIPT && menu != UI_MENU_SCRIPT_FULLSCREEN) {
        ui_activeMenu = menuValue;
    } else {
        previousMenu = ui_activeMenu;
    }

    switch (menu) {
    case UI_MENU_NONE:
        trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
        trap_Key_ClearStates();
        trap_Cvar_Set("cl_paused", "0");
        Menus_CloseAll();
        return 1;

    case UI_MENU_MAIN:
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_OpenByName("main");
        trap_Cvar_VariableStringBuffer("com_errorMessage", buffer, sizeof(buffer));
        if (buffer[0] != '\0' && Q_stricmpn(";", buffer, UI_MENU_BUFFER_COMPARE_LIMIT) != 0) {
            Menus_OpenByName("error_popmenu");
        }
        return 1;

    case UI_MENU_INGAME:
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        trap_Cvar_VariableStringBuffer("g_scriptMainMenu", buffer, sizeof(buffer));
        if (!Menus_OpenByName(buffer)) {
            Menus_OpenByName("main");
        }
        return 1;

    case UI_MENU_NEED_CD:
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_OpenByName("needcd");
        return 1;

    case UI_MENU_BAD_CD:
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_OpenByName("badcd");
        return 1;

    case UI_MENU_TEAM:
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_OpenByName("team");
        return 1;

    case UI_MENU_QUICK_MESSAGE:
        DC->cursorx = UI_CURSOR_RIGHT;
        DC->cursory = UI_CURSOR_BOTTOM;
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        Menus_OpenByName("quickmessage");
        return 1;

    case UI_MENU_AUTOUPDATE:
        Menus_OpenByName("autoupdate");
        return 1;

    case UI_MENU_QUICK_MAP:
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        trap_Cvar_VariableStringBuffer("g_scriptQuickMap", buffer, sizeof(buffer));
        if (!Menus_OpenByName(buffer)) {
            Menus_OpenByName("main");
        }
        DC->cursorx = UI_CURSOR_RIGHT;
        DC->cursory = UI_CURSOR_BOTTOM;
        return 1;

    case UI_MENU_PURCHASE:
        DC->cursorx = UI_CURSOR_RIGHT;
        DC->cursory = UI_CURSOR_BOTTOM;
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        Menus_OpenByName("purchase");
        return 1;

    case UI_MENU_SCRIPT:
    case UI_MENU_SCRIPT_FULLSCREEN: {
        menuDef_t *focused = Menu_GetFocused();

        if (focused != NULL && previousMenu != UI_MENU_SCRIPT && previousMenu != UI_MENU_SCRIPT_FULLSCREEN) {
            return 0;
        }
        trap_Cvar_VariableStringBuffer("ui_newScriptMenu", buffer, sizeof(buffer));
        if (focused != NULL && Q_stricmp(buffer, focused->window.name) == 0) {
            return 1;
        }

        ui_activeMenu = UI_MENU_SCRIPT;
        if (menu == UI_MENU_SCRIPT_FULLSCREEN) {
            DC->cursorx = UI_CURSOR_RIGHT;
            DC->cursory = UI_CURSOR_BOTTOM;
        } else {
            DC->cursorx = UI_CURSOR_CENTER_X;
            DC->cursory = UI_CURSOR_CENTER_Y;
        }
        trap_Key_SetCatcher(KEYCATCH_UI);
        Menus_CloseAll();
        trap_Cvar_Set("ui_scriptMenu", buffer);
        trap_Cvar_VariableStringBuffer("ui_newScriptMenuIndex", buffer, sizeof(buffer));
        trap_Cvar_Set("ui_scriptMenuIndex", buffer);
        trap_Cvar_Set("ui_newScriptMenu", "");
        trap_Cvar_Set("ui_newScriptMenuIndex", "-1");
        trap_Cvar_VariableStringBuffer("ui_scriptMenu", buffer, sizeof(buffer));
        Menus_OpenByName(buffer);
        return 1;
    }

    case UI_MENU_UNUSED_6:
    case UI_MENU_HELP:
    default:
        return 0;
    }
}
