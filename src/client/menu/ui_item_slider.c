#include "ui_runtime.h"

#include "compat/coduo_native_x87.h"

#include <stddef.h>
#include <stdint.h>

enum {
    UI_SLIDER_TEXT_GAP = 8,
    UI_SLIDER_TRACK_WIDTH = 96,
    UI_SLIDER_USABLE_WIDTH = 84,
    UI_SLIDER_THUMB_CENTER = 6,
    UI_SLIDER_THUMB_HALF_WIDTH = 5,
    UI_SLIDER_THUMB_WIDTH = 10,
    UI_SLIDER_THUMB_TOP_OFFSET = 2,
    UI_SLIDER_THUMB_HEIGHT = 20,
    UI_SLIDER_PULSE_PERIOD = 75,
    UI_SLIDER_THUMB_REGION = WINDOW_LB_THUMB
};

extern displayContextDef_t *DC;

void Com_Printf(const char *format, ...);
void Item_Text_Paint(itemDef_t *item);

/*
 * This complete slider geometry/input/paint cluster is instruction-identical
 * between the original Windows client modules after rebasing image-local
 * globals, constants, strings, and calls:
 *
 *                                   cgame       UI
 * Item_Slider_ThumbPosition         0x30052d80  0x400148d0
 * Item_Slider_OverSlider            0x30052e50  0x400149a0
 * Item_Slider_HandleKey             0x30054880  0x400163e0
 * Item_Slider_Paint                 0x30056c80  0x400187e0
 */
long double Item_Slider_ThumbPosition(itemDef_t *item)
{
    editFieldDef_t *editField;
    float origin;
    long double value;

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
                   "ITEM_TYPE_YESNO, ITEM_TYPE_BIND, ITEM_TYPE_SLIDER, "
                   "or ITEM_TYPE_TEXT\n");
        return 0.0L;
    }

    editField = (editFieldDef_t *)item->typeData;
    if (editField == NULL) {
        return 0.0L;
    }

    if (item->text != NULL) {
        origin = item->textRect.x + item->textRect.w + UI_SLIDER_TEXT_GAP;
    } else {
        origin = item->window.rect.x;
    }

    value = DC->getCVarValue(item->cvar);
    if (value < (long double)editField->minVal) {
        value = editField->minVal;
    } else if (value > (long double)editField->maxVal) {
        value = editField->maxVal;
    }

    return ((value - (long double)editField->minVal) / ((long double)editField->maxVal - (long double)editField->minVal)) *
               UI_SLIDER_USABLE_WIDTH +
           (long double)origin + UI_SLIDER_THUMB_CENTER;
}

int32_t Item_Slider_OverSlider(itemDef_t *item, float x, float y)
{
    const long double left = Item_Slider_ThumbPosition(item) - UI_SLIDER_THUMB_HALF_WIDTH;
    const float top = item->window.rect.y - UI_SLIDER_THUMB_TOP_OFFSET;

    if ((long double)x < left || (long double)x > left + UI_SLIDER_THUMB_WIDTH || y < top || y > top + UI_SLIDER_THUMB_HEIGHT) {
        return 0;
    }
    return UI_SLIDER_THUMB_REGION;
}

qboolean Item_Slider_HandleKey(itemDef_t *item, int32_t key)
{
    editFieldDef_t *editField;
    float origin;
    long double value;

    if ((item->window.flags & WINDOW_HASFOCUS) == 0 || item->cvar == NULL ||
        !Rect_ContainsPoint(&item->window.rect, (float)DC->cursorx, (float)DC->cursory)) {
        return qfalse;
    }
    if (key != K_MOUSE1 && key != K_ENTER && key != K_MOUSE2 && key != K_MOUSE3) {
        return qfalse;
    }

    editField = Item_GetEditFieldDef(item);
    if (editField == NULL) {
        return qfalse;
    }
    if (item->text != NULL) {
        origin = item->textRect.x + item->textRect.w + UI_SLIDER_TEXT_GAP;
    } else {
        origin = item->window.rect.x;
    }
    if (!Rect_ContainsPoint(&item->window.rect, (float)DC->cursorx, (float)DC->cursory)) {
        return qfalse;
    }

    value = (long double)editField->minVal +
            (((long double)(float)DC->cursorx - (long double)origin) * (long double)(1.0f / (float)UI_SLIDER_TRACK_WIDTH)) *
                ((long double)editField->maxVal - (long double)editField->minVal);
    DC->setCVar(item->cvar, va("%f", (double)value));
    return qtrue;
}

void Item_Slider_Paint(itemDef_t *item)
{
    vec4_t color;
    float trackX;
    float rectY;
    const char *cvar = item->cvar;
    menuDef_t *parent = item->parent;

    if (cvar != NULL) {
        (void)DC->getCVarValue(cvar);
    }

    if ((item->window.flags & WINDOW_HASFOCUS) != 0) {
        vec4_t dimmed;
        int32_t component;
        const int32_t phase = DC->realTime / UI_SLIDER_PULSE_PERIOD;
        const float fraction = (float)((coduo_x87_sinl((long double)phase) + 1.0L) * 0.5L);

        for (component = 0; component < 4; ++component) {
            dimmed[component] = parent->focusColor[component] * 0.8f;
        }
        LerpColor(color, parent->focusColor, dimmed, fraction);
    } else {
        color[0] = item->window.foreColor[0];
        color[1] = item->window.foreColor[1];
        color[2] = item->window.foreColor[2];
        color[3] = item->window.foreColor[3];
    }

    rectY = item->window.rect.y;
    if (item->text != NULL) {
        Item_Text_Paint(item);
        trackX = (float)((long double)item->textRect.w + (long double)item->textRect.x + UI_SLIDER_TEXT_GAP);
    } else {
        trackX = item->window.rect.x;
    }

    {
        displayContextDef_t *display = DC;
        qhandle_t sliderBar;

        display->setColor(color);
        display = DC;
        sliderBar = display->sliderBar;
        display->drawHandlePic(trackX, rectY, UI_SLIDER_TRACK_WIDTH, 16.0f, sliderBar);
    }

    {
        const long double thumbX = Item_Slider_ThumbPosition(item);
        const long double thumbY = (long double)rectY - UI_SLIDER_THUMB_TOP_OFFSET;
        displayContextDef_t *display = DC;
        const qhandle_t sliderThumb = display->sliderThumb;

        display->drawHandlePic((float)(thumbX - UI_SLIDER_THUMB_HALF_WIDTH), (float)thumbY, UI_SLIDER_THUMB_WIDTH, UI_SLIDER_THUMB_HEIGHT,
                               sliderThumb);
    }
}
