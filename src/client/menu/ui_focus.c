#include "ui_runtime.h"
#include "compat/coduo_int32_bits.h"
#include "ui_parse.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern displayContextDef_t *DC;
extern itemDef_t *captureItem;
extern int32_t g_waitingForKey;
extern int32_t g_editingField;

/*
 * The authoritative Windows cgame/UI bodies are instruction-identical after
 * rebasing their module-local calls and display-context global:
 *
 *                              cgame       UI
 * Menu_ClearFocus              0x300510a0  0x40012bc0
 * Script_SetFocus              0x300521b0  0x40013d00
 * Item_SetFocus                0x30052920  0x40014470
 * Menu_GetItemUnderCursor      0x300533d0  0x40014f20
 * Item_SetMouseOver            0x30053440  0x40014f90
 * Item_MouseEnter              0x30053260  0x40014db0
 * Item_MouseLeave              0x30053390  0x40014ee0
 * Menu_SetPrevCursorItem       0x30054ac0  0x40016620
 * Menu_SetNextCursorItem       0x30054ba0  0x40016700
 * Menu_HandleMouseMove         0x30058a10  0x4001a580
 *
 * Both Mac modules retain Menu_ClearFocus and Item_SetMouseOver.  The latter
 * resolves cgame's reconstruction-local Window_SetFlag1 spelling: the original
 * interface takes an itemDef_t, whose leading window owns the +0x48 flag word.
 * The exact PE32 pair and UI interface resolve Menu_GetItemUnderCursor.
 */

itemDef_t *Menu_ClearFocus(menuDef_t *menu)
{
    itemDef_t *focused = NULL;
    int32_t index;

    if (menu == NULL) {
        return NULL;
    }
    for (index = 0; index < menu->itemCount; ++index) {
        itemDef_t *item = menu->items[index];

        if ((item->window.flags & WINDOW_HASFOCUS) != 0) {
            focused = item;
        }
        item->window.flags &= ~WINDOW_HASFOCUS;
        if (item->leaveFocus != NULL) {
            Item_RunScript(item, item->leaveFocus);
        }
    }
    return focused;
}

void Script_SetFocus(itemDef_t *item, char **arguments)
{
    const char *name;
    menuDef_t *menu;
    itemDef_t *target;

    if (!String_Parse(arguments, &name)) {
        return;
    }
    menu = item->parent;
    target = Menu_FindItemByName(menu, name);
    if (target == NULL ||
        (target->window.flags & (WINDOW_HASFOCUS | WINDOW_DECORATION)) != 0) {
        return;
    }

    Menu_ClearFocus(menu);
    target->window.flags |= WINDOW_HASFOCUS;
    if (target->onFocus != NULL) {
        Item_RunScript(target, target->onFocus);
    }
    if (DC->itemFocusSound != NULL) {
        DC->startLocalSound(DC->itemFocusSound);
    }
}

qboolean Item_SetFocus(itemDef_t *item, float x, float y)
{
    menuDef_t *menu;
    itemDef_t *previous;
    const char *focusSound;
    const char *onFocus;
    int32_t index;

    /* Both originals snapshot the default sound before even testing item. */
    focusSound = DC->itemFocusSound;
    if (item == NULL ||
        (item->window.flags & (WINDOW_HASFOCUS | WINDOW_DECORATION)) != 0 ||
        (item->window.flags & WINDOW_VISIBLE) == 0) {
        return qfalse;
    }

    /* This parent read precedes both cvar callbacks in the original bodies. */
    menu = item->parent;
    if ((item->cvarFlags & ITEM_CVAR_ENABLE_MASK) != 0 &&
        !Item_EnableShowViaCvar(item, ITEM_CVAR_ENABLE)) {
        return qfalse;
    }
    if ((item->cvarFlags & ITEM_CVAR_SHOW_MASK) != 0 &&
        !Item_EnableShowViaCvar(item, ITEM_CVAR_SHOW)) {
        return qfalse;
    }

    previous = Menu_ClearFocus(menu);
    if (item->type == ITEM_TYPE_TEXT) {
        rectDef_t hit;

        /* The originals copy x/w/h as integer words and evaluate only the y
         * subtraction in x87, preserving their binary32 payload behavior. */
        memcpy(&hit.x, &item->textRect.x, sizeof(hit.x));
        hit.y = item->textRect.y - item->textRect.h;
        memcpy(&hit.w, &item->textRect.w, sizeof(hit.w));
        memcpy(&hit.h, &item->textRect.h, sizeof(hit.h));
        if (Rect_ContainsPoint(&hit, x, y)) {
            item->window.flags |= WINDOW_HASFOCUS;
        } else {
            if (previous != NULL) {
                /* Both original bodies fetch the script before publishing the
                 * restored focus flag. */
                onFocus = previous->onFocus;
                previous->window.flags |= WINDOW_HASFOCUS;
                if (onFocus != NULL) {
                    Item_RunScript(previous, onFocus);
                }
            }
            goto publish_cursor;
        }
    } else {
        onFocus = item->onFocus;
        item->window.flags |= WINDOW_HASFOCUS;
        if (onFocus != NULL) {
            Item_RunScript(item, onFocus);
        }
    }

    if (item->focusSound != NULL) {
        focusSound = item->focusSound;
    }
    if (focusSound != NULL) {
        DC->startLocalSound(focusSound);
    }

publish_cursor:
    for (index = 0; index < menu->itemCount; ++index) {
        if (menu->items[index] == item) {
            menu->cursorItem = index;
            break;
        }
    }
    return qtrue;
}

