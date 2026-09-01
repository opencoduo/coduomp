#include "ui_runtime.h"

#include <stddef.h>

/*
 * Instruction-identical in the original Windows client modules:
 * uo_cgame_mp_x86.dll 0x3005b300..0x3005b32b and
 * uo_ui_mp_x86.dll    0x4001d020..0x4001d04b.
 */
void Display_HandleKey(int32_t x, int32_t y, int32_t key, qboolean down)
{
    menuDef_t *menu = Menu_GetAtPoint(x, y);

    if (menu == NULL) {
        menu = Menu_GetFocused();
    }
    if (menu != NULL) {
        Menu_HandleKey(menu, key, down);
    }
}
