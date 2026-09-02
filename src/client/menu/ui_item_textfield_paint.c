#include "ui_runtime.h"
#include "compat/coduo_native_x87.h"

#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <math.h>

enum {
    UI_TEXTFIELD_VALUE_GAP = 8,
    UI_TEXTFIELD_PULSE_PERIOD = 75,
    UI_TEXTFIELD_INSERT_CURSOR = '|',
    UI_TEXTFIELD_OVERSTRIKE_CURSOR = '_'
};

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30056120 and
 * uo_ui_mp_x86.dll 0x40017c80. */
extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

void Item_TextField_Paint(itemDef_t *item)
{
    editFieldDef_t *editField;
    char buffer[MAX_STRING_CHARS];
    vec4_t color;
    displayContextDef_t *display;
    uint32_t focusBits;
    menuDef_t *parent;
    int32_t labelGap;

    switch (item->typeValidated) {
    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_SLIDER:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_UPREDITFIELD:
        break;
    default:
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_EDITFIELD, "
                   "ITEM_TYPE_NUMERICFIELD, ITEM_TYPE_UPREDITFIELD, "
                   "ITEM_TYPE_YESNO, ITEM_TYPE_BIND, ITEM_TYPE_SLIDER, or "
                   "ITEM_TYPE_TEXT\n");
        return;
    }

    editField = (editFieldDef_t *)item->typeData;
    if (editField == NULL)
        return;

    Item_Text_Paint(item);
    buffer[0] = '\0';
    {
        const char *cvar = item->cvar;

        if (cvar != NULL) {
            display = DC;
            display->getCVarString(cvar, buffer, MAX_STRING_CHARS);
        }
    }

    focusBits = (uint32_t)item->window.flags & WINDOW_HASFOCUS;
    parent = item->parent;
    if (focusBits != 0u) {
        vec4_t dimmed;
        float fraction;
        int32_t component;
        int32_t pulseStep;

        for (component = 0; component < 4; ++component) {
            dimmed[component] = parent->focusColor[component] * 0.8f;
        }
        display = DC;
        pulseStep = display->realTime / UI_TEXTFIELD_PULSE_PERIOD;
        fraction = (float)((coduo_x87_sinl((long double)pulseStep) + 1.0f) * 0.5f);
        LerpColor(color, parent->focusColor, dimmed, fraction);
    } else {
        color[0] = item->window.foreColor[0];
        color[1] = item->window.foreColor[1];
        color[2] = item->window.foreColor[2];
        color[3] = item->window.foreColor[3];
    }

    if (item->text != NULL && item->text[0] != '\0') {
        labelGap = UI_TEXTFIELD_VALUE_GAP;
    } else {
        labelGap = 0;
    }

    if (focusBits != 0u && g_editingField) {
        int8_t cursorCharacter;
        float valueX;
        int32_t paintOffset;
        int32_t cursorColumn;

        display = DC;
        cursorCharacter = display->getOverstrikeMode() ? UI_TEXTFIELD_OVERSTRIKE_CURSOR : UI_TEXTFIELD_INSERT_CURSOR;
        valueX = (float)((long double)item->textRect.w + (long double)item->textRect.x + (long double)labelGap);
        paintOffset = editField->paintOffset;
        cursorColumn = coduo_int32_from_bits((uint32_t)item->cursorPos - (uint32_t)paintOffset);

        display = DC;
        display->drawTextWithCursor(valueX, item->textRect.y, item->font, item->textscale, color, buffer + paintOffset, cursorColumn,
                                    cursorCharacter, editField->maxPaintChars, item->textStyle);
    } else {
        float valueX = (float)((long double)item->textRect.w + (long double)item->textRect.x + (long double)labelGap);
        int32_t paintOffset = editField->paintOffset;

        display = DC;
        display->drawText(valueX, item->textRect.y, item->font, item->textscale, color, buffer + paintOffset, 0, editField->maxPaintChars,
                          item->textStyle);
    }
}