itemDef_t *Menu_GetItemUnderCursor(menuDef_t *menu, float x, float y)
{
    const int32_t itemCount = menu->itemCount;
    int32_t index;

    for (index = 0; index < itemCount; ++index) {
        itemDef_t *item = menu->items[index];

        /* Positive ordered comparisons preserve the paired x87 NaN rejection. */
        if (item != NULL && x >= item->window.rect.x &&
            x <= item->window.rect.x + item->window.rect.w &&
            y >= item->window.rect.y &&
            y <= item->window.rect.y + item->window.rect.h) {
            return item;
        }
    }
    return NULL;
}

void Item_SetMouseOver(itemDef_t *item, qboolean mouseOver)
{
    if (item == NULL) {
        return;
    }
    if (mouseOver) {
        item->window.flags |= WINDOW_MOUSEOVER;
    } else {
        item->window.flags &= ~WINDOW_MOUSEOVER;
    }
}

void Item_MouseEnter(itemDef_t *item, float x, float y)
{
    rectDef_t textBox;
    uint32_t flags;

    if (item == NULL) {
        return;
    }

    /* Both PE32 bodies copy the unchanged lanes as words and perform only the
     * baseline-minus-height operation in x87. */
    memcpy(&textBox, &item->textRect, sizeof(textBox));
    textBox.y = textBox.y - textBox.h;

    if ((item->cvarFlags & ITEM_CVAR_ENABLE_MASK) != 0 &&
        !Item_EnableShowViaCvar(item, ITEM_CVAR_ENABLE)) {
        return;
    }
    if ((item->cvarFlags & ITEM_CVAR_SHOW_MASK) != 0 &&
        !Item_EnableShowViaCvar(item, ITEM_CVAR_SHOW)) {
        return;
    }

    /* The original bodies snapshot flags before the hit test result branches. */
    flags = (uint32_t)item->window.flags;
    if (Rect_ContainsPoint(&textBox, x, y)) {
        if ((flags & WINDOW_MOUSEOVERTEXT) == 0) {
            Item_RunScript(item, item->mouseEnterText);
            item->window.flags = (int32_t)(
                (uint32_t)item->window.flags | WINDOW_MOUSEOVERTEXT);
        }
        if ((item->window.flags & WINDOW_MOUSEOVER) == 0) {
            Item_RunScript(item, item->mouseEnter);
            item->window.flags = (int32_t)(
                (uint32_t)item->window.flags | WINDOW_MOUSEOVER);
        }
        return;
    }

    if ((flags & WINDOW_MOUSEOVERTEXT) != 0) {
        Item_RunScript(item, item->mouseExitText);
        item->window.flags = (int32_t)(
            (uint32_t)item->window.flags & ~WINDOW_MOUSEOVERTEXT);
    }
    if ((item->window.flags & WINDOW_MOUSEOVER) == 0) {
        Item_RunScript(item, item->mouseEnter);
        item->window.flags = (int32_t)(
            (uint32_t)item->window.flags | WINDOW_MOUSEOVER);
    }
    if (item->type == ITEM_TYPE_LISTBOX) {
        Item_ListBox_MouseEnter(item, x, y);
    }
}

void Item_MouseLeave(itemDef_t *item)
{
    if (item == NULL) {
        return;
    }
    if ((item->window.flags & WINDOW_MOUSEOVERTEXT) != 0) {
        Item_RunScript(item, item->mouseExitText);
        item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
    }
    Item_RunScript(item, item->mouseExit);
    item->window.flags &= ~(WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW);
}

