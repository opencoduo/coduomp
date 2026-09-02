#include "ui_runtime.h"
#include "ui_menu_globals.h"
#include "ui_parse.h"
#include "compat/coduo_int32_bits.h"

#include <stddef.h>
#include <stdint.h>

qboolean Item_Bind_HandleKey(itemDef_t *item, int32_t key, qboolean down);
qboolean Item_TextField_HandleKey(itemDef_t *item, int32_t key);
qboolean Menu_OverActiveItem(menuDef_t *menu, float x, float y);
/*
 * The authoritative Windows cgame/UI bodies are instruction twins after
 * rebasing their calls, globals, strings, and switch-table addresses:
 *
 *                          cgame       UI
 * Menus_HandleOOBClick     0x30054ec0  0x40016a20
 * Menu_HandleKey           0x300550a0  0x40016c00
 *
 * Each pair also has the same byte extent (0x155 and 0x440 respectively), and
 * the supporting Mac modules retain both exact source names.
 */

void Menus_HandleOOBClick(menuDef_t *menu, int32_t key, qboolean down)
{
    displayContextDef_t *display;
    uint32_t lastIndexBits;
    uint32_t indexBits;
    uint32_t visibleCountBits = 0u;

    if (menu == NULL) {
        return;
    }
    if (down && ((uint32_t)menu->window.flags & WINDOW_OOB_CLICK) != 0u) {
        Menu_RunCloseScript(menu);
        menu->window.flags &= ~(int32_t)(WINDOW_HASFOCUS | WINDOW_VISIBLE);
    }

    /* Close scripts can mutate either the display context or open-menu stack.
     * Preserve the original snapshot/reload points around those callbacks. */
    display = DC;
    lastIndexBits = (uint32_t)openMenuCount - 1u;
    indexBits = lastIndexBits;
    while (coduo_int32_from_bits(indexBits) >= 0) {
        int32_t cursorY = display->cursory;
        menuDef_t *candidate = menuStack[indexBits];
        int32_t cursorX = display->cursorx;

        if (Menu_OverActiveItem(candidate, (float)cursorX, (float)cursorY)) {
            uint32_t clearIndexBits = lastIndexBits;

            while (coduo_int32_from_bits(clearIndexBits) >= 0) {
                menuStack[clearIndexBits]->window.flags &= ~(int32_t)WINDOW_HASFOCUS;
                clearIndexBits -= 1u;
            }
            menuStack[indexBits]->window.flags |= (int32_t)(WINDOW_HASFOCUS | WINDOW_VISIBLE);

            cursorY = display->cursory;
            cursorX = display->cursorx;
            Display_MouseMove(NULL, cursorX, cursorY);
            {
                displayContextDef_t *movementDisplay = DC;
                int32_t movementY = movementDisplay->cursory;
                menuDef_t *movementMenu = menuStack[indexBits];
                int32_t movementX = movementDisplay->cursorx;

                Menu_HandleMouseMove(movementMenu, (float)movementX, (float)movementY);
            }
            Menu_HandleKey(menuStack[indexBits], key, down);
            display = DC;
            break;
        }
        indexBits -= 1u;
    }

    {
        int32_t remaining = menuCount;
        menuDef_t *registeredMenu = &Menus[0];

        while (remaining > 0) {
            if (((uint32_t)registeredMenu->window.flags & WINDOW_MOUSE_ACTIVE) != 0u) {
                visibleCountBits += 1u;
            }
            ++registeredMenu;
            remaining = coduo_int32_from_bits((uint32_t)remaining - 1u);
        }
    }
    if (visibleCountBits == 0u) {
        ui_pause_t pause = display->pause;

        if (pause != NULL) {
            pause(qfalse);
        }
    }

    indexBits = (uint32_t)openMenuCount - 1u;
    while (coduo_int32_from_bits(indexBits) >= 0) {
        Menu_CloseCinematics(menuStack[indexBits]);
        indexBits -= 1u;
    }
}

