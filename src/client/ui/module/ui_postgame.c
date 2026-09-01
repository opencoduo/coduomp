#include "../abi/ui_module_abi.h"
#include "ui_functions.h"
#include "ui_globals.h"

enum {
    UI_POSTGAME_ACTIVE_MENU = 6
};

// Source: uo_ui_mp_x86.dll 0x400089d0..0x40008a28
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400089d0_40008a28.mcode
// Same-module PPC symbol: UI_ShowPostGame.
void UI_ShowPostGame(qboolean newHighScore)
{
    trap_Cvar_Set("cg_thirdPerson", "0");
    trap_Cvar_Set("sv_killserver", "1");
    ui_displayContextStorage.soundHighScore = newHighScore;
    if (menuCount > 0) {
        ui_activeMenu = UI_POSTGAME_ACTIVE_MENU;
    }
}
