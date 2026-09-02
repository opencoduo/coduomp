#include "ui_runtime.h"
#include "ui_memory.h"
#include "ui_parse.h"

#include "compat/coduo_int32_bits.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

/*
 * This foundation cluster is retained independently in both authoritative
 * Windows client modules.  After rebasing image-local globals, strings, and
 * calls, the paired bodies have identical instruction flow and field access:
 *
 *                              cgame       UI
 * Fade                         0x30050a60  0x40012580
 * Window_Init                  0x30050a30  0x40012550
 * Rect_ContainsPoint           0x30051130  0x40012c50
 * IsVisible                    0x30051110  0x40012c30
 * Item_GetListBoxDef           0x30052aa0  0x400145f0
 * Item_IsEditFieldDef          0x30052ac0  0x40014610
 * Item_GetEditFieldDef         0x30052b00  0x40014650
 * Item_GetMultiDef             0x30052b40  0x40014690
 * Item_GetModelDef             0x30052b60  0x400146b0
 * Menu_Init                    0x30058750  0x4001a2c0
 * Item_Init                    0x300589c0  0x4001a530
 * Item_ValidateTypeData        0x30058d20  0x4001a890
 *
 * The supporting Mac cgame and UI traceback tables independently retain all
 * of these exact source names.  In particular, IsVisible supersedes the
 * cgame reconstruction's local Window_IsVisible spelling.
 */

void Fade(int32_t *flags, float *alpha, float clamp, int32_t *nextTime, int32_t offsetTime, qboolean clearFlags, float fadeAmount,
          float fadeInAmount)
{
    const int32_t realTime = DC->realTime;

    if ((*flags & (WINDOW_FADINGOUT | WINDOW_FADINGIN)) == 0 || realTime <= *nextTime) {
        return;
    }

    *nextTime = coduo_int32_from_bits((uint32_t)realTime + (uint32_t)offsetTime);
    if ((*flags & WINDOW_FADINGOUT) != 0) {
        /* Both PE32 bodies store the rounded binary32 result with FST while
         * retaining the unrounded x87 value for the following comparison. */
        long double faded = (long double)*alpha - fadeAmount;

        *alpha = (float)faded;
        if (clearFlags && faded <= 0.0f) {
            *flags &= ~(WINDOW_FADINGOUT | WINDOW_VISIBLE);
        }
        return;
    }

    {
        long double faded = (long double)*alpha + fadeInAmount;

        *alpha = (float)faded;
        if (faded >= clamp) {
            *alpha = clamp;
            if (clearFlags) {
                *flags &= ~WINDOW_FADINGIN;
            }
        }
    }
}

void Window_Init(windowDef_t *window)
{
    memset(window, 0, sizeof(*window));
    window->borderSize = 1.0f;
    window->foreColor[3] = 1.0f;
    window->foreColor[2] = 1.0f;
    window->foreColor[1] = 1.0f;
    window->foreColor[0] = 1.0f;
    window->cinematic = -1;
}

void Menu_Init(menuDef_t *menu, int32_t loadMode)
{
    displayContextDef_t *display;

    memset(menu, 0, sizeof(*menu));
    display = DC;
    menu->cursorItem = -1;
    menu->fadeAmount = display->menuFadeAmountOut;
    menu->fadeInAmount = display->menuFadeAmountIn;
    menu->fadeClamp = display->menuFadeClamp;
    menu->fadeCycle = display->menuFadeCycle;
    menu->loadMode = loadMode;

    /* Window_Init is inlined in both originals after the outer record clear. */
    memset(&menu->window, 0, sizeof(menu->window));
    menu->window.borderSize = 1.0f;
    menu->window.foreColor[3] = 1.0f;
    menu->window.foreColor[2] = 1.0f;
    menu->window.foreColor[1] = 1.0f;
    menu->window.foreColor[0] = 1.0f;
    menu->window.cinematic = -1;
}

void Item_Init(itemDef_t *item, int32_t loadMode)
{
    memset(item, 0, sizeof(*item));
    item->loadMode = loadMode;
    item->textscale = 0.55f;

    /* Window_Init is likewise inlined in both retained Item_Init bodies. */
    memset(&item->window, 0, sizeof(item->window));
    item->window.borderSize = 1.0f;
    item->window.foreColor[3] = 1.0f;
    item->window.foreColor[2] = 1.0f;
    item->window.foreColor[1] = 1.0f;
    item->window.foreColor[0] = 1.0f;
    item->window.cinematic = -1;
}

