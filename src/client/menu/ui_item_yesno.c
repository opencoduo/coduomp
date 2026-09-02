#include "ui_runtime.h"

#include "compat/coduo_native_x87.h"

#include <stddef.h>
#include <stdint.h>

enum {
    UI_YESNO_VALUE_GAP = 8,
    UI_YESNO_PULSE_PERIOD = 75
};

extern displayContextDef_t *DC;

void Item_Text_Paint(itemDef_t *item);

/*
 * The original Windows yes/no input and paint bodies are instruction twins:
 *
 *                                   cgame       UI
 * Item_YesNo_HandleKey              0x300539e0  0x40015540
 * Item_YesNo_Paint                  0x300563b0  0x40017f10
 */
qboolean Item_YesNo_HandleKey(itemDef_t *item, int32_t key)
{
    float cursorX;
    float cursorY;
    int32_t newValue;

    if (item == NULL) {
        return qfalse;
    }

    cursorX = (float)DC->cursorx;
    cursorY = (float)DC->cursory;
    if (!(cursorX >= item->window.rect.x && cursorX <= item->window.rect.x + item->window.rect.w && cursorY >= item->window.rect.y &&
          cursorY <= item->window.rect.y + item->window.rect.h) ||
        (item->window.flags & WINDOW_HASFOCUS) == 0 || item->cvar == NULL) {
        return qfalse;
    }
    if (key != K_MOUSE1 && key != K_ENTER && key != K_MOUSE2 && key != K_MOUSE3) {
        return qfalse;
    }

    newValue = DC->getCVarValue(item->cvar) == 0.0f;
    DC->setCVar(item->cvar, va("%i", newValue));
    return qtrue;
}

void Item_YesNo_Paint(itemDef_t *item)
{
    vec4_t color;
    const char *valueText;
    float value = 0.0f;
    float x;
    const char *cvar = item->cvar;
    menuDef_t *parent = item->parent;
    displayContextDef_t *display;

    if (cvar != NULL) {
        display = DC;
        value = display->getCVarValue(cvar);
    }
    display = DC;

    if ((item->window.flags & WINDOW_HASFOCUS) != 0) {
        vec4_t dimmed;
        float fraction;
        int32_t component;
        int32_t pulseStep;

        for (component = 0; component < 4; ++component) {
            dimmed[component] = parent->focusColor[component] * 0.8f;
        }
        pulseStep = display->realTime / UI_YESNO_PULSE_PERIOD;
        fraction = (float)((coduo_x87_sinl((long double)pulseStep) + 1.0L) * 0.5L);
        LerpColor(color, parent->focusColor, dimmed, fraction);
    } else {
        color[0] = item->window.foreColor[0];
        color[1] = item->window.foreColor[1];
        color[2] = item->window.foreColor[2];
        color[3] = item->window.foreColor[3];
    }

    valueText = display->getLocalizedString(value == 0.0f ? "EXE_NO" : "EXE_YES");
    if (item->text != NULL) {
        Item_Text_Paint(item);
        x = (float)((long double)item->textRect.w + (long double)item->textRect.x + UI_YESNO_VALUE_GAP);
    } else {
        x = item->textRect.x;
    }
    display = DC;
    display->drawText(x, item->textRect.y, item->font, item->textscale, color, valueText, 0, 0, item->textStyle);
}
