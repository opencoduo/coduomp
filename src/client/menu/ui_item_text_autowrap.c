#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"

#include <math.h>

enum {
    UI_TEXT_ALIGN_CENTER = 1,
    UI_TEXT_ALIGN_RIGHT = 2,
    UI_TEXT_LINE_SPACING = 5
};

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30055ad0 and
 * uo_ui_mp_x86.dll 0x40017630. */
extern displayContextDef_t *DC;

void Item_Text_AutoWrapped_Paint(itemDef_t *item, const char *text, const vec4_t color)
{
    char line[MAX_STRING_CHARS];
    displayContextDef_t *display;
    int32_t lineHeight;
    int32_t fullWidth;
    int32_t maxWidth;
    int32_t textWidth;
    int32_t breakWidth = 0;
    int32_t breakIndex = 0;
    int32_t length = 0;
    const char *cursor = text;
    const char *breakCursor = text;
    float y;

    {
        int32_t font = item->font;
        display = DC;
        lineHeight = display->textHeight(font, item->textscale);
    }
    {
        int32_t font = item->font;
        display = DC;
        fullWidth = display->textWidth(text, font, item->textscale, 0);
    }
    float fullWidthFloat = (float)fullWidth; /* FST [esp+0xc], FILD value retained */
    if ((long double)fullWidth > item->window.rect.w) {
        int32_t columns = coduo_fp_to_i32_extended((long double)ceil((double)((long double)fullWidthFloat / item->window.rect.w)));

        /* 0x400176bd FILD keeps columns exact in the division; fullWidthFloat
         * is the float scratch reloaded at 0x4001769f. */
        maxWidth = coduo_fp_to_i32_extended((long double)fullWidthFloat / (long double)columns);
    } else {
        maxWidth = coduo_fp_to_i32_extended((long double)item->window.rect.w);
    }

    y = item->textaligny;
    textWidth = fullWidth;
    line[0] = '\0';
    if (cursor == NULL)
        return;

    for (;;) {
        char character = *cursor;
        qboolean flush;
        qboolean lineFull;

        if (character == ' ' || character == '\t' || character == '\n' || character == '\0') {
            breakWidth = textWidth;
            breakCursor = cursor + 1;
            breakIndex = length;
        }

        {
            float scale = item->textscale;
            int32_t font = item->font;
            display = DC;
            textWidth = display->textWidth(line, font, scale, 0);
        }
        /* NOT_FROM_ORIGINAL_SOURCE: force a visual line break at the shared
         * text capacity. An unbroken word uses the current source position;
         * prior whitespace retains the normal wrapping preference. */
        lineFull = length >= MAX_STRING_CHARS - 1 ? qtrue : qfalse;
        if (lineFull != qfalse && breakIndex == 0) {
            breakWidth = textWidth;
            breakCursor = cursor;
            breakIndex = length;
        }
        flush = lineFull != qfalse || (breakIndex != 0 && (long double)textWidth > item->window.rect.w) || character == '\n' ||
                character == '\0' || (character == ' ' && textWidth > maxWidth);

        if (!flush) {
            line[length] = character;
            length = coduo_int32_from_bits((uint32_t)length + 1u);
            ++cursor;
            if (character == '\r')
                line[length - 1] = ' ';
            line[length] = '\0';
            continue;
        }

        if (length != 0) {
            if (item->textalignment == UI_TEXT_ALIGN_CENTER) {
                /* bare FILD at 0x400177b5/0x4001779e: breakWidth stays an
                 * exact integer. */
                item->textRect.x = (float)((long double)item->textalignx - breakWidth / 2);
            } else if (item->textalignment == UI_TEXT_ALIGN_RIGHT) {
                item->textRect.x = (float)((long double)item->textalignx - breakWidth);
            } else {
                item->textRect.x = item->textalignx;
            }
            item->textRect.y = y;
            if (item->window.border != 0) {
                item->textRect.x = item->window.borderSize + item->textRect.x;
                item->textRect.y = item->window.borderSize + item->textRect.y;
            }
            item->textRect.x = item->window.rect.x + item->textRect.x;
            line[breakIndex] = '\0';
            item->textRect.y = item->window.rect.y + item->textRect.y;
            display = DC;
            display->drawText(item->textRect.x, item->textRect.y, item->font, item->textscale, color, line, 0, 0, item->textStyle);
        }

        if (*cursor == '\0')
            return;
        /* bare FILD at 0x4001787f: lineHeight+spacing stays an exact int. */
        {
            int32_t lineStep = coduo_int32_from_bits((uint32_t)lineHeight + UI_TEXT_LINE_SPACING);
            y = (float)((long double)lineStep + (long double)y);
        }
        cursor = breakCursor;
        breakIndex = 0;
        length = 0;
        breakWidth = 0;
    }
}