qboolean Menu_HandleMouseMove(menuDef_t *menu, float x, float y)
{
    itemDef_t *focusItem = NULL;
    qboolean action = qfalse;
    int32_t pass;

    if (menu == NULL ||
        (menu->window.flags & WINDOW_MOUSE_ACTIVE) == 0 ||
        captureItem != NULL || g_waitingForKey || g_editingField) {
        return qfalse;
    }

    for (pass = 0; pass < 2; ++pass) {
        uint32_t indexBits = (uint32_t)menu->itemCount - 1u;

        for (; coduo_int32_from_bits(indexBits) >= 0; indexBits -= 1u) {
            itemDef_t *item = menu->items[indexBits];
            int32_t flags;

            if ((item->window.flags & WINDOW_MOUSE_ACTIVE) == 0) {
                continue;
            }
            if ((item->cvarFlags & ITEM_CVAR_ENABLE_MASK) != 0 &&
                !Item_EnableShowViaCvar(item, ITEM_CVAR_ENABLE)) {
                continue;
            }

            /* Each cvar callback can replace the menu slot. Both binaries
             * therefore re-read it before the next gate and after that gate. */
            item = menu->items[indexBits];
            if ((item->cvarFlags & ITEM_CVAR_SHOW_MASK) != 0 &&
                !Item_EnableShowViaCvar(item, ITEM_CVAR_SHOW)) {
                continue;
            }
            item = menu->items[indexBits];
            flags = item->window.flags;
            if ((flags & WINDOW_HASFOCUS) != 0 && focusItem == NULL) {
                focusItem = item;
            }

            if (Rect_ContainsPoint(&item->window.rect, x, y)) {
                if (pass != 1) {
                    continue;
                }
                if (item->type == ITEM_TYPE_TEXT && item->text != NULL &&
                    !Rect_ContainsPoint(Item_CorrectedTextRect(item), x, y)) {
                    continue;
                }
                if ((item->window.flags & WINDOW_VISIBLE) == 0 ||
                    (item->window.flags & WINDOW_FADINGOUT) != 0) {
                    continue;
                }
                Item_MouseEnter(item, x, y);
                if (action) {
                    continue;
                }
                action = Item_SetFocus(item, x, y);
                if (action) {
                    focusItem = item;
                }
            } else if ((flags & WINDOW_MOUSEOVER) != 0) {
                itemDef_t *current;

                Item_MouseLeave(item);
                current = menu->items[indexBits];
                if (current != NULL) {
                    current->window.flags &= ~WINDOW_MOUSEOVER;
                }
            }
        }
    }

    if (action) {
        return qtrue;
    }
    if (focusItem != NULL &&
        !Rect_ContainsPoint(&focusItem->window.rect, x, y)) {
        Menu_ClearFocus(menu);
    }
    return qfalse;
}

itemDef_t *Menu_SetPrevCursorItem(menuDef_t *menu)
{
    const int32_t original = menu->cursorItem;
    qboolean wrapped = qfalse;

    if (menu->cursorItem < 0) {
        menu->cursorItem = coduo_int32_from_bits(
            (uint32_t)menu->itemCount - 1u);
        wrapped = qtrue;
    }
    if (menu->cursorItem <= -1) {
        menu->cursorItem = original;
        return NULL;
    }

    while (menu->cursorItem > -1) {
        displayContextDef_t *display;
        itemDef_t *item;
        int32_t cursorY;
        int32_t cursorX;

        menu->cursorItem = coduo_int32_from_bits(
            (uint32_t)menu->cursorItem - 1u);
        if (menu->cursorItem < 0 && !wrapped) {
            menu->cursorItem = coduo_int32_from_bits(
                (uint32_t)menu->itemCount - 1u);
            wrapped = qtrue;
        }
        if (menu->cursorItem < 0) {
            break;
        }

        /* The paired bodies read y, the candidate, then x before focusing. */
        display = DC;
        cursorY = display->cursory;
        item = menu->items[menu->cursorItem];
        cursorX = display->cursorx;
        if (Item_SetFocus(item, (float)cursorX, (float)cursorY)) {
            float mouseY;
            float mouseX;

            item = menu->items[menu->cursorItem];
            mouseY = item->window.rect.y + 1.0f;
            mouseX = item->window.rect.x + 1.0f;
            Menu_HandleMouseMove(menu, mouseX, mouseY);
            /* The callback can mutate the menu, so the originals re-read both
             * cursorItem and the selected item for the return value. */
            return menu->items[menu->cursorItem];
        }
    }

    menu->cursorItem = original;
    return NULL;
}

itemDef_t *Menu_SetNextCursorItem(menuDef_t *menu)
{
    const int32_t original = menu->cursorItem;
    qboolean wrapped = qfalse;

    if (menu->cursorItem == -1) {
        menu->cursorItem = 0;
        wrapped = qtrue;
    }
    while (menu->cursorItem < menu->itemCount) {
        displayContextDef_t *display;
        itemDef_t *item;
        int32_t cursorY;
        int32_t cursorX;

        menu->cursorItem = coduo_int32_from_bits(
            (uint32_t)menu->cursorItem + 1u);
        if (menu->cursorItem >= menu->itemCount) {
            if (wrapped) {
                if (original == -1) {
                    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    menu->cursorItem = original;
                    return NULL;
                }
                return menu->items[original];
            }
            wrapped = qtrue;
            menu->cursorItem = 0;
        }

        display = DC;
        cursorY = display->cursory;
        item = menu->items[menu->cursorItem];
        cursorX = display->cursorx;
        if (Item_SetFocus(item, (float)cursorX, (float)cursorY)) {
            float mouseY;
            float mouseX;

            item = menu->items[menu->cursorItem];
            mouseY = item->window.rect.y + 1.0f;
            mouseX = item->window.rect.x + 1.0f;
            Menu_HandleMouseMove(menu, mouseX, mouseY);
            return menu->items[menu->cursorItem];
        }
    }

    menu->cursorItem = original;
    return NULL;
}
