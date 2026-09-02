#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"

#include <string.h>

enum {
    UI_TEXT_ALIGN_CENTER = 1,
    UI_TEXT_ALIGN_RIGHT = 2,
    UI_TEXT_ALIGN_CENTER_WITH_VALUE = 3,
    ITEM_TEXT_CVAR_BUFFER_SIZE = 256
};

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30055630 and
 * uo_ui_mp_x86.dll 0x40017190. */
extern displayContextDef_t *DC;

void Item_SetTextExtents(itemDef_t *item, int32_t *width, int32_t *height, const char *text)
{
    displayContextDef_t *display;
    int32_t fullWidth;

    if (text == NULL) {
        text = item->text;
        if (text == NULL)
            return;
    }

    *width = coduo_fp_to_i32_extended((long double)item->textRect.w);
    *height = coduo_fp_to_i32_extended((long double)item->textRect.h);
    if (*width != 0 && item->textalignment != UI_TEXT_ALIGN_CENTER_WITH_VALUE &&
        !((item->type == ITEM_TYPE_OWNERDRAW || item->cvar != NULL) && item->textalignment == UI_TEXT_ALIGN_CENTER)) {
        return;
    }

    {
        float scale = item->textscale;
        int32_t font = item->font;

        display = DC;
        fullWidth = display->textWidth(text, font, scale, 0);
    }
    if (item->type == ITEM_TYPE_OWNERDRAW && (item->textalignment == UI_TEXT_ALIGN_CENTER || item->textalignment == UI_TEXT_ALIGN_RIGHT)) {
        float scale = item->textscale;
        int32_t font = item->font;
        int32_t ownerDraw = item->window.ownerDraw;
        int32_t extraWidth;

        display = DC;
        extraWidth = display->ownerDrawWidth(ownerDraw, font, scale);
        fullWidth = coduo_int32_from_bits((uint32_t)fullWidth + (uint32_t)extraWidth);
    } else if ((item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD || item->type == ITEM_TYPE_UPREDITFIELD) &&
               item->textalignment == UI_TEXT_ALIGN_CENTER && item->cvar != NULL) {
        char cvarBuffer[ITEM_TEXT_CVAR_BUFFER_SIZE];
        const char *cvar = item->cvar;
        int32_t extraWidth;

        display = DC;
        display->getCVarString(cvar, cvarBuffer, ITEM_TEXT_CVAR_BUFFER_SIZE);
        {
            float scale = item->textscale;
            int32_t font = item->font;

            display = DC;
            extraWidth = display->textWidth(cvarBuffer, font, scale, 0);
        }
        fullWidth = coduo_int32_from_bits((uint32_t)fullWidth + (uint32_t)extraWidth);
    } else if (item->textalignment == UI_TEXT_ALIGN_CENTER_WITH_VALUE) {
        float scale = item->textscale;
        int32_t font = item->font;
        int32_t extraWidth;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        display = DC;
        extraWidth = display->textWidth(text, font, scale, 0);
        fullWidth = coduo_int32_from_bits((uint32_t)fullWidth + (uint32_t)extraWidth);
    }

    {
        float scale = item->textscale;
        int32_t font = item->font;
        int32_t measuredWidth;
        int32_t measuredHeight;

        display = DC;
        measuredWidth = display->textWidth(text, font, scale, 0);
        display = DC;
        *width = measuredWidth;
        scale = item->textscale;
        font = item->font;
        measuredHeight = display->textHeight(font, scale);
        *height = measuredHeight;
    }

    {
        int32_t widthForCache = *width;
        uint32_t alignYBits;
        uint32_t alignXBits;
        int32_t heightForCache;
        int32_t alignment;

        memcpy(&alignYBits, &item->textaligny, sizeof(alignYBits));
        memcpy(&alignXBits, &item->textalignx, sizeof(alignXBits));
        item->textRect.w = (float)widthForCache;
        heightForCache = *height;
        memcpy(&item->textRect.y, &alignYBits, sizeof(alignYBits));
        alignment = item->textalignment;
        item->textRect.h = (float)heightForCache;
        memcpy(&item->textRect.x, &alignXBits, sizeof(alignXBits));

        if (alignment == UI_TEXT_ALIGN_RIGHT) {
            /* bare FILD at 0x4001736d: fullWidth stays an exact integer. */
            item->textRect.x = (float)((long double)item->textalignx - (long double)fullWidth);
        } else if (alignment == UI_TEXT_ALIGN_CENTER || alignment == UI_TEXT_ALIGN_CENTER_WITH_VALUE) {
            /* bare FILD at 0x40017388: fullWidth/2 stays an exact integer. */
            item->textRect.x = (float)((long double)item->textalignx - (long double)(fullWidth / 2));
        }
    }

    if (item->window.border != 0) {
        item->textRect.x += item->window.borderSize;
        item->textRect.y += item->window.borderSize;
    }
    item->textRect.x = item->window.rect.x + item->textRect.x;
    item->textRect.y = item->window.rect.y + item->textRect.y;
}
