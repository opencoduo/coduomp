#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x400135e0..0x40013638
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400135e0_40013638.mcode
// Exact same-module PPC symbol: Script_FadeIn.
// This UI body contains the fade loop inline; cgame's same-named body calls
// Menu_FadeItemByName, so this function intentionally remains module-local.
void Script_FadeIn(itemDef_t *item, char **arguments)
{
    const char *name;
    menuDef_t *menu;
    int32_t count;
    int32_t index;

    if (!String_Parse(arguments, &name)) {
        return;
    }

    menu = item->parent;
    count = Menu_ItemsMatchingGroup(menu, name);
    for (index = 0; index < count; ++index) {
        itemDef_t *matchingItem = Menu_GetMatchingItemByNumber(menu, name, index);

        if (matchingItem != NULL) {
            matchingItem->window.flags = (matchingItem->window.flags & ~WINDOW_FADINGOUT) | WINDOW_FADINGIN | WINDOW_VISIBLE;
        }
    }
}
