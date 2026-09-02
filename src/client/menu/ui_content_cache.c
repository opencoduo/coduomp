#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <stddef.h>
#include <stdint.h>

extern displayContextDef_t *DC;

/*
 * This complete content-cache/query cluster is instruction-identical between
 * the original Windows client modules after rebasing image-local globals and
 * calls:
 *
 *                                   cgame       UI
 * Window_CacheContents              0x3005b330  0x4001d050
 * Item_CacheContents                0x3005b360  0x4001d080
 * Menu_CacheContents                0x3005b390  0x4001d0b0
 * Display_CacheAll                  0x3005b420  0x4001d140
 * Menu_OverActiveItem               0x3005b450  0x4001d170
 *
 * WINDOW_MOUSE_ACTIVE names the exact 0x00100004 mask tested by both copies.
 */
void Window_CacheContents(windowDef_t *window)
{
    if (window != NULL && window->cinematicName != NULL) {
        const char *cinematicName = window->cinematicName;
        displayContextDef_t *display = DC;
        int32_t handle = display->playCinematic(cinematicName, 0, 0, 0, 0);

        display = DC;
        display->stopCinematic(handle);
    }
}

void Item_CacheContents(itemDef_t *item)
{
    if (item != NULL && item->window.cinematicName != NULL) {
        const char *cinematicName = item->window.cinematicName;
        displayContextDef_t *display = DC;
        int32_t handle = display->playCinematic(cinematicName, 0, 0, 0, 0);

        display = DC;
        display->stopCinematic(handle);
    }
}

void Menu_CacheContents(menuDef_t *menu)
{
    uint32_t indexBits = 0u;

    if (menu == NULL) {
        return;
    }
    if (menu->window.cinematicName != NULL) {
        const char *cinematicName = menu->window.cinematicName;
        displayContextDef_t *display = DC;
        int32_t handle = display->playCinematic(cinematicName, 0, 0, 0, 0);

        display = DC;
        display->stopCinematic(handle);
    }
    while (coduo_int32_from_bits(indexBits) < menu->itemCount) {
        itemDef_t *item = menu->items[indexBits];

        if (item != NULL && item->window.cinematicName != NULL) {
            const char *cinematicName = item->window.cinematicName;
            displayContextDef_t *display = DC;
            int32_t handle = display->playCinematic(cinematicName, 0, 0, 0, 0);

            display = DC;
            display->stopCinematic(handle);
        }
        indexBits += 1u;
    }
}

void Display_CacheAll(void)
{
    uint32_t indexBits = 0u;

    while (coduo_int32_from_bits(indexBits) < menuCount) {
        Menu_CacheContents(&Menus[indexBits]);
        indexBits += 1u;
    }
}

qboolean Menu_OverActiveItem(menuDef_t *menu, float x, float y)
{
    uint32_t indexBits;

    if (menu == NULL || ((uint32_t)menu->window.flags & WINDOW_MOUSE_ACTIVE) == 0u || !Rect_ContainsPoint(&menu->window.rect, x, y)) {
        return qfalse;
    }

    for (indexBits = 0u; coduo_int32_from_bits(indexBits) < menu->itemCount; indexBits += 1u) {
        itemDef_t *item = menu->items[indexBits];

        if (((uint32_t)item->window.flags & WINDOW_MOUSE_ACTIVE) == 0u || ((uint32_t)item->window.flags & WINDOW_DECORATION) != 0u ||
            !Rect_ContainsPoint(&item->window.rect, x, y)) {
            continue;
        }
        if (item->type != ITEM_TYPE_TEXT || item->text == NULL || Rect_ContainsPoint(Item_CorrectedTextRect(item), x, y)) {
            return qtrue;
        }
    }
    return qfalse;
}
