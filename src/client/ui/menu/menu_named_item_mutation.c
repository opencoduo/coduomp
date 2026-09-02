#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000baa0..0x4000bb01
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000baa0_4000bb01.mcode
// Role name: Menu_SetItemBackground.
void Menu_SetItemBackground(const char *itemName, const char *backgroundName)
{
    itemDef_t *item = Menu_FindItemByName(Menu_GetFocused(), itemName);

    if (item != NULL) {
        item->window.background = DC->registerShaderNoMip(backgroundName, R_IMAGE_TRACK_UI);
    }
}

// Source: uo_ui_mp_x86.dll 0x4000bb10..0x4000bb6d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000bb10_4000bb6d.mcode
// Role name: Menu_SetItemVisible.
void Menu_SetItemVisible(const char *itemName, qboolean visible)
{
    itemDef_t *item = Menu_FindItemByName(Menu_GetFocused(), itemName);

    if (item == NULL) {
        return;
    }
    if (visible) {
        item->window.flags |= WINDOW_VISIBLE;
    } else {
        item->window.flags &= ~WINDOW_VISIBLE;
    }
}
