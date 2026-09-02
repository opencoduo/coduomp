#include "ui_runtime.h"

#include "qcommon/q_string.h"
#include "ui_menu_globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    MENU_NAME_COMPARE_LIMIT = 99999
};

/*
 * The authoritative Windows cgame/UI pairs below are instruction-identical
 * after rebasing the image-local string and Q-string call addresses:
 *
 *                                   cgame       UI
 * Menu_ItemsMatchingGroup           0x30051180  0x40012ca0
 * Menu_GetMatchingItemByNumber      0x300512a0  0x40012dc0
 * Menu_FindItemByName               0x30051520  0x40013040
 * Menu_GetFocusedItem               0x300587d0  0x4001a340
 * Menus_MenuIsInStack               0x300518b0  0x400133d0
 * Menus_FindByName                  0x300518e0  0x40013400
 * Menu_GetFocused                   0x30058810  0x4001a380
 * Menus_AnyFullScreenVisible        0x30058980  0x4001a4f0
 * Menu_GetAtPoint                   0x3005b120  0x4001ce40
 *
 * Mac cgame and UI traceback tables independently retain the four menu-local
 * names plus Menus_MenuIsInStack, Menus_FindByName, and Menu_GetFocused; UI's
 * table also retains Menus_AnyFullScreenVisible.  The exact Windows pair and
 * the UI symbol resolve Menu_GetAtPoint.  The semantic menu/name argument order
 * follows those retained source interfaces; the cgame reconstruction's name-
 * first spelling merely exposed one compiler-selected register assignment as
 * source order.  The wildcard literals are at cgame 0x3007baa8 and UI
 * 0x4003568c.
 */

itemDef_t *Menu_FindItemByName(menuDef_t *menu, const char *name)
{
    int32_t itemCount;
    int32_t index;

    if (menu == NULL || name == NULL) {
        return NULL;
    }

    itemCount = menu->itemCount;
    for (index = 0; index < itemCount; ++index) {
        itemDef_t *item = menu->items[index];

        if (item->window.name != NULL && Q_stricmpn(name, item->window.name, MENU_NAME_COMPARE_LIMIT) == 0) {
            return menu->items[index];
        }
    }
    return NULL;
}

int32_t Menu_ItemsMatchingGroup(menuDef_t *menu, const char *name)
{
    const char *wildcard = strstr(name, "*");
    int32_t wildcardOffset = wildcard == NULL ? -1 : (int32_t)(wildcard - name);
    int32_t itemCount = menu->itemCount;
    int32_t index;
    int32_t count = 0;

    for (index = 0; index < itemCount; ++index) {
        itemDef_t *item = menu->items[index];
        qboolean matches;

        if (wildcardOffset >= 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            matches = (item->window.name != NULL && Q_strncmp(item->window.name, name, wildcardOffset) == 0) ||
                      (item->window.group != NULL && Q_strncmp(item->window.group, name, wildcardOffset) == 0);
        } else {
            matches = (item->window.name != NULL && name != NULL && Q_stricmpn(item->window.name, name, MENU_NAME_COMPARE_LIMIT) == 0) ||
                      (item->window.group != NULL && name != NULL && Q_stricmpn(item->window.group, name, MENU_NAME_COMPARE_LIMIT) == 0);
        }
        if (matches) {
            ++count;
        }
    }
    return count;
}

itemDef_t *Menu_GetMatchingItemByNumber(menuDef_t *menu, const char *name, int32_t requestedIndex)
{
    const char *wildcard = strstr(name, "*");
    int32_t wildcardOffset = wildcard == NULL ? -1 : (int32_t)(wildcard - name);
    int32_t itemCount = menu->itemCount;
    int32_t index;
    int32_t matchIndex = 0;

    for (index = 0; index < itemCount; ++index) {
        itemDef_t *item = menu->items[index];
        qboolean matches;

        if (wildcardOffset >= 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            matches = (item->window.name != NULL && Q_strncmp(item->window.name, name, wildcardOffset) == 0) ||
                      (item->window.group != NULL && Q_strncmp(item->window.group, name, wildcardOffset) == 0);
        } else {
            matches = (item->window.name != NULL && name != NULL && Q_stricmpn(item->window.name, name, MENU_NAME_COMPARE_LIMIT) == 0) ||
                      (item->window.group != NULL && name != NULL && Q_stricmpn(item->window.group, name, MENU_NAME_COMPARE_LIMIT) == 0);
        }
        if (!matches) {
            continue;
        }
        if (matchIndex == requestedIndex) {
            return menu->items[index];
        }
        ++matchIndex;
    }
    return NULL;
}

itemDef_t *Menu_GetFocusedItem(menuDef_t *menu)
{
    int32_t itemCount;
    int32_t index;

    if (menu == NULL) {
        return NULL;
    }

    itemCount = menu->itemCount;
    for (index = 0; index < itemCount; ++index) {
        if ((menu->items[index]->window.flags & WINDOW_HASFOCUS) != 0) {
            return menu->items[index];
        }
    }
    return NULL;
}

qboolean Menus_MenuIsInStack(menuDef_t *menu)
{
    int32_t index = openMenuCount - 1;

    while (index >= 0) {
        if (menuStack[index] == menu) {
            return qtrue;
        }
        --index;
    }
    return qfalse;
}

menuDef_t *Menus_FindByName(const char *name)
{
    const int32_t count = menuCount;
    int32_t index;

    for (index = 0; index < count; ++index) {
        if (Menus[index].window.name != NULL && name != NULL && Q_stricmpn(Menus[index].window.name, name, MENU_NAME_COMPARE_LIMIT) == 0) {
            return &Menus[index];
        }
    }
    return NULL;
}

menuDef_t *Menu_GetFocused(void)
{
    int32_t index = openMenuCount - 1;

    while (index >= 0) {
        int32_t flags = menuStack[index]->window.flags;

        if ((flags & WINDOW_HASFOCUS) != 0 && (flags & WINDOW_VISIBLE) != 0) {
            return menuStack[index];
        }
        --index;
    }
    return NULL;
}

qboolean Menus_AnyFullScreenVisible(void)
{
    int32_t index = openMenuCount - 1;

    while (index >= 0) {
        menuDef_t *menu = menuStack[index];

        if ((menu->window.flags & WINDOW_VISIBLE) != 0 && menu->fullScreen != 0) {
            return qtrue;
        }
        --index;
    }
    return qfalse;
}

menuDef_t *Menu_GetAtPoint(int32_t x, int32_t y)
{
    const float pointX = (float)x;
    const float pointY = (float)y;
    int32_t index = openMenuCount - 1;

    while (index >= 0) {
        menuDef_t *menu = menuStack[index];

        if (menu != NULL && pointX >= menu->window.rect.x && pointX <= menu->window.rect.x + menu->window.rect.w &&
            pointY >= menu->window.rect.y && pointY <= menu->window.rect.y + menu->window.rect.h) {
            return menu;
        }
        --index;
    }
    return NULL;
}
