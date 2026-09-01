#include "ui_runtime.h"
#include "ui_menu_globals.h"
#include "ui_parse.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"

#include <stddef.h>
#include <stdint.h>

void Com_Printf(const char *format, ...);
qboolean Item_ListBox_HandleKey(itemDef_t *item, int32_t key,
                                qboolean force);
int32_t Item_Slider_OverSlider(itemDef_t *item, float x, float y);
qboolean Item_Slider_HandleKey(itemDef_t *item, int32_t key);
qboolean Item_YesNo_HandleKey(itemDef_t *item, int32_t key);
qboolean Item_Multi_HandleKey(itemDef_t *item, int32_t key);
qboolean Item_Bind_HandleKey(itemDef_t *item, int32_t key, qboolean down);

enum {
    CAPTURE_SCROLL_INITIAL_DELAY = 500,
    CAPTURE_SCROLL_ADJUST_DELAY = 150,
    CAPTURE_SCROLL_MINIMUM_DELAY = 20,
    CAPTURE_SCROLL_DELAY_STEP = 40,
    CAPTURE_SCROLL_TRACK_INSET = 17,
    CAPTURE_SCROLL_DOUBLE_THUMB = 32,
    CAPTURE_SCROLL_HALF_THUMB = 8,
    CAPTURE_SLIDER_TRACK_WIDTH = 96
};

/*
 * The retained Windows client functions below are instruction twins after
 * rebasing their module-local calls, globals, strings, and float constants:
 *
 *                              cgame       UI
 * Scroll_ListBox_AutoFunc      0x30054420  0x40015f80
 * Scroll_ListBox_ThumbFunc     0x30054490  0x40015ff0
 * Scroll_Slider_ThumbFunc      0x30054660  0x400161c0
 * Item_StartCapture            0x30054740  0x400162a0
 * Item_StopCapture             0x30054870  0x400163d0
 * Item_HandleKey               0x300549a0  0x40016500
 *
 * The corresponding Mac cgame and UI symbol banks independently retain the
 * same six names.  Each DLL owns separate capture globals, but the state and
 * operation graph are one common ui_shared subsystem.
 */

void Scroll_ListBox_AutoFunc(void *captureDataValue)
{
    scrollInfo_t *info = (scrollInfo_t *)captureDataValue;
    displayContextDef_t *context = DC;

    if (context->realTime > info->nextScrollTime) {
        Item_ListBox_HandleKey(info->item, info->scrollKey, qfalse);
        context = DC;
        info->nextScrollTime = coduo_int32_from_bits(
            (uint32_t)context->realTime + (uint32_t)info->adjustValue);
    }
    if (context->realTime > info->nextAdjustTime) {
        info->nextAdjustTime = coduo_int32_from_bits(
            (uint32_t)context->realTime + CAPTURE_SCROLL_ADJUST_DELAY);
        if (info->adjustValue > CAPTURE_SCROLL_MINIMUM_DELAY) {
            info->adjustValue = coduo_int32_from_bits(
                (uint32_t)info->adjustValue - CAPTURE_SCROLL_DELAY_STEP);
        }
    }
}

void Scroll_ListBox_ThumbFunc(void *captureDataValue)
{
    scrollInfo_t *info = (scrollInfo_t *)captureDataValue;
    itemDef_t *item = info->item;
    listBoxDef_t *listBox;
    int32_t maximum;
    int32_t position;
    displayContextDef_t *context;

    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return;
    }
    listBox = (listBoxDef_t *)item->typeData;
    if (listBox == NULL) {
        return;
    }

    if (((uint32_t)item->window.flags & WINDOW_HORIZONTAL) != 0u) {
        float trackOrigin;
        float trackExtent;

        context = DC;
        if ((double)context->cursorx == (double)info->xStart) {
            return;
        }
        trackOrigin = item->window.rect.x + CAPTURE_SCROLL_TRACK_INSET;
        trackExtent = item->window.rect.w - CAPTURE_SCROLL_DOUBLE_THUMB - 2.0f;
        maximum = Item_ListBox_MaxScroll(item);
        context = DC;
        position = coduo_fp_to_i32_extended(
            ((((double)context->cursorx - trackOrigin) -
              CAPTURE_SCROLL_HALF_THUMB) * (double)maximum) /
            ((double)trackExtent - SCROLLBAR_SIZE));
        if (position < 0) {
            position = 0;
        } else if (position > maximum) {
            position = maximum;
        }
        listBox->startPos = position;
        info->xStart = (float)context->cursorx;
    } else {
        float trackOrigin;
        float trackExtent;

        context = DC;
        if ((double)context->cursory != (double)info->yStart) {
            trackOrigin = item->window.rect.y + CAPTURE_SCROLL_TRACK_INSET;
            trackExtent = item->window.rect.h - CAPTURE_SCROLL_DOUBLE_THUMB -
                          2.0f;
            maximum = Item_ListBox_MaxScroll(item);
            context = DC;
            position = coduo_fp_to_i32_extended(
                ((((double)context->cursory - trackOrigin) -
                  CAPTURE_SCROLL_HALF_THUMB) * (double)maximum) /
                ((double)trackExtent - SCROLLBAR_SIZE));
            if (position < 0) {
                position = 0;
            } else if (position > maximum) {
                position = maximum;
            }
            listBox->startPos = position;
            info->yStart = (float)context->cursory;
        }
    }

    if (context->realTime > info->nextScrollTime) {
        Item_ListBox_HandleKey(info->item, info->scrollKey, qfalse);
        context = DC;
        info->nextScrollTime = coduo_int32_from_bits(
            (uint32_t)context->realTime + (uint32_t)info->adjustValue);
    }
    if (context->realTime > info->nextAdjustTime) {
        info->nextAdjustTime = coduo_int32_from_bits(
            (uint32_t)context->realTime + CAPTURE_SCROLL_ADJUST_DELAY);
        if (info->adjustValue > CAPTURE_SCROLL_MINIMUM_DELAY) {
            info->adjustValue = coduo_int32_from_bits(
                (uint32_t)info->adjustValue - CAPTURE_SCROLL_DELAY_STEP);
        }
    }
}

