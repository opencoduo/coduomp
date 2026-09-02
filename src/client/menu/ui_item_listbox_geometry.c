#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

enum {
    LISTBOX_SCROLLBAR_HALF_PIXELS = 8
};

/*
 * Complete shared listbox geometry and hover subsystem.  Each cgame/UI pair is
 * instruction-identical after rebasing only image-local calls, globals, and
 * constants:
 *
 *                                  cgame       UI
 * Item_ListBox_MaxScroll           0x30052b90  0x400146e0
 * Item_ListBox_ThumbPosition       0x30052c00  0x40014750
 * Item_ListBox_ThumbDrawPosition   0x30052cd0  0x40014820
 * Item_ListBox_OverLB              0x30052ed0  0x40014a20
 * Item_ListBox_MouseEnter          0x30053110  0x40014c60
 *
 * The shared input and paint bodies use a target boundary for the UI DLL's
 * additional server-list selection synchronization.
 */

int32_t Item_ListBox_MaxScroll(itemDef_t *item)
{
    listBoxDef_t *listBox;
    int32_t count;
    int32_t maximum;

    if (item->typeValidated == ITEM_TYPE_LISTBOX) {
        listBox = (listBoxDef_t *)item->typeData;
    } else {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        listBox = NULL;
    }

    count = DC->feederCount(item->special);
    if (listBox == NULL) {
        return 0;
    }

    if ((item->window.flags & WINDOW_HORIZONTAL) != 0) {
        maximum = coduo_int32_from_bits((uint32_t)count -
                                        (uint32_t)coduo_fp_to_i32_extended((long double)item->window.rect.w / listBox->elementWidth) + 1u);
    } else {
        maximum = coduo_int32_from_bits((uint32_t)count -
                                        (uint32_t)coduo_fp_to_i32_extended((long double)item->window.rect.h / listBox->elementHeight) + 1u);
    }

    return maximum < 0 ? 0 : maximum;
}

int32_t Item_ListBox_ThumbPosition(itemDef_t *item)
{
    listBoxDef_t *listBox;
    int32_t maximum;
    long double trackStep;
    float extent;
    float origin;

    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return 0;
    }

    listBox = (listBoxDef_t *)item->typeData;
    if (listBox == NULL) {
        return 0;
    }

    maximum = Item_ListBox_MaxScroll(item);
    if ((item->window.flags & WINDOW_HORIZONTAL) != 0) {
        extent = item->window.rect.w;
        origin = item->window.rect.x;
    } else {
        extent = item->window.rect.h;
        origin = item->window.rect.y;
    }

    /* The original FIDIV leaves this quotient in the x87 stack through the
     * following FILD/FMUL; there is no binary32 spill before conversion. */
    if (maximum > 0) {
        trackStep = ((long double)extent - (long double)(SCROLLBAR_SIZE * 2) - 2.0L - (long double)SCROLLBAR_SIZE) / (long double)maximum;
    } else {
        trackStep = 0.0L;
    }

    return coduo_fp_to_i32_extended((long double)listBox->startPos * trackStep + (long double)origin + (long double)(SCROLLBAR_SIZE + 1));
}

int32_t Item_ListBox_ThumbDrawPosition(itemDef_t *item)
{
    int32_t cursor;
    int32_t lowerBound;
    int32_t upperBound;
    long double edge;

    if (captureItem != item) {
        return Item_ListBox_ThumbPosition(item);
    }

    if ((item->window.flags & WINDOW_HORIZONTAL) != 0) {
        cursor = DC->cursorx;
        edge = (long double)item->window.rect.x + (long double)(SCROLLBAR_SIZE + 1);
        lowerBound = coduo_int32_from_bits((uint32_t)coduo_fp_to_i32_extended(edge) + LISTBOX_SCROLLBAR_HALF_PIXELS);
        edge = ((long double)item->window.rect.w + (long double)item->window.rect.x) - (long double)(SCROLLBAR_SIZE * 2) - 1.0L;
        upperBound = coduo_int32_from_bits((uint32_t)coduo_fp_to_i32_extended(edge) + LISTBOX_SCROLLBAR_HALF_PIXELS);
    } else {
        cursor = DC->cursory;
        edge = (long double)item->window.rect.y + (long double)(SCROLLBAR_SIZE + 1);
        lowerBound = coduo_int32_from_bits((uint32_t)coduo_fp_to_i32_extended(edge) + LISTBOX_SCROLLBAR_HALF_PIXELS);
        edge = ((long double)item->window.rect.h + (long double)item->window.rect.y) - (long double)(SCROLLBAR_SIZE * 2) - 1.0L;
        upperBound = coduo_int32_from_bits((uint32_t)coduo_fp_to_i32_extended(edge) + LISTBOX_SCROLLBAR_HALF_PIXELS);
    }

    if (cursor < lowerBound || cursor > upperBound) {
        return Item_ListBox_ThumbPosition(item);
    }
    return coduo_int32_from_bits((uint32_t)cursor - LISTBOX_SCROLLBAR_HALF_PIXELS);
}

