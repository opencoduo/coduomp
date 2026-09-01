#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <stddef.h>
#include <stdint.h>

void Com_Error(errorParm_t level, const char *format, ...);
extern displayContextDef_t *DC;

/*
 * The authoritative Windows cgame/UI bodies in this menu-lifecycle cluster are
 * instruction-identical after rebasing image-local globals, strings, and calls:
 *
 *                                   cgame       UI
 * Menu_RunCloseScript               0x30051930  0x40013450
 * Menus_RemoveFromStack             0x300517e0  0x40013300
 * Menus_AddToStack                  0x30051830  0x40013350
 * Menus_Close                       0x30051970  0x40013490
 * Menus_CloseByName                 0x30051a10  0x40013530
 * Menus_CloseAll                    0x30051a30  0x40013550
 * Window_CloseCinematic             0x30054ca0  0x40016800
 * Menu_CloseCinematics              0x30054cd0  0x40016830
 * Display_CloseCinematics           0x30054d70  0x400168d0
 * Menus_Open                        0x30054da0  0x40016900
 * Menus_OpenByName                  0x30054e70  0x400169d0
 * Menus_VisibleCount                0x30054e90  0x400169f0
 *
 * The Mac cgame and UI traceback tables independently retain the canonical
 * source names for the shared cluster.  In particular, cgame's reconstructed
 * Menus_Activate spelling was wrong: both Mac modules name the exact stack
 * operation Menus_AddToStack.
 */

void Menu_RunCloseScript(menuDef_t *menu)
{
    if (menu != NULL &&
        (menu->window.flags & WINDOW_VISIBLE) != 0 &&
        menu->onClose != NULL) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        itemDef_t item = {0};

        item.parent = menu;
        Item_RunScript(&item, menu->onClose);
    }
}

qboolean Menus_RemoveFromStack(menuDef_t *menu)
{
    const uint32_t lastIndexBits = (uint32_t)openMenuCount - 1u;
    uint32_t indexBits = lastIndexBits;

    while (coduo_int32_from_bits(indexBits) >= 0 &&
           menuStack[indexBits] != menu) {
        indexBits -= 1u;
    }
    if (coduo_int32_from_bits(indexBits) < 0) {
        return qfalse;
    }

    openMenuCount = coduo_int32_from_bits(lastIndexBits);
    while (coduo_int32_from_bits(indexBits) <
           coduo_int32_from_bits(lastIndexBits)) {
        menuStack[indexBits] = menuStack[indexBits + 1u];
        indexBits += 1u;
    }
    return qtrue;
}

void Menus_AddToStack(menuDef_t *menu)
{
    const uint32_t countBits = (uint32_t)openMenuCount;
    uint32_t appendIndexBits = countBits;
    uint32_t indexBits = countBits - 1u;

    if (coduo_int32_from_bits(indexBits) >= 0) {
        while (coduo_int32_from_bits(indexBits) >= 0 &&
               menuStack[indexBits] != menu) {
            indexBits -= 1u;
        }
        if (coduo_int32_from_bits(indexBits) >= 0) {
            appendIndexBits = countBits - 1u;
            openMenuCount = coduo_int32_from_bits(appendIndexBits);
            while (coduo_int32_from_bits(indexBits) <
                   coduo_int32_from_bits(appendIndexBits)) {
                menuStack[indexBits] = menuStack[indexBits + 1u];
                indexBits += 1u;
            }
        }
    }

    if (appendIndexBits == MAX_OPEN_MENUS) {
        Com_Error(ERR_DROP, "\x15" "Too many menus opened");
        appendIndexBits = (uint32_t)openMenuCount;
    }
    menuStack[appendIndexBits] = menu;
    openMenuCount = coduo_int32_from_bits(appendIndexBits + 1u);
}

void Menus_Close(menuDef_t *menu)
{
    uint32_t openMenusRemainingBits;
    uint32_t lastIndexBits;
    uint32_t indexBits;

    /* Menus_Close contains the same close-script source operation as
     * Menu_RunCloseScript, inlined in both authoritative PE32 bodies. */
    if (menu != NULL &&
        (menu->window.flags & WINDOW_VISIBLE) != 0 &&
        menu->onClose != NULL) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        itemDef_t item = {0};

        item.parent = menu;
        Item_RunScript(&item, menu->onClose);
    }

    openMenusRemainingBits = (uint32_t)openMenuCount;
    lastIndexBits = openMenusRemainingBits - 1u;
    indexBits = lastIndexBits;
    if (coduo_int32_from_bits(indexBits) >= 0) {
        while (menuStack[indexBits] != menu) {
            indexBits -= 1u;
            if (coduo_int32_from_bits(indexBits) < 0) {
                break;
            }
        }
        if (coduo_int32_from_bits(indexBits) >= 0) {
            openMenusRemainingBits = lastIndexBits;
            openMenuCount = coduo_int32_from_bits(lastIndexBits);
            while (coduo_int32_from_bits(indexBits) <
                   coduo_int32_from_bits(lastIndexBits)) {
                menuStack[indexBits] = menuStack[indexBits + 1u];
                indexBits += 1u;
            }
        }
    }

    if ((menu->window.flags & WINDOW_HASFOCUS) != 0 &&
        openMenusRemainingBits != 0u) {
        menuStack[openMenusRemainingBits - 1u]->window.flags |=
            WINDOW_HASFOCUS;
    }
    menu->window.flags &= ~(WINDOW_VISIBLE | WINDOW_HASFOCUS);
}