void Scroll_Slider_ThumbFunc(void *captureDataValue)
{
    scrollInfo_t *info = (scrollInfo_t *)captureDataValue;
    itemDef_t *item = info->item;
    editFieldDef_t *edit = Item_GetEditFieldDef(item);
    double origin;
    double cursor;
    double value;
    float upper;
    displayContextDef_t *context;

    if (edit == NULL) {
        return;
    }
    origin = item->window.rect.x;
    if (item->text != NULL) {
        origin = (double)item->textRect.x + item->textRect.w + 8.0;
    }
    context = DC;
    cursor = context->cursorx;
    if (cursor < origin) {
        cursor = origin;
    } else {
        /* Both PE bodies round the upper endpoint to binary32 before the
         * comparison, even though origin remains in the PC=53 x87 domain. */
        upper = (float)(origin + CAPTURE_SLIDER_TRACK_WIDTH);
        if (cursor > upper) {
            cursor = upper;
        }
    }
    value = (double)edit->minVal +
            ((cursor - origin) * (double)(1.0f / 96.0f)) *
            ((double)edit->maxVal - edit->minVal);
    context->setCVar(item->cvar, va("%f", value));
}

void Item_StartCapture(itemDef_t *item, int32_t key)
{
    int32_t region;
    displayContextDef_t *context;

    if (item->type == ITEM_TYPE_LISTBOX) {
        context = DC;
        region = Item_ListBox_OverLB(item, (float)context->cursorx,
                                     (float)context->cursory);
        if ((region & (WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW)) != 0) {
            context = DC;
            ui_scrollInfo.nextScrollTime = coduo_int32_from_bits(
                (uint32_t)context->realTime + CAPTURE_SCROLL_INITIAL_DELAY);
            ui_scrollInfo.item = item;
            captureItem = item;
            ui_scrollInfo.nextAdjustTime = coduo_int32_from_bits(
                (uint32_t)context->realTime + CAPTURE_SCROLL_ADJUST_DELAY);
            ui_scrollInfo.adjustValue = CAPTURE_SCROLL_INITIAL_DELAY;
            ui_scrollInfo.scrollKey = key;
            ui_scrollInfo.scrollDir = (region >> 11) & 1;
            captureFunc = Scroll_ListBox_AutoFunc;
            captureData = &ui_scrollInfo;
            return;
        }
        if ((region & WINDOW_LB_THUMB) == 0) {
            return;
        }
        captureFunc = Scroll_ListBox_ThumbFunc;
    } else {
        if (item->type != ITEM_TYPE_SLIDER) {
            return;
        }
        context = DC;
        region = Item_Slider_OverSlider(item, (float)context->cursorx,
                                        (float)context->cursory);
        if ((region & WINDOW_LB_THUMB) == 0) {
            return;
        }
        captureFunc = Scroll_Slider_ThumbFunc;
    }

    context = DC;
    ui_scrollInfo.item = item;
    ui_scrollInfo.scrollKey = key;
    captureData = &ui_scrollInfo;
    captureItem = item;
    ui_scrollInfo.xStart = (float)context->cursorx;
    ui_scrollInfo.yStart = (float)context->cursory;
}

void Item_StopCapture(void)
{
}

qboolean Item_HandleKey(itemDef_t *item, int32_t key, qboolean down)
{
    if (captureItem == NULL) {
        if (!down) {
            return qfalse;
        }
        if (key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3) {
            Item_StartCapture(item, key);
        }
    } else {
        captureItem = NULL;
        captureFunc = NULL;
        captureData = NULL;
    }

    if (!down) {
        return qfalse;
    }
    switch (item->type) {
    case ITEM_TYPE_LISTBOX:
        return Item_ListBox_HandleKey(item, key, qfalse);
    case ITEM_TYPE_OWNERDRAW:
        return Item_OwnerDraw_HandleKey(item, key);
    case ITEM_TYPE_SLIDER:
        return Item_Slider_HandleKey(item, key);
    case ITEM_TYPE_YESNO:
        return Item_YesNo_HandleKey(item, key);
    case ITEM_TYPE_MULTI:
        return Item_Multi_HandleKey(item, key);
    case ITEM_TYPE_BIND:
        return Item_Bind_HandleKey(item, key, down);
    default:
        return qfalse;
    }
}
