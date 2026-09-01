#include "ui_runtime.h"

#include "ui_parse.h"

#include <stddef.h>
#include <stdint.h>

extern displayContextDef_t *DC;

/*
 * These authoritative Windows cgame/UI pairs are instruction-identical after
 * rebasing image-local globals and calls:
 *
 *                              cgame       UI
 * Menu_ShowItemByName          0x30051710  0x40013230
 * Menu_FadeItemByName          0x30051790  0x400132b0
 * Script_Show                  0x30051a60  0x40013580
 * Script_Hide                  0x30051a90  0x400135b0
 * Script_FadeOut               0x30051af0  0x40013640
 *
 * Both Mac modules retain all five source names.  The semantic helper argument
 * order is (menu, name, operation); cgame's former name-first declaration only
 * exposed the register allocation selected for its PE32 body.
 *
 * Script_FadeIn deliberately remains module-local.  The cgame PE32 body at
 * 0x30051ac0 calls Menu_FadeItemByName, whereas UI 0x400135e0 contains the
 * fade-in loop inline.  Their observable transformation agrees, but they fail
 * the same-target instruction-identity requirement for shared functions.
 */

void Menu_ShowItemByName(menuDef_t *menu, const char *name, qboolean show)
{
    const int32_t count = Menu_ItemsMatchingGroup(menu, name);
    int32_t index;

    for (index = 0; index < count; ++index) {
        itemDef_t *item =
            Menu_GetMatchingItemByNumber(menu, name, index);

        if (item == NULL) {
            continue;
        }
        if (show) {
            item->window.flags |= WINDOW_VISIBLE;
        } else {
            item->window.flags &= ~WINDOW_VISIBLE;
            if (item->window.cinematic >= 0) {
                int32_t cinematic = item->window.cinematic;
                displayContextDef_t *display = DC;

                display->stopCinematic(cinematic);
                item->window.cinematic = -1;
            }
        }
    }
}

void Menu_FadeItemByName(menuDef_t *menu, const char *name,
                         qboolean fadeOut)
{
    const int32_t count = Menu_ItemsMatchingGroup(menu, name);
    int32_t index;

    for (index = 0; index < count; ++index) {
        itemDef_t *item =
            Menu_GetMatchingItemByNumber(menu, name, index);

        if (item == NULL) {
            continue;
        }
        if (fadeOut) {
            item->window.flags =
                (item->window.flags & ~WINDOW_FADINGIN) |
                WINDOW_FADINGOUT | WINDOW_VISIBLE;
        } else {
            item->window.flags =
                (item->window.flags & ~WINDOW_FADINGOUT) |
                WINDOW_FADINGIN | WINDOW_VISIBLE;
        }
    }
}

void Script_Show(itemDef_t *item, char **arguments)
{
    const char *name;

    if (String_Parse(arguments, &name)) {
        Menu_ShowItemByName(item->parent, name, qtrue);
    }
}

void Script_Hide(itemDef_t *item, char **arguments)
{
    const char *name;

    if (String_Parse(arguments, &name)) {
        Menu_ShowItemByName(item->parent, name, qfalse);
    }
}

void Script_FadeOut(itemDef_t *item, char **arguments)
{
    const char *name;

    if (String_Parse(arguments, &name)) {
        Menu_FadeItemByName(item->parent, name, qtrue);
    }
}
