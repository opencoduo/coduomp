#include "ui_runtime.h"

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30055fc0 and
 * uo_ui_mp_x86.dll 0x40017b20. */
extern displayContextDef_t *DC;

void Item_Text_Paint(itemDef_t *item)
{
    char cvarBuffer[MAX_STRING_CHARS];
    vec4_t color;
    int32_t width;
    int32_t height;
    displayContextDef_t *display;
    const char *text = item->text;

    if (text == NULL) {
        char *cursor;
        const char *cvar = item->cvar;

        if (cvar == NULL)
            return;
        display = DC;
        display->getCVarString(cvar, cvarBuffer, MAX_STRING_CHARS);
        if ((item->window.flags & WINDOW_TEXTCVARSHORT) != 0) {
            cursor = cvarBuffer;
            {
                char character = *cursor;

                while (character != '\0' && character != '.') {
                    *cursor = character;
                    character = cursor[1];
                    ++cursor;
                }
            }
            *cursor = '\0';
        }
        text = cvarBuffer;
    }

    if (text[0] == '@') {
        display = DC;
        text = display->getLocalizedString(text + 1);
    }

    Item_TextColor(item, color);
    Item_SetTextExtents(item, &width, &height, text);
    if (text[0] == '\0')
        return;

    {
        int32_t flags = item->window.flags;

        if ((flags & WINDOW_WRAPPED) != 0) {
            Item_Text_Wrapped_Paint(item, text, color);
        } else if ((flags & WINDOW_AUTOWRAPPED) != 0) {
            Item_Text_AutoWrapped_Paint(item, text, color);
        } else {
            display = DC;
            display->drawText(item->textRect.x, item->textRect.y, item->font, item->textscale, color, text, 0, 0, item->textStyle);
        }
    }
}
