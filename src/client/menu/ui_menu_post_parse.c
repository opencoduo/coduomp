#include "ui_parse.h"
#include "ui_runtime.h"

// Source: uo_cgame_mp_x86.dll 0x30051070..0x3005109f;
//         uo_ui_mp_x86.dll    0x40012b90..0x40012bbf (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40012b90_40012bbf.mcode
// Exact same-module PPC symbol: Menu_PostParse.
void Menu_PostParse(menuDef_t *menu)
{
    if (menu == NULL) {
        return;
    }

    if (menu->fullScreen) {
        menu->window.rect.x = 0.0f;
        menu->window.rect.y = 0.0f;
        menu->window.rect.w = 640.0f;
        menu->window.rect.h = 480.0f;
    }
    Menu_UpdatePosition(menu);
}
