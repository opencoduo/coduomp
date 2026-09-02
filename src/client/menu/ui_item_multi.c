#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"
#include "qcommon/q_string.h"

#include <stddef.h>
#include <stdint.h>

enum {
    UI_MULTI_COMPARE_LIMIT = 99999,
    UI_MULTI_VALUE_GAP = 8,
    UI_MULTI_PULSE_PERIOD = 75
};

extern displayContextDef_t *DC;

void Com_Printf(const char *format, ...);
void Item_Text_Paint(itemDef_t *item);

/*
 * Except for the explicitly marked compatibility behavior below, this complete
 * multi-item runtime is instruction-identical between the two original Windows
 * client modules after rebasing image-local strings, globals, and calls:
 *
 *                                   cgame       UI
 * Item_Multi_CountSettings          0x30053ac0  0x40015620
 * Item_Multi_FindCvarByValue        0x30053af0  0x40015650
 * Item_Multi_Setting                0x30053c00  0x40015760
 * Item_Multi_HandleKey              0x30053d10  0x40015870
 * Item_Multi_Paint                  0x30056560  0x400180c0
 */
int32_t Item_Multi_CountSettings(itemDef_t *item)
{
    multiDef_t *multi;

    if (item->typeValidated != ITEM_TYPE_MULTI) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MULTI\n");
        return 0;
    }
    multi = (multiDef_t *)item->typeData;
    return multi != NULL ? multi->count : 0;
}

int32_t Item_Multi_FindCvarByValue(itemDef_t *item)
{
    multiDef_t *multi;
    char cvarBuffer[MAX_STRING_CHARS];
    float cvarValue = 0.0f;
    int32_t index;

    if (item->typeValidated != ITEM_TYPE_MULTI) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MULTI\n");
        return 0;
    }
    multi = (multiDef_t *)item->typeData;
    if (multi == NULL) {
        return 0;
    }

    if (multi->strDef != 0) {
        DC->getCVarString(item->cvar, cvarBuffer, sizeof(cvarBuffer));
    } else {
        cvarValue = DC->getCVarValue(item->cvar);
    }

    for (index = 0; index < multi->count; ++index) {
        if (multi->strDef != 0) {
            if (multi->cvarStr[index] != NULL && Q_stricmpn(multi->cvarStr[index], cvarBuffer, UI_MULTI_COMPARE_LIMIT) == 0) {
                return index;
            }
        } else if (multi->cvarValue[index] == cvarValue) {
            return index;
        }
    }
    return 0;
}

const char *Item_Multi_Setting(itemDef_t *item)
{
    multiDef_t *multi;
    char cvarBuffer[MAX_STRING_CHARS];
    float cvarValue = 0.0f;
    int32_t index;

    if (item->typeValidated != ITEM_TYPE_MULTI) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MULTI\n");
        return "";
    }
    multi = (multiDef_t *)item->typeData;
    if (multi == NULL) {
        return "";
    }

    if (multi->strDef != 0) {
        DC->getCVarString(item->cvar, cvarBuffer, sizeof(cvarBuffer));
    } else {
        cvarValue = DC->getCVarValue(item->cvar);
    }

    for (index = 0; index < multi->count; ++index) {
        if (multi->strDef != 0) {
            if (multi->cvarStr[index] != NULL && Q_stricmpn(multi->cvarStr[index], cvarBuffer, UI_MULTI_COMPARE_LIMIT) == 0) {
                return multi->cvarList[index];
            }
        } else if (multi->cvarValue[index] == cvarValue) {
            return multi->cvarList[index];
        }
    }
    return "";
}

qboolean Item_Multi_HandleKey(itemDef_t *item, int32_t key)
{
    multiDef_t *multi;
    int32_t current;
    int32_t count;

    if (item->typeValidated != ITEM_TYPE_MULTI) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MULTI\n");
        return qfalse;
    }
    multi = (multiDef_t *)item->typeData;
    if (multi == NULL) {
        return qfalse;
    }

    if (!((float)DC->cursorx >= item->window.rect.x && (float)DC->cursorx <= item->window.rect.x + item->window.rect.w &&
          (float)DC->cursory >= item->window.rect.y && (float)DC->cursory <= item->window.rect.y + item->window.rect.h) ||
        (item->window.flags & WINDOW_HASFOCUS) == 0 || item->cvar == NULL) {
        return qfalse;
    }
    if (key != K_MOUSE1 && key != K_ENTER && key != K_MOUSE2 && key != K_MOUSE3) {
        return qfalse;
    }

    current = Item_Multi_FindCvarByValue(item);
    count = Item_Multi_CountSettings(item);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): right-click cycles to
     * the previous choice while left-click, Enter, and middle-click retain the
     * original forward direction. */
    if (key == K_MOUSE2 && count > 0) {
        current = current > 0 ? current - 1 : count - 1;
    } else {
        ++current;
        if (current < 0 || current >= count) {
            current = 0;
        }
    }

    if (multi->strDef != 0) {
        DC->setCVar(item->cvar, multi->cvarStr[current]);
    } else {
        const float value = multi->cvarValue[current];
        const int32_t integerValue = coduo_fp_to_i32_extended((long double)value);

        if ((long double)integerValue == (long double)value) {
            DC->setCVar(item->cvar, va("%i", integerValue));
        } else {
            DC->setCVar(item->cvar, va("%f", (double)value));
        }
    }
    return qtrue;
}

void Item_Multi_Paint(itemDef_t *item)
{
    vec4_t color;
    const char *setting;
    float x;
    const uint32_t focusBits = (uint32_t)item->window.flags & WINDOW_HASFOCUS;
    menuDef_t *parent = item->parent;
    displayContextDef_t *display;

    if (focusBits != 0u) {
        vec4_t dimmed;
        float fraction;
        int32_t component;
        int32_t pulseStep;

        for (component = 0; component < 4; ++component) {
            dimmed[component] = parent->focusColor[component] * 0.8f;
        }
        display = DC;
        pulseStep = display->realTime / UI_MULTI_PULSE_PERIOD;
        fraction = (float)((coduo_x87_sinl((long double)pulseStep) + 1.0L) * 0.5L);
        LerpColor(color, parent->focusColor, dimmed, fraction);
    } else {
        color[0] = item->window.foreColor[0];
        color[1] = item->window.foreColor[1];
        color[2] = item->window.foreColor[2];
        color[3] = item->window.foreColor[3];
    }

    setting = Item_Multi_Setting(item);
    if (setting[0] == '@') {
        display = DC;
        setting = display->getLocalizedString(setting + 1);
    }
    if (item->text != NULL) {
        Item_Text_Paint(item);
        x = (float)((long double)item->textRect.w + (long double)item->textRect.x + UI_MULTI_VALUE_GAP);
    } else {
        x = item->textRect.x;
    }
    display = DC;
    display->drawText(x, item->textRect.y, item->font, item->textscale, color, setting, 0, 0, item->textStyle);
}