void Menu_HandleKey(menuDef_t *menu, int32_t key, qboolean down)
{
    itemDef_t *focusItem = NULL;
    displayContextDef_t *display;
    int32_t cursorX;
    int32_t cursorY;

    display = DC;
    cursorY = display->cursory;
    cursorX = display->cursorx;
    Menu_HandleMouseMove(menu, (float)cursorX, (float)cursorY);

    if (g_waitingForKey && down) {
        Item_Bind_HandleKey(g_bindItem, key, down);
        return;
    }
    if (g_editingField && down) {
        itemDef_t *editItem = g_editItem;

        if (!Item_TextField_HandleKey(editItem, key)) {
            g_editingField = qfalse;
            g_editItem = NULL;
            return;
        }
        if (key != K_MOUSE1 && key != K_MOUSE2 && key != K_MOUSE3) {
            return;
        }
        g_editingField = qfalse;
        g_editItem = NULL;
        display = DC;
        cursorY = display->cursory;
        cursorX = display->cursorx;
        Display_MouseMove(NULL, cursorX, cursorY);
    }

    if (menu == NULL) {
        return;
    }
    if (down && ((uint32_t)menu->window.flags & WINDOW_MODAL) == 0u &&
        (display = DC, cursorY = display->cursory, cursorX = display->cursorx,
         !Rect_ContainsPoint(&menu->window.rect, (float)cursorX, (float)cursorY)) &&
        !inHandleKey && (key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3)) {
        inHandleKey = qtrue;
        Menus_HandleOOBClick(menu, key, down);
        inHandleKey = qfalse;
        return;
    }

    {
        int32_t remaining = menu->itemCount;
        itemDef_t **itemSlot = &menu->items[0];

        while (remaining > 0) {
            itemDef_t *item = *itemSlot;

            if (((uint32_t)item->window.flags & WINDOW_HASFOCUS) != 0u) {
                focusItem = item;
            }
            ++itemSlot;
            remaining = coduo_int32_from_bits((uint32_t)remaining - 1u);
        }
    }
    if (focusItem != NULL && Item_HandleKey(focusItem, key, down)) {
        const char *action = focusItem->action;

        Item_RunScript(focusItem, action);
        return;
    }
    if (!down) {
        return;
    }

    if (key > 0 && key < MAX_KEYS) {
        const char *keyScript = menu->onKey[key];

        if (keyScript != NULL) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            itemDef_t scriptItem = {0};

            scriptItem.parent = menu;
            Item_RunScript(&scriptItem, keyScript);
            return;
        }
    }
    {
        const char *fallbackScript = menu->onKey[MAX_KEYS - 1];

        if (fallbackScript != NULL) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            itemDef_t scriptItem = {0};

            scriptItem.parent = menu;
            Item_RunScript(&scriptItem, fallbackScript);
        }
    }

    switch (key) {
    case K_TAB:
    case K_DOWNARROW:
    case K_RIGHTARROW:
    case K_KP_DOWNARROW:
    case K_MWHEELDOWN:
        Menu_SetNextCursorItem(menu);
        return;

    case K_ENTER:
    case K_KP_ENTER:
    case K_MOUSE3:
        if (focusItem != NULL) {
            if (focusItem->type == ITEM_TYPE_EDITFIELD || focusItem->type == ITEM_TYPE_NUMERICFIELD ||
                focusItem->type == ITEM_TYPE_UPREDITFIELD) {
                display = DC;
                focusItem->cursorPos = 0;
                g_editingField = qtrue;
                g_editItem = focusItem;
                display->setOverstrikeMode(qtrue);
            } else {
                Item_Action(focusItem);
            }
        }
        return;

    case K_ESCAPE:
        if (!g_waitingForKey) {
            const char *escapeScript = menu->onESC;

            if (escapeScript != NULL) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                itemDef_t scriptItem = {0};

                scriptItem.parent = menu;
                Item_RunScript(&scriptItem, escapeScript);
            }
        }
        return;

    case K_UPARROW:
    case K_LEFTARROW:
    case K_KP_UPARROW:
    case K_MWHEELUP:
        Menu_SetPrevCursorItem(menu);
        return;

    case K_F11:
        display = DC;
        if (display->getCVarValue("developer") != 0.0f) {
            debugMode ^= 1;
        }
        return;

    case K_F12:
        display = DC;
        if (display->getCVarValue("developer") != 0.0f) {
            display = DC;
            display->executeText(EXEC_APPEND, "screenshot\n");
        }
        return;

    case K_MOUSE1:
    case K_MOUSE2:
        if (focusItem == NULL) {
            return;
        }
        if (focusItem->type == ITEM_TYPE_TEXT) {
            rectDef_t *correctedRect;

            display = DC;
            cursorY = display->cursory;
            cursorX = display->cursorx;
            correctedRect = Item_CorrectedTextRect(focusItem);
            if (Rect_ContainsPoint(correctedRect, (float)cursorX, (float)cursorY)) {
                Item_Action(focusItem);
            }
        } else if (focusItem->type == ITEM_TYPE_EDITFIELD || focusItem->type == ITEM_TYPE_NUMERICFIELD ||
                   focusItem->type == ITEM_TYPE_UPREDITFIELD) {
            display = DC;
            cursorY = display->cursory;
            cursorX = display->cursorx;
            if (Rect_ContainsPoint(&focusItem->window.rect, (float)cursorX, (float)cursorY)) {
                editFieldDef_t *edit = Item_GetEditFieldDef(focusItem);

                if (edit != NULL) {
                    edit->paintOffset = 0;
                }
                display = DC;
                focusItem->cursorPos = 0;
                g_editingField = qtrue;
                g_editItem = focusItem;
                display->setOverstrikeMode(qtrue);
            }
        } else {
            display = DC;
            cursorY = display->cursory;
            cursorX = display->cursorx;
            if (Rect_ContainsPoint(&focusItem->window.rect, (float)cursorX, (float)cursorY)) {
                Item_Action(focusItem);
            }
        }
        return;

    default:
        return;
    }
}