void Menus_CloseByName(const char *name)
{
    menuDef_t *menu = Menus_FindByName(name);

    if (menu != NULL) {
        Menus_Close(menu);
    }
}

void Menus_CloseAll(void)
{
    int32_t index = 0;

    while (index < menuCount) {
        Menus_Close(&Menus[index]);
        ++index;
    }
}

void Window_CloseCinematic(windowDef_t *window)
{
    if (window->style == WINDOW_STYLE_CINEMATIC &&
        window->cinematic >= 0) {
        int32_t cinematic = window->cinematic;
        displayContextDef_t *display = DC;

        display->stopCinematic(cinematic);
        window->cinematic = -1;
    }
}

void Menu_CloseCinematics(menuDef_t *menu)
{
    uint32_t indexBits = 0u;

    if (menu == NULL) {
        return;
    }

    /* Window_CloseCinematic is inlined at both of these source sites by both
     * authoritative Windows modules. */
    Window_CloseCinematic(&menu->window);
    while (coduo_int32_from_bits(indexBits) < menu->itemCount) {
        itemDef_t *item = menu->items[indexBits];

        Window_CloseCinematic(&item->window);
        /* Both originals re-read the item-table slot after the stop callback,
         * so callback-visible mutation of menu->items remains observable. */
        item = menu->items[indexBits];
        if (item->type == ITEM_TYPE_OWNERDRAW) {
            uint32_t ownerDrawBits = 0u - (uint32_t)item->window.ownerDraw;

            DC->stopCinematic(coduo_int32_from_bits(ownerDrawBits));
        }
        indexBits += 1u;
    }
}

void Display_CloseCinematics(void)
{
    uint32_t indexBits = (uint32_t)openMenuCount - 1u;

    while (coduo_int32_from_bits(indexBits) >= 0) {
        Menu_CloseCinematics(menuStack[indexBits]);
        indexBits -= 1u;
    }
}

void Menus_Open(menuDef_t *menu)
{
    const char *onOpen;
    const char *soundName;
    displayContextDef_t *display;
    int32_t cursorX;
    int32_t cursorY;
    uint32_t indexBits = (uint32_t)openMenuCount - 1u;

    while (coduo_int32_from_bits(indexBits) >= 0) {
        menuStack[indexBits]->window.flags &= ~WINDOW_HASFOCUS;
        indexBits -= 1u;
    }

    Menus_AddToStack(menu);
    onOpen = menu->onOpen;
    menu->window.flags |= WINDOW_HASFOCUS | WINDOW_VISIBLE;
    if (onOpen != NULL) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        itemDef_t item = {0};

        item.parent = menu;
        Item_RunScript(&item, onOpen);
    }

    soundName = menu->soundName;
    if (soundName != NULL && soundName[0] != '\0') {
        display = DC;
        display->startLocalSound(soundName);
    }

    indexBits = (uint32_t)openMenuCount - 1u;
    while (coduo_int32_from_bits(indexBits) >= 0) {
        Menu_CloseCinematics(menuStack[indexBits]);
        indexBits -= 1u;
    }

    display = DC;
    cursorY = display->cursory;
    cursorX = display->cursorx;
    Display_MouseMove(NULL, cursorX, cursorY);
}

qboolean Menus_OpenByName(const char *name)
{
    menuDef_t *menu = Menus_FindByName(name);

    if (menu == NULL) {
        return qfalse;
    }
    Menus_Open(menu);
    return qtrue;
}

int32_t Menus_VisibleCount(void)
{
    int32_t remaining = menuCount;
    menuDef_t *menu = &Menus[0];
    uint32_t countBits = 0u;

    while (remaining > 0) {
        if ((menu->window.flags & WINDOW_MOUSE_ACTIVE) != 0) {
            countBits += 1u;
        }
        ++menu;
        remaining = coduo_int32_from_bits((uint32_t)remaining - 1u);
    }
    return coduo_int32_from_bits(countBits);
}
