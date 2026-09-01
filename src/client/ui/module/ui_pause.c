#include "ui_functions.h"

enum {
    UI_KEY_CATCHER = 2
};

// Source: uo_ui_mp_x86.dll 0x4000f770..0x4000f7ca
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f770_4000f7ca.mcode
// Exact same-module PPC symbol: UI_Pause.
void UI_Pause(qboolean pause)
{
    if (pause) {
        trap_Cvar_Set("cl_paused", "1");
        trap_Key_SetCatcher(UI_KEY_CATCHER);
        return;
    }

    trap_Key_SetCatcher(trap_Key_GetCatcher() & ~UI_KEY_CATCHER);
    trap_Key_ClearStates();
    trap_Cvar_Set("cl_paused", "0");
}
