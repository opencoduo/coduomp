// Sources: uo_cgame_mp_x86.dll 0x30057e20..0x3005824f and
//          uo_ui_mp_x86.dll    0x40019990..0x40019dbf

#include "ui_runtime.h"

#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

extern displayContextDef_t *DC;

/*
 * Item_OwnerDraw_Paint — update and paint one owner-draw menu item.
 *
 * The .mcode's size-only VEH_UpdatePath guess is rejected: this function walks
 * itemDef_t/menuDef_t UI fields and dispatches through DC->ownerDrawItem. The
 * same-module PPC name bank corroborates Item_OwnerDraw_Paint. The item pointer
 * arrives in EDI and there are no stack arguments; the register ABI is modeled
 * as an ordinary C parameter.
 */

enum {
    UI_PULSE_PERIOD_MS = 75,
    UI_BLINK_PERIOD_MS = 256,
    ITEM_TEXTSTYLE_PULSE = 1
};

static const float UI_PULSE_DIM_SCALE = 0.8f;
static const float OWNERDRAW_TEXT_GAP = 8.0f;

void Item_OwnerDraw_Paint(itemDef_t *item)
{
    displayContextDef_t *display;
    ui_ownerDrawValue_t ownerDrawValue;
    menuDef_t *menu;
    vec4_t color;
    int32_t i;
    long double x;
    float textX;

    if (item == NULL) {
        return;
    }

    /* 0x30057e2b: ECX retains this one display-context snapshot through the
     * fade work and the ownerDrawValue lookup at 0x30057f06. */
    display = DC;
    if (display->ownerDrawItem == NULL) {
        return;
    }

    menu = item->parent;

    if ((item->window.flags & (WINDOW_FADINGOUT | WINDOW_FADINGIN)) != 0 && display->realTime > item->window.nextTime) {
        item->window.nextTime = (int32_t)((uint32_t)display->realTime + (uint32_t)menu->fadeCycle);

        if ((item->window.flags & WINDOW_FADINGOUT) != 0) {
            /* 0x30057e83-0x30057e93: FST (not FSTP) stores the rounded alpha to
             * foreColor[3] but KEEPS the unrounded 80-bit difference in st0, and the
             * FCOMP against 0.0 tests that unrounded value -- Class 8. Model the live
             * value in long double and round only the stored copy. */
            long double faded = (long double)item->window.foreColor[3] - menu->fadeAmount;
            item->window.foreColor[3] = (float)faded;
            if (faded <= 0.0f) {
                item->window.flags = (int32_t)((uint32_t)item->window.flags & ~(WINDOW_FADINGOUT | WINDOW_VISIBLE));
            }
        } else {
            /* 0x30057ea8-0x30057eb8: same FST-keep (Class 8) -- the clamp compare
             * runs on the unrounded st0, not the float stored to foreColor[3]. */
            long double faded = (long double)item->window.foreColor[3] + menu->fadeInAmount;
            item->window.foreColor[3] = (float)faded;
            if (faded >= menu->fadeClamp) {
                item->window.foreColor[3] = menu->fadeClamp;
                item->window.flags = (int32_t)((uint32_t)item->window.flags & ~WINDOW_FADINGIN);
            }
        }
    }

    color[0] = item->window.foreColor[0];
    color[1] = item->window.foreColor[1];
    color[2] = item->window.foreColor[2];
    color[3] = item->window.foreColor[3];

    if (item->numColors > 0 && (ownerDrawValue = display->ownerDrawValue) != NULL) {
        /* Direct consumer proof: 0x30057f11 loads item+0x248, not the adjacent
         * feeder/special float at +0x24c. 0x30057f06..0x30057f1c also snapshots
         * the callback pointer from the entry display context before calling it. */
        float value = ownerDrawValue(item->window.ownerDraw, item->colorRangeType);

        for (i = 0; i < item->numColors; i++) {
            colorRangeDef_t *range = &item->colorRanges[i];

            if (value >= range->low && value <= range->high) {
                color[0] = range->color[0];
                color[1] = range->color[1];
                color[2] = range->color[2];
                color[3] = range->color[3];
                break;
            }
        }
    }

    if (((uint32_t)item->window.flags & WINDOW_NO_HUD_ALPHA) == 0) {
        color[3] *= DC->getCVarValue("cg_hudAlpha");
    }

    if ((item->window.flags & WINDOW_HASFOCUS) != 0) {
        vec4_t dimmed;
        float t;

        dimmed[0] = menu->focusColor[0] * UI_PULSE_DIM_SCALE;
        dimmed[1] = menu->focusColor[1] * UI_PULSE_DIM_SCALE;
        dimmed[2] = menu->focusColor[2] * UI_PULSE_DIM_SCALE;
        dimmed[3] = menu->focusColor[3] * UI_PULSE_DIM_SCALE;
        /* 0x30058018..0x300580b1: signed integer phase -> FILD -> FSIN;
         * there is no intervening float/double argument store. */
        t = (float)((coduo_x87_sinl((long double)(DC->realTime / UI_PULSE_PERIOD_MS)) + 1.0L) * 0.5L);
        LerpColor(color, menu->focusColor, dimmed, t);
    } else if (item->textStyle == ITEM_TEXTSTYLE_PULSE && ((DC->realTime / UI_BLINK_PERIOD_MS) & 1) == 0) {
        vec4_t dimmed;
        float t;

        dimmed[0] = item->window.foreColor[0] * UI_PULSE_DIM_SCALE;
        dimmed[1] = item->window.foreColor[1] * UI_PULSE_DIM_SCALE;
        dimmed[2] = item->window.foreColor[2] * UI_PULSE_DIM_SCALE;
        dimmed[3] = item->window.foreColor[3] * UI_PULSE_DIM_SCALE;
        t = (float)((coduo_x87_sinl((long double)(DC->realTime / UI_PULSE_PERIOD_MS)) + 1.0L) * 0.5L);
        LerpColor(color, item->window.foreColor, dimmed, t);
    }

    if (((uint32_t)item->cvarFlags & ITEM_CVAR_ENABLE_MASK) != 0 && Item_EnableShowViaCvar(item, ITEM_CVAR_ENABLE) == qfalse) {
        color[0] = menu->disableColor[0];
        color[1] = menu->disableColor[1];
        color[2] = menu->disableColor[2];
        color[3] = menu->disableColor[3];
    }

    if (item->text != NULL) {
        Item_Text_Paint(item);
        /* 0x3005810f-0x3005812c: the DLL keeps textRect.x + textRect.w in st0
         * (80-bit) across the text[0] test and rounds only after the optional
         * + 8.0f, at the call store. A separate `x = a + b; x += 8;` would round
         * the partial sum an extra time (Class 1), so fold each arm into one
         * rvalue. */
        if (item->text[0] != '\0') {
            x = (long double)item->textRect.x + item->textRect.w + OWNERDRAW_TEXT_GAP;
        } else {
            x = (long double)item->textRect.x + item->textRect.w;
        }
        textX = 0.0f;
    } else {
        x = item->window.rect.x;
        textX = item->textalignx;
    }

    DC->ownerDrawItem(x, item->window.rect.y, item->window.rect.w, item->window.rect.h, textX, item->textaligny, item->window.ownerDraw,
                      item->window.ownerDrawFlags, item->alignment, item->special, item->font, item->textscale, color,
                      item->window.background, item->textStyle);
}
