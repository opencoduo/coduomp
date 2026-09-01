#include "ui_runtime.h"
#include "compat/coduo_native_x87.h"

#include "compat/coduo_int32_bits.h"

#include <math.h>

enum {
    UI_TEXT_STYLE_PULSE = 1,
    UI_TEXT_PULSE_PERIOD = 75,
    UI_TEXT_BLINK_PERIOD = 256,
    UI_ITEM_ENABLE_DISABLE_MASK = ITEM_CVAR_ENABLE | ITEM_CVAR_DISABLE
};

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30055890 and
 * uo_ui_mp_x86.dll 0x400173f0. */
extern displayContextDef_t *DC;

void Item_TextColor(itemDef_t *item, vec4_t color)
{
    menuDef_t *menu = item->parent;
    float fadeInAmount = menu->fadeInAmount;
    float fadeAmount = menu->fadeAmount;
    float fadeClamp = menu->fadeClamp;
    displayContextDef_t *display = DC;

    if ((item->window.flags & (WINDOW_FADINGOUT | WINDOW_FADINGIN)) != 0 &&
        display->realTime > item->window.nextTime) {
        item->window.nextTime = coduo_int32_from_bits(
            (uint32_t)display->realTime + (uint32_t)menu->fadeCycle);
        if ((item->window.flags & WINDOW_FADINGOUT) != 0) {
            /* FST-keep at 0x4001744a..0x40017450: the store rounds
             * foreColor[3] to float, but the FCOMP tests the unrounded
             * 80-bit difference. */
            long double fadedAlpha =
                (long double)item->window.foreColor[3] - fadeAmount;
            item->window.foreColor[3] = (float)fadedAlpha;
            if (fadedAlpha <= 0.0f) {
                item->window.flags &= ~(WINDOW_FADINGOUT | WINDOW_VISIBLE);
            }
        } else {
            /* FST-keep at 0x4001746f..0x40017475: as above. */
            long double fadedAlpha =
                (long double)fadeInAmount + item->window.foreColor[3];
            item->window.foreColor[3] = (float)fadedAlpha;
            if (fadedAlpha >= fadeClamp) {
                item->window.foreColor[3] = fadeClamp;
                item->window.flags &= ~WINDOW_FADINGIN;
            }
        }
    }

    if ((item->window.flags & WINDOW_HASFOCUS) != 0) {
        vec4_t dimmed;
        float fraction;
        int32_t component;
        int32_t pulseStep = display->realTime / UI_TEXT_PULSE_PERIOD;

        for (component = 0; component < 4; ++component) {
            dimmed[component] = menu->focusColor[component] * 0.8f;
        }
        fraction = (float)((coduo_x87_sinl((long double)pulseStep) + 1.0f) *
                           0.5f);
        LerpColor(color, menu->focusColor, dimmed, fraction);
    } else if (item->textStyle == UI_TEXT_STYLE_PULSE) {
        int32_t realTime = display->realTime;

        if (((realTime / UI_TEXT_BLINK_PERIOD) & 1) == 0) {
            vec4_t dimmed;
            float fraction;
            int32_t component;
            int32_t pulseStep = realTime / UI_TEXT_PULSE_PERIOD;

            for (component = 0; component < 4; ++component) {
                dimmed[component] = item->window.foreColor[component] * 0.8f;
            }
            fraction = (float)((coduo_x87_sinl((long double)pulseStep) +
                                1.0f) * 0.5f);
            LerpColor(color, item->window.foreColor, dimmed, fraction);
        } else {
            color[0] = item->window.foreColor[0];
            color[1] = item->window.foreColor[1];
            color[2] = item->window.foreColor[2];
            color[3] = item->window.foreColor[3];
        }
    } else {
        color[0] = item->window.foreColor[0];
        color[1] = item->window.foreColor[1];
        color[2] = item->window.foreColor[2];
        color[3] = item->window.foreColor[3];
    }

    if (item->cvarTest != NULL && item->cvarTest[0] != '\0' &&
        item->enableCvar != NULL && item->enableCvar[0] != '\0' &&
        (item->cvarFlags & UI_ITEM_ENABLE_DISABLE_MASK) != 0 &&
        !Item_EnableShowViaCvar(item, qtrue)) {
        color[0] = menu->disableColor[0];
        color[1] = menu->disableColor[1];
        color[2] = menu->disableColor[2];
        color[3] = menu->disableColor[3];
    }
}
