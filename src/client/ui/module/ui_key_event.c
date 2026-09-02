#include "ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000fdf0..0x4000feb1
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000fdf0_4000feb1.mcode
// Exact same-module PPC symbol: _UI_KeyEvent.
void UI_KeyEvent(int32_t key, qboolean down)
{
    menuDef_t *menu;

    if (menuCount <= 0) {
        return;
    }

    menu = Menu_GetFocused();
    if (menu != NULL) {
        if (trap_Cvar_VariableValue("cl_bypassMouseInput") != 0.0f) {
            ui_bypassMouseInput = qtrue;
        }
        if (key == K_ESCAPE && down &&
            !Menus_AnyFullScreenVisible()) {
            Menus_CloseAll();
            return;
        }
        Menu_HandleKey(menu, key, down);
        return;
    }

    trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
    if (!ui_bypassMouseInput) {
        trap_Key_ClearStates();
    }
    ui_bypassMouseInput = qfalse;
    trap_Cvar_Set("cl_paused", "0");
}
