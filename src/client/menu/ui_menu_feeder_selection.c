// Sources: uo_cgame_mp_x86.dll 0x300588b0..0x3005897a and
//          uo_ui_mp_x86.dll    0x4001a420..0x4001a4ea
//
// Menu_SetFeederSelection -- set every item in a menu bound to a given feeder
// to the requested row, resetting listbox scroll/cursor state when row zero is
// selected. Name confirmed by the ui_mp Mac symbol (0xf8 bytes).
// Register ABI: menu ECX, optional menuName EAX; feeder/index are stack args.

#include "ui_runtime.h"

extern displayContextDef_t *DC;

void Com_Printf(const char *format, ...);

void Menu_SetFeederSelection(menuDef_t *menu, const char *menuName,
                             int32_t feeder, int32_t index)
{
    if (menu == NULL) {
        menu = menuName == NULL ? Menu_GetFocused() : Menus_FindByName(menuName);
        if (menu == NULL) {
            return;
        }
    }

    float feederID = (float)feeder;
    for (int32_t i = 0; i < menu->itemCount; ++i) {
        itemDef_t *item = menu->items[i];
        if (item->special != feederID) {
            continue;
        }

        if (index == 0) {
            if (item->typeValidated != ITEM_TYPE_LISTBOX) {
                Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
                continue;
            }
            listBoxDef_t *list = (listBoxDef_t *)item->typeData;
            if (list == NULL) {
                continue;
            }
            list->cursorPos = 0;
            list->startPos = 0;
        }

        /* 0x30058931 reloads menu->items[i] before storing the item cursor,
         * after the optional listbox-state checks above. */
        item = menu->items[i];
        item->cursorPos = index;
        {
            int32_t cursorPos = item->cursorPos;
            float special = item->special;
            displayContextDef_t *display = DC;
            display->feederSelection(special, cursorPos);
        }
    }
}