void Item_ValidateTypeData(itemDef_t *item, int32_t sourceHandle)
{
    int32_t type;

    if (item->typeData != NULL) {
        if (item->typeValidated != item->type) {
            PC_SourceError(sourceHandle,
                           "Attempting to change type from %d to %d.\n"
                           "Move the type definition higher up in the itemDef.\n",
                           item->typeValidated, item->type);
        }
        return;
    }

    type = item->type;
    item->typeValidated = type;
    if (type == ITEM_TYPE_LISTBOX) {
        item->typeData = UI_Alloc((int32_t)sizeof(listBoxDef_t));
        memset(item->typeData, 0, sizeof(listBoxDef_t));
        return;
    }

    switch (type) {
    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_UPREDITFIELD:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_SLIDER: {
        editFieldDef_t *editField;

        item->typeData = UI_Alloc((int32_t)sizeof(editFieldDef_t));
        memset(item->typeData, 0, sizeof(editFieldDef_t));
        /* Both binaries reload the parsed type after the allocator callback. */
        type = item->type;
        if (type == ITEM_TYPE_EDITFIELD || type == ITEM_TYPE_NUMERICFIELD || type == ITEM_TYPE_UPREDITFIELD) {
            editField = Item_GetEditFieldDef(item);
            if (editField->maxPaintChars == 0) {
                editField->maxPaintChars = 256;
            }
        }
        return;
    }

    case ITEM_TYPE_MULTI:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        item->typeData = UI_Alloc((int32_t)sizeof(multiDef_t));
        memset(item->typeData, 0, sizeof(multiDef_t));
        return;

    case ITEM_TYPE_MODEL:
    case ITEM_TYPE_MENUMODEL:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        item->typeData = UI_Alloc((int32_t)sizeof(modelDef_t));
        memset(item->typeData, 0, sizeof(modelDef_t));
        return;

    default:
        return;
    }
}

listBoxDef_t *Item_GetListBoxDef(itemDef_t *item)
{
    if (item->typeValidated == ITEM_TYPE_LISTBOX) {
        return (listBoxDef_t *)item->typeData;
    }
    Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
    return NULL;
}

qboolean Item_IsEditFieldDef(itemDef_t *item)
{
    switch (item->typeValidated) {
    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_UPREDITFIELD:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_SLIDER:
        return qtrue;
    default:
        return qfalse;
    }
}

editFieldDef_t *Item_GetEditFieldDef(itemDef_t *item)
{
    switch (item->typeValidated) {
    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_UPREDITFIELD:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_SLIDER:
        return (editFieldDef_t *)item->typeData;
    default:
        break;
    }
    Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_EDITFIELD, "
               "ITEM_TYPE_NUMERICFIELD, ITEM_TYPE_UPREDITFIELD, "
               "ITEM_TYPE_YESNO, ITEM_TYPE_BIND, ITEM_TYPE_SLIDER, "
               "or ITEM_TYPE_TEXT\n");
    return NULL;
}

multiDef_t *Item_GetMultiDef(itemDef_t *item)
{
    if (item->typeValidated == ITEM_TYPE_MULTI) {
        return (multiDef_t *)item->typeData;
    }
    Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MULTI\n");
    return NULL;
}

modelDef_t *Item_GetModelDef(itemDef_t *item)
{
    if (item->typeValidated == ITEM_TYPE_MODEL || item->typeValidated == ITEM_TYPE_MENUMODEL) {
        return (modelDef_t *)item->typeData;
    }
    Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MODEL, or "
               "ITEM_TYPE_MENUMODEL\n");
    return NULL;
}

qboolean Rect_ContainsPoint(const rectDef_t *rect, float x, float y)
{
    if (rect == NULL) {
        return qfalse;
    }

    /* Positive ordered comparisons preserve the originals' NaN rejection. */
    return (x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h) ? qtrue : qfalse;
}

qboolean IsVisible(int32_t flags)
{
    return (flags & WINDOW_VISIBLE) != 0 && (flags & WINDOW_FADINGOUT) == 0;
}