int32_t Item_ListBox_OverLB(itemDef_t *item, float x, float y)
{
    listBoxDef_t *listBox;
    rectDef_t region;
    float thumb;

    (void)DC->feederCount(item->special);
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return 0;
    }
    listBox = (listBoxDef_t *)item->typeData;
    if (listBox == NULL) {
        return 0;
    }

    region.w = SCROLLBAR_SIZE;
    region.h = SCROLLBAR_SIZE;
    if ((item->window.flags & WINDOW_HORIZONTAL) != 0) {
        region.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
        region.x = item->window.rect.x;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_LEFTARROW;
        }
        region.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_RIGHTARROW;
        }
        thumb = (float)Item_ListBox_ThumbPosition(item);
        region.x = thumb;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_THUMB;
        }
        region.x = item->window.rect.x + SCROLLBAR_SIZE;
        region.w = thumb - region.x;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_PGUP;
        }
        region.x = thumb + SCROLLBAR_SIZE;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        region.w = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE - region.x;
    } else {
        region.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_SIZE;
        region.y = item->window.rect.y;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_LEFTARROW;
        }
        region.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_RIGHTARROW;
        }
        thumb = (float)Item_ListBox_ThumbPosition(item);
        region.y = thumb;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_THUMB;
        }
        region.y = item->window.rect.y + SCROLLBAR_SIZE;
        region.h = thumb - region.y;
        if (Rect_ContainsPoint(&region, x, y)) {
            return WINDOW_LB_PGUP;
        }
        region.y = thumb + SCROLLBAR_SIZE;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        region.h = item->window.rect.y + item->window.rect.h - SCROLLBAR_SIZE - region.y;
    }

    return Rect_ContainsPoint(&region, x, y) ? WINDOW_LB_PGDN : 0;
}

void Item_ListBox_MouseEnter(itemDef_t *item, float x, float y)
{
    listBoxDef_t *listBox;
    rectDef_t body;
    int32_t cursor;

    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return;
    }
    listBox = (listBoxDef_t *)item->typeData;
    if (listBox == NULL) {
        return;
    }

    item->window.flags &= ~WINDOW_LB_REGION_MASK;
    item->window.flags |= Item_ListBox_OverLB(item, x, y);
    memcpy(&body.x, &item->window.rect.x, sizeof(body.x));
    memcpy(&body.y, &item->window.rect.y, sizeof(body.y));

    if ((item->window.flags & WINDOW_HORIZONTAL) != 0) {
        if ((item->window.flags & WINDOW_LB_REGION_MASK) != 0 || listBox->elementStyle != LISTBOX_ELEMENT_IMAGE) {
            return;
        }
        body.w = (float)((long double)item->window.rect.w - (long double)listBox->drawPadding);
        body.h = (float)((long double)item->window.rect.h - (long double)SCROLLBAR_SIZE);
        if (!Rect_ContainsPoint(&body, x, y)) {
            return;
        }
        cursor = coduo_int32_from_bits((uint32_t)coduo_fp_to_i32_extended(((long double)x - item->window.rect.x) / listBox->elementWidth) +
                                       (uint32_t)listBox->startPos);
        listBox->cursorPos = cursor;
        if (cursor >= listBox->endPos) {
            listBox->cursorPos = listBox->endPos;
        }
        return;
    }

    if ((item->window.flags & WINDOW_LB_REGION_MASK) != 0) {
        return;
    }
    body.w = (float)((long double)item->window.rect.w - (long double)SCROLLBAR_SIZE);
    body.h = (float)((long double)item->window.rect.h - (long double)listBox->drawPadding);
    if (!Rect_ContainsPoint(&body, x, y)) {
        return;
    }
    cursor =
        coduo_int32_from_bits((uint32_t)coduo_fp_to_i32_extended((((long double)y - 2.0L) - item->window.rect.y) / listBox->elementHeight) +
                              (uint32_t)listBox->startPos);
    listBox->cursorPos = cursor;
    if (cursor > listBox->endPos) {
        listBox->cursorPos = listBox->endPos;
    }
}
