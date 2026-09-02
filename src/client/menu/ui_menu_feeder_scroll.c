// Sources: uo_cgame_mp_x86.dll 0x30058840..0x300588a4 and
//          uo_ui_mp_x86.dll    0x4001a3b0..0x4001a414

#include "ui_runtime.h"

qboolean Item_ListBox_HandleKey(itemDef_t *item, int32_t key,
                                qboolean down);

/* Route one forced up/down key to the first item bound to a feeder.  The
 * command keys are the literal 154/155 values formed at 0x30058892. */
void Menu_ScrollFeeder(menuDef_t *menu, int32_t feeder, qboolean down)
{
    if (menu == NULL) {
        return;
    }

    const int32_t itemCount = menu->itemCount;
    const float feederId = (float)feeder;
    for (int32_t i = 0; i < itemCount; ++i) {
        itemDef_t *item = menu->items[i];
        if (item->special == feederId) {
            /* 0x30058884 reloads the matching slot for the call rather than
             * retaining the pointer used by the comparison. */
            Item_ListBox_HandleKey(menu->items[i],
                                   down ? K_DOWNARROW : K_UPARROW,
                                   qtrue);
            return;
        }
    }
}
