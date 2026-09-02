#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <stdint.h>

extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

// Sources: uo_cgame_mp_x86.dll 0x30053490..0x300539da and
//          uo_ui_mp_x86.dll    0x40014fe0..0x40015531.
// The operation graphs are identical after rebasing. UI additionally calls
// UI_SyncServerListSelection before Item_ListBox_MaxScroll; the target adapter
// preserves that original module-specific edge without forking this body.
//
// Item_ListBox_HandleKey — apply a keyboard, wheel, or mouse-scrollbar action
// to a listbox. The original .mcode label CG_SetShellShockParmsFromCvars was a
// size-only match and is rejected: this body validates ITEM_TYPE_LISTBOX, calls
// feederCount/feederSelection, updates listBoxDef_t scroll and cursor members,
// and recognizes the WINDOW_LB_* regions.
//
// ABI: item is in ECX, key in EAX, and force is the sole stack argument. The
// plain RET leaves that argument caller-cleaned; represented as a normal C ABI.

enum {
    LISTBOX_DOUBLE_CLICK_DELAY = 300
};

qboolean Item_ListBox_HandleKey(itemDef_t *item, int32_t key, qboolean force)
{
    listBoxDef_t *listBox;
    displayContextDef_t *context;
    int32_t count;
    int32_t maxScroll;
    int32_t page;
    int32_t selection;
    int32_t hovered;
    float feederId;

    if (item->typeValidated == ITEM_TYPE_LISTBOX) {
        listBox = (listBoxDef_t *)item->typeData;
    } else {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        listBox = NULL;
    }

    /* 0x300534b8..0x300534cb: the count callback precedes the NULL payload
     * check in the machine code, including on the type-error path. */
    feederId = item->special;
    context = DC;
    count = context->feederCount(feederId);
    if (listBox == NULL) {
        return qfalse;
    }

    /* When not forced, only the focused listbox under the cursor consumes the
     * action (0x300534dd..0x30053517). */
    if (!force) {
        int32_t cursorY;
        int32_t cursorX;

        context = DC;
        cursorY = context->cursory;
        cursorX = context->cursorx;
        if (!Rect_ContainsPoint(&item->window.rect,
                                (float)cursorX, (float)cursorY) ||
            (item->window.flags & WINDOW_HASFOCUS) == 0) {
            return qfalse;
        }
    }

    client_ui_compat_sync_server_list_selection(item);
    maxScroll = Item_ListBox_MaxScroll(item);
    page = coduo_fp_to_i32_extended(
        (item->window.flags & WINDOW_HORIZONTAL)
            ? (long double)item->window.rect.w / listBox->elementWidth
            : (long double)item->window.rect.h / listBox->elementHeight);

    /* 0x30053527..0x3005368e: direction keys are orientation-specific. */
    if ((item->window.flags & WINDOW_HORIZONTAL) != 0) {
        switch (key) {
        case K_LEFTARROW:
        case K_KP_LEFTARROW:
            if (listBox->notselectable) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)listBox->startPos - 1u);
                if (listBox->startPos < 0) listBox->startPos = 0;
                return qtrue;
            }
            listBox->cursorPos = coduo_int32_from_bits(
                (uint32_t)listBox->cursorPos - 1u);
            if (listBox->cursorPos < 0) listBox->cursorPos = 0;
            if (listBox->cursorPos < listBox->startPos)
                listBox->startPos = listBox->cursorPos;
            if (listBox->cursorPos >= coduo_int32_from_bits(
                    (uint32_t)listBox->startPos + (uint32_t)page)) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)listBox->cursorPos - (uint32_t)page + 1u);
            }
            goto publish_horizontal;

        case K_RIGHTARROW:
        case K_KP_RIGHTARROW:
            if (listBox->notselectable) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)listBox->startPos + 1u);
                if (listBox->startPos >= count)
                    listBox->startPos = coduo_int32_from_bits((uint32_t)count - 1u);
                return qtrue;
            }
            listBox->cursorPos = coduo_int32_from_bits(
                (uint32_t)listBox->cursorPos + 1u);
            if (listBox->cursorPos < listBox->startPos)
                listBox->startPos = listBox->cursorPos;
            if (listBox->cursorPos >= count)
                listBox->cursorPos = coduo_int32_from_bits((uint32_t)count - 1u);
            if (listBox->cursorPos >= coduo_int32_from_bits(
                    (uint32_t)listBox->startPos + (uint32_t)page)) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)listBox->cursorPos - (uint32_t)page + 1u);
            }
            goto publish_horizontal;

        default:
            break;
        }
    } else {
        switch (key) {
        case K_UPARROW:
        case K_KP_UPARROW:
        case K_MWHEELUP:
            if (listBox->notselectable) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)listBox->startPos - 1u);
                if (listBox->startPos < 0) listBox->startPos = 0;
                return qtrue;
            }
            item->cursorPos = coduo_int32_from_bits((uint32_t)item->cursorPos - 1u);
            if (item->cursorPos < 0) item->cursorPos = 0;
            if (item->cursorPos < listBox->startPos)
                listBox->startPos = item->cursorPos;
            if (item->cursorPos >= coduo_int32_from_bits(
                    (uint32_t)listBox->startPos + (uint32_t)page)) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)item->cursorPos - (uint32_t)page + 1u);
            }
            goto publish_vertical;

        case K_DOWNARROW:
        case K_KP_DOWNARROW:
        case K_MWHEELDOWN:
            if (listBox->notselectable) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)listBox->startPos + 1u);
                if (listBox->startPos > maxScroll) listBox->startPos = maxScroll;
                return qtrue;
            }
            item->cursorPos = coduo_int32_from_bits((uint32_t)item->cursorPos + 1u);
            if (item->cursorPos < listBox->startPos)
                listBox->startPos = item->cursorPos;
            if (item->cursorPos >= count)
                item->cursorPos = coduo_int32_from_bits((uint32_t)count - 1u);
            if (item->cursorPos >= coduo_int32_from_bits(
                    (uint32_t)listBox->startPos + (uint32_t)page)) {
                listBox->startPos = coduo_int32_from_bits(
                    (uint32_t)item->cursorPos - (uint32_t)page + 1u);
            }
            goto publish_vertical;

        default:
            break;
        }
    }

    switch (key) {

    case K_MOUSE1:
    case K_MOUSE2:
        if (item->window.flags & WINDOW_LB_LEFTARROW) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)listBox->startPos - 1u);
            if (listBox->startPos < 0) listBox->startPos = 0;
            return qtrue;
        }
        if (item->window.flags & WINDOW_LB_RIGHTARROW) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)listBox->startPos + 1u);
            if (listBox->startPos > maxScroll) listBox->startPos = maxScroll;
            return qtrue;
        }
        if (item->window.flags & WINDOW_LB_PGUP) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)listBox->startPos - (uint32_t)page);
            if (listBox->startPos < 0) listBox->startPos = 0;
            return qtrue;
        }
        if (item->window.flags & WINDOW_LB_PGDN) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)listBox->startPos + (uint32_t)page);
            if (listBox->startPos > maxScroll) listBox->startPos = maxScroll;
            return qtrue;
        }
        if (item->window.flags & WINDOW_LB_THUMB) {
            return qtrue;
        }

        context = DC;
        if (context->realTime < lastListBoxClickTime) {
            const char *doubleClick = listBox->doubleClick;

            if (doubleClick != NULL && item->cursorPos == listBox->cursorPos) {
                Item_RunScript(item, doubleClick);
                context = DC;
            }
        }
        lastListBoxClickTime = coduo_int32_from_bits(
            (uint32_t)context->realTime + LISTBOX_DOUBLE_CLICK_DELAY);

        if (item->cursorPos == listBox->cursorPos) {
            return qtrue;
        }
        hovered = listBox->cursorPos;
        feederId = item->special;
        if (hovered < context->feederCount(feederId)) {
            item->cursorPos = hovered;
        }
        selection = item->cursorPos;
        feederId = item->special;
        context = DC;
        context->feederSelection(feederId, selection);
        return qtrue;

    case K_PGUP:
    case K_KP_PGUP:
        if (listBox->notselectable) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)listBox->startPos - (uint32_t)page);
            if (listBox->startPos < 0) listBox->startPos = 0;
            return qtrue;
        }
        item->cursorPos = coduo_int32_from_bits(
            (uint32_t)item->cursorPos - (uint32_t)page);
        if (item->cursorPos < 0) item->cursorPos = 0;
        if (item->cursorPos < listBox->startPos)
            listBox->startPos = item->cursorPos;
        if (item->cursorPos >= coduo_int32_from_bits(
                (uint32_t)listBox->startPos + (uint32_t)page)) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)item->cursorPos - (uint32_t)page + 1u);
        }
        goto publish_vertical;

    case K_PGDN:
    case K_KP_PGDN:
        if (listBox->notselectable) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)listBox->startPos + (uint32_t)page);
            if (listBox->startPos > maxScroll) listBox->startPos = maxScroll;
            return qtrue;
        }
        item->cursorPos = coduo_int32_from_bits(
            (uint32_t)item->cursorPos + (uint32_t)page);
        if (item->cursorPos < listBox->startPos)
            listBox->startPos = item->cursorPos;
        if (item->cursorPos >= count)
            item->cursorPos = coduo_int32_from_bits((uint32_t)count - 1u);
        if (item->cursorPos >= coduo_int32_from_bits(
                (uint32_t)listBox->startPos + (uint32_t)page)) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)item->cursorPos - (uint32_t)page + 1u);
        }
        goto publish_vertical;

    case K_HOME:
    case K_KP_HOME:
        if (listBox->notselectable) {
            listBox->startPos = 0;
            return qtrue;
        }
        context = DC;
        item->cursorPos = 0;
        listBox->startPos = 0;
        listBox->cursorPos = 0;
        selection = item->cursorPos;
        feederId = item->special;
        context->feederSelection(feederId, selection);
        return qtrue;

    case K_END:
    case K_KP_END:
        if (listBox->notselectable) {
            listBox->startPos = maxScroll;
            return qtrue;
        }
        item->cursorPos = coduo_int32_from_bits((uint32_t)count - 1u);
        listBox->cursorPos = item->cursorPos;
        if (item->cursorPos >= coduo_int32_from_bits(
                (uint32_t)listBox->startPos + (uint32_t)page)) {
            listBox->startPos = coduo_int32_from_bits(
                (uint32_t)item->cursorPos - (uint32_t)page + 1u);
        }
        goto publish_vertical;

    default:
        return qfalse;
    }

publish_horizontal:
    feederId = item->special;
    selection = listBox->cursorPos;
    item->cursorPos = selection;
    context = DC;
    context->feederSelection(feederId, selection);
    return qtrue;

publish_vertical:
    selection = item->cursorPos;
    feederId = item->special;
    context = DC;
    context->feederSelection(feederId, selection);
    return qtrue;
}
