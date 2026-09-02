#include "ui_runtime.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * The authoritative Windows cgame/UI pairs are instruction-identical after
 * rebasing the image-local call and scratch-record addresses:
 *
 *                              cgame       UI
 * Item_SetScreenCoords         0x30050f30  0x40012a50
 * Item_UpdatePosition          0x30050f80  0x40012aa0
 * Menu_UpdatePosition          0x30050fd0  0x40012af0
 * Item_CorrectedTextRect       0x30055020  0x40016b80
 * ToWindowCoords              0x300555d0  0x40017130
 * Rect_ToWindowCoords         0x30055600  0x40017160
 *
 * The supporting Mac cgame and UI traceback tables independently retain all
 * four source names.  The geometry functions intentionally preserve the
 * asymmetric x87 stores visible in both PE32 modules.
 */

void Item_SetScreenCoords(itemDef_t *item, float x, float y)
{
    long double effectiveY;

    if (item == NULL) {
        return;
    }

    effectiveY = y;
    if (item->window.border != 0) {
        /* Both originals spill x + borderSize to binary32, but retain the
         * corresponding y sum until rectClient.y has also been added. */
        x += item->window.borderSize;
        effectiveY = (long double)y + item->window.borderSize;
    }

    item->window.rect.w = item->window.rectClient.w;
    item->window.rect.h = item->window.rectClient.h;
    item->window.rect.x = x + item->window.rectClient.x;
    item->window.rect.y = (float)(effectiveY + item->window.rectClient.y);
    item->textRect.w = 0.0f;
    item->textRect.h = 0.0f;
}

void Item_UpdatePosition(itemDef_t *item)
{
    menuDef_t *menu;
    float x;
    float y;

    if (item == NULL || item->parent == NULL) {
        return;
    }

    menu = item->parent;
    x = menu->window.rect.x;
    y = menu->window.rect.y;
    if (menu->window.border != 0) {
        x += menu->window.borderSize;
        y += menu->window.borderSize;
    }
    Item_SetScreenCoords(item, x, y);
}

void Menu_UpdatePosition(menuDef_t *menu)
{
    long double menuX;
    long double menuY;
    int32_t index;

    if (menu == NULL) {
        return;
    }

    menuX = menu->window.rect.x;
    menuY = menu->window.rect.y;
    if (menu->window.border != 0) {
        menuX += menu->window.borderSize;
        menuY += menu->window.borderSize;
    }

    for (index = 0; index < menu->itemCount; ++index) {
        itemDef_t *item = menu->items[index];
        long double itemX;
        float itemY;

        if (item == NULL) {
            continue;
        }

        itemX = menuX;
        /* Both originals round Y once per item while retaining X in the x87
         * stack across the item-border adjustment. */
        itemY = (float)menuY;
        if (item->window.border != 0) {
            itemX += item->window.borderSize;
            itemY = (float)(menuY + item->window.borderSize);
        }

        item->window.rect.x = (float)(itemX + item->window.rectClient.x);
        item->window.rect.w = item->window.rectClient.w;
        item->window.rect.h = item->window.rectClient.h;
        item->textRect.w = 0.0f;
        item->textRect.h = 0.0f;
        item->window.rect.y = (float)((long double)itemY + item->window.rectClient.y);
    }
}

rectDef_t *Item_CorrectedTextRect(itemDef_t *item)
{
    /* Each original module owns one 16-byte static scratch rectangle
     * (cgame 0x30133c18, UI 0x401c35e8).  Every caller consumes the returned
     * pointer immediately; no caller retains or publishes it. */
    static rectDef_t correctedTextRect;

    correctedTextRect.x = 0.0f;
    correctedTextRect.y = 0.0f;
    correctedTextRect.w = 0.0f;
    correctedTextRect.h = 0.0f;
    if (item != NULL) {
        uint32_t heightBits;
        qboolean hasWidth;

        memcpy(&correctedTextRect.x, &item->textRect.x, sizeof(uint32_t));
        memcpy(&correctedTextRect.y, &item->textRect.y, sizeof(uint32_t));
        memcpy(&correctedTextRect.w, &item->textRect.w, sizeof(uint32_t));
        memcpy(&heightBits, &item->textRect.h, sizeof(heightBits));
        hasWidth = correctedTextRect.w != 0.0f;
        memcpy(&correctedTextRect.h, &heightBits, sizeof(heightBits));
        if (hasWidth) {
            correctedTextRect.y -= correctedTextRect.h;
        }
    }
    return &correctedTextRect;
}

void ToWindowCoords(float *x, float *y, const windowDef_t *window)
{
    if (window->border != 0) {
        *x += window->borderSize;
        *y += window->borderSize;
    }
    *x += window->rect.x;
    *y = window->rect.y + *y;
}

void Rect_ToWindowCoords(rectDef_t *rect, const windowDef_t *window)
{
    if (window->border != 0) {
        rect->x = window->borderSize + rect->x;
        rect->y += window->borderSize;
    }
    rect->x += window->rect.x;
    rect->y = window->rect.y + rect->y;
}
