#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "qcommon/q_string.h"

#include <string.h>

enum {
    UI_TEXT_ALIGN_CENTER = 1,
    UI_TEXT_ALIGN_RIGHT = 2,
    UI_TEXT_LINE_SPACING = 5,
    UI_TEXT_HARD_NEWLINE = '\r'
};

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30055d50 and
 * uo_ui_mp_x86.dll 0x400178b0. */
extern displayContextDef_t *DC;

void Item_Text_Wrapped_Paint(itemDef_t *item, const char *text,
                             const vec4_t color)
{
    char line[MAX_STRING_CHARS];
    displayContextDef_t *display;
    int32_t font = item->font;
    display = DC;
    int32_t lineHeight = display->textHeight(font, item->textscale);
    float y = item->textRect.y;
    const char *lineStart = text;
    const char *newline = strchr(lineStart, UI_TEXT_HARD_NEWLINE);

    while (newline != NULL) {
        const size_t lineLength = (size_t)(newline - lineStart);
        const int32_t copyLength = lineLength < MAX_STRING_CHARS
                                       ? (int32_t)lineLength
                                       : MAX_STRING_CHARS - 1;
        float x;

        if (*newline == '\0') break;
        /* NOT_FROM_ORIGINAL_SOURCE: keep the rendered line within the shared
         * text capacity, including its terminator. */
        Q_strncpyz(line, lineStart, copyLength + 1);
        line[copyLength] = '\0';

        if (item->textalignment == UI_TEXT_ALIGN_CENTER) {
            int32_t width;

            font = item->font;
            display = DC;
            width = display->textWidth(line, font, item->textscale, 0);

            /* bare FILD at 0x40017956: width stays an exact integer. */
            x = (float)((long double)item->textRect.x +
                        ((long double)item->textRect.w -
                         (long double)width) * 0.5f);
        } else if (item->textalignment == UI_TEXT_ALIGN_RIGHT) {
            /* Class 1: the DLL spills textRect.x+textRect.w to a float slot
             * across the textWidth call (0x4001799b) and reloads it before
             * the width subtract (bare FILD, 0x400179ac). */
            float rightEdge = item->textRect.w + item->textRect.x;
            int32_t width;

            font = item->font;
            display = DC;
            width = display->textWidth(line, font, item->textscale, 0);
            x = (float)((long double)rightEdge - (long double)width);
        } else {
            x = item->textRect.x;
        }

        display = DC;
        display->drawText(
            x, y, item->font, item->textscale, color, line,
            0, 0, item->textStyle);
        /* bare FILD at 0x40017a0b: lineHeight+spacing stays an exact int. */
        {
            int32_t lineStep = coduo_int32_from_bits(
                (uint32_t)lineHeight + UI_TEXT_LINE_SPACING);
            y = (float)((long double)lineStep + (long double)y);
        }
        lineStart = newline + 1;
        newline = strchr(lineStart, UI_TEXT_HARD_NEWLINE);
    }

    {
        float x;

        if (item->textalignment == UI_TEXT_ALIGN_CENTER) {
            int32_t width;

            font = item->font;
            display = DC;
            width = display->textWidth(lineStart, font, item->textscale, 0);

            /* bare FILD at 0x40017a59: width stays an exact integer. */
            x = (float)((long double)item->textRect.x +
                        ((long double)item->textRect.w -
                         (long double)width) * 0.5f);
        } else if (item->textalignment == UI_TEXT_ALIGN_RIGHT) {
            /* Class 1: the DLL spills textRect.x+textRect.w to a float slot
             * across the textWidth call (0x40017a9e) and reloads it before
             * the width subtract (bare FILD, 0x40017aab). */
            float rightEdge = item->textRect.w + item->textRect.x;
            int32_t width;

            font = item->font;
            display = DC;
            width = display->textWidth(lineStart, font, item->textscale, 0);
            x = (float)((long double)rightEdge - (long double)width);
        } else {
            x = item->textRect.x;
        }

        display = DC;
        display->drawText(
            x, y, item->font, item->textscale, color, lineStart,
            0, 0, item->textStyle);
    }
}
