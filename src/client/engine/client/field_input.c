#include "console.h"

#include "cgame.h"
#include "../platform/crt_boundary.h"
#include "qcommon/q_string.h"
#include "../renderer/renderer_api.h"
#include "../ui/ui_module_loader.h"

#include <stdlib.h>
#include <string.h>

enum {
    FIELD_CHAR_HOME = 1,
    FIELD_CHAR_CLEAR = 3,
    FIELD_CHAR_END = 5,
    FIELD_CHAR_BACKSPACE = 8,
    FIELD_CHAR_PASTE = 22
};

static const float fieldFontScale =
    0.02083333395421505f; /* 0x3caaaaab, approximately 1/48 */
static const float fieldVirtualWidth = 640.0f;  /* 0x44200000 */
static const float fieldVirtualHeight = 480.0f; /* 0x43f00000 */

enum {
    FIELD_DEFAULT_VISIBLE_CHARS = CON_INPUT_BUFFER_SIZE,
    FIELD_FIXED_FONT_HANDLE = 5,
    FIELD_FIXED_TEXT_STYLE = 0,
    FIELD_VARIABLE_TEXT_STYLE = 3,
    FIELD_FIXED_INSERT_CURSOR = 10,
    FIELD_FIXED_OVERSTRIKE_CURSOR = 11
};

/* Source: CoDUOMP.exe 0x0040d6d0..0x0040d822.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040d6d0_0040d823.mcode.
 * Name: exact same-module Mac symbol Field_Draw. The Win32 function receives
 * field in ESI and x/y on the stack; the maintained source uses an ordinary
 * portable signature. */
void Field_Draw(console_input_field_t *field, int32_t x, int32_t y)
{
    char visibleText[MAX_STRING_CHARS];
    const vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    const int32_t copyExtent =
        CON_INPUT_BUFFER_SIZE - field->scroll;
    float drawX = (float)x;
    float drawY = (float)y + field->charHeight;
    float scale = field->charHeight * fieldFontScale;
    float fixedAdvance = field->charWidth;
    int32_t fontHandle;
    int32_t textStyle;
    uint8_t cursorCharacter;

    strncpy(visibleText, &field->buffer[field->scroll],
            (size_t)(uint32_t)(copyExtent - 1));
    visibleText[copyExtent - 1] = '\0';

    if (field->fixedSize != qfalse) {
        const float horizontalScale =
            fieldVirtualWidth / (float)cls.rendererConfig.vidWidth;
        const float verticalScale =
            fieldVirtualHeight / (float)cls.rendererConfig.vidHeight;

        drawX *= horizontalScale;
        drawY *= verticalScale;
        scale *= verticalScale;
        fixedAdvance *= horizontalScale;
        fontHandle = FIELD_FIXED_FONT_HANDLE;
        textStyle = FIELD_FIXED_TEXT_STYLE;
        cursorCharacter = key_overstrikeMode != qfalse
            ? FIELD_FIXED_OVERSTRIKE_CURSOR
            : FIELD_FIXED_INSERT_CURSOR;
    } else {
        fontHandle = 0;
        textStyle = FIELD_VARIABLE_TEXT_STYLE;
        cursorCharacter =
            key_overstrikeMode != qfalse ? (uint8_t)'_' : (uint8_t)'|';
    }

    if (field->widthInChars == 0)
        field->widthInChars = FIELD_DEFAULT_VISIBLE_CHARS;

    rendererExports.TextPaintWithCursor(
        drawX, drawY, fontHandle, scale, color, visibleText,
        field->cursor - field->scroll, cursorCharacter, fixedAdvance,
        field->widthInChars, textStyle);
}

/* Source: CoDUOMP.exe 0x0040da40..0x0040da8f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040da40_0040da90.mcode.
 * Name and signature: exact same-module Mac symbol Field_Paste. The system
 * clipboard function returns malloc-owned storage, matching the original call
 * to the statically linked CRT free routine after all characters are inserted. */
void Field_Paste(console_input_field_t *field)
{
    char *clipboard = Sys_GetClipboardData();
    if (clipboard == NULL)
        return;

    const int32_t length = (int32_t)strlen(clipboard);
    for (int32_t index = 0; index < length; ++index) {
        Field_CharEvent(field, (signed char)clipboard[index]);
    }
    free(clipboard);
}

/* Source: CoDUOMP.exe 0x0040da90..0x0040dc36.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040da90_0040dc37.mcode.
 * Name and signature: exact same-module Mac symbol Field_KeyDownEvent. */
void Field_KeyDownEvent(console_input_field_t *field, int32_t key)
{
    const int32_t length = (int32_t)strlen(field->buffer);

    if ((key == K_INS || key == K_KP_INS) &&
        keyStates[K_SHIFT].down != qfalse) {
        Field_Paste(field);
    } else if (key == K_DEL) {
        if (field->cursor < length) {
            memmove(&field->buffer[field->cursor],
                    &field->buffer[field->cursor + 1],
                    (size_t)(length - field->cursor));
        }
    } else if (key == K_RIGHTARROW) {
        if (field->cursor < length)
            ++field->cursor;

        if (keyStates[K_CTRL].down != qfalse) {
            while (field->cursor < length && coduo_crt_isspace(
                       (int32_t)(signed char)field->buffer[field->cursor])) {
                ++field->cursor;
            }
            while (field->cursor < length && !coduo_crt_isspace(
                       (int32_t)(signed char)field->buffer[field->cursor])) {
                ++field->cursor;
            }
        }
    } else if (key == K_LEFTARROW) {
        if (field->cursor > 0)
            --field->cursor;

        if (keyStates[K_CTRL].down != qfalse) {
            while (field->cursor > 0 && coduo_crt_isspace(
                       (int32_t)(signed char)field->buffer[field->cursor - 1])) {
                --field->cursor;
            }
            while (field->cursor > 0 && !coduo_crt_isspace(
                       (int32_t)(signed char)field->buffer[field->cursor - 1])) {
                --field->cursor;
            }
        }

        if (field->cursor < field->scroll)
            field->scroll = field->cursor;
    } else if (key == K_HOME ||
               (coduo_crt_tolower(key) == 'a' &&
                keyStates[K_CTRL].down != qfalse)) {
        field->cursor = 0;
    } else if (key == K_END ||
               (coduo_crt_tolower(key) == 'e' &&
                keyStates[K_CTRL].down != qfalse)) {
        field->cursor = length;
    } else if (key == K_INS) {
        key_overstrikeMode = key_overstrikeMode == qfalse ? qtrue : qfalse;
    }

    if (coduo_uiVm != NULL)
        Field_AdjustScroll(field);
}

/* Source: CoDUOMP.exe 0x0040d830..0x0040da31.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040d830_0040da32.mcode.
 * Name and signature: exact same-module Mac symbol Field_AdjustScroll. */
void Field_AdjustScroll(console_input_field_t *field)
{
    float scale = field->charHeight * fieldFontScale;
    float fixedAdvance = field->charWidth;
    float availableWidth = (float)field->widthInPixels;
    int32_t fontHandle = 0;

    if (field->fixedSize != qfalse) {
        const float horizontalScale =
            fieldVirtualWidth / (float)cls.rendererConfig.vidWidth;

        scale *= fieldVirtualHeight / (float)cls.rendererConfig.vidHeight;
        fixedAdvance *= horizontalScale;
        availableWidth *= horizontalScale;
        fontHandle = 5;
    }

    if ((float)rendererExports.TextWidth(field->buffer, fontHandle, scale,
                                          fixedAdvance, 0) < availableWidth) {
        field->scroll = 0;
        field->widthInChars = SEH_PrintStrlen(field->buffer);
        return;
    }

    if (availableWidth > 0.0f) {
        while (field->scroll > 0 &&
               (float)rendererExports.TextWidth(
                   &field->buffer[field->scroll - 1], fontHandle, scale,
                   fixedAdvance, 0) < availableWidth) {
            --field->scroll;
        }
    }

    for (;;) {
        int32_t visibleWidth =
            rendererExports.TextWidth(&field->buffer[field->scroll],
                                      fontHandle, scale, fixedAdvance, 0) -
            rendererExports.TextWidth(&field->buffer[field->cursor],
                                      fontHandle, scale, fixedAdvance, 0);

        if (visibleWidth < 0) {
            if (field->scroll == 0) {
                visibleWidth = 0;
            } else {
                --field->scroll;
            }
        } else if ((float)visibleWidth >= availableWidth) {
            ++field->scroll;
        }

        if (visibleWidth < 0 || (float)visibleWidth >= availableWidth)
            continue;

        const int32_t remainingLength =
            (int32_t)strlen(&field->buffer[field->scroll]);
        field->widthInChars = field->cursor - field->scroll;

        if (availableWidth > 0.0f) {
            while (field->widthInChars < remainingLength &&
                   (float)rendererExports.TextWidth(
                       &field->buffer[field->scroll], fontHandle, scale,
                       fixedAdvance, field->widthInChars + 1) <
                       availableWidth) {
                ++field->widthInChars;
            }
        }
        return;
    }
}

/* Source: CoDUOMP.exe 0x0040dc40..0x0040dd4a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040dc40_0040dd4b.mcode.
 * Name and signature: exact same-module Mac symbol Field_CharEvent. */
void Field_CharEvent(console_input_field_t *field, int32_t character)
{
    int32_t length = (int32_t)strlen(field->buffer);

    if (character == FIELD_CHAR_PASTE) {
        Field_Paste(field);
        Field_AdjustScroll(field);
        return;
    }

    if (character == FIELD_CHAR_CLEAR) {
        memset(field->buffer, 0, sizeof(field->buffer));
        field->cursor = 0;
        field->scroll = 0;
        field->widthInChars = CON_INPUT_BUFFER_SIZE;
        Field_AdjustScroll(field);
        return;
    }

    if (character == FIELD_CHAR_BACKSPACE) {
        if (field->cursor > 0) {
            memmove(&field->buffer[field->cursor - 1],
                    &field->buffer[field->cursor],
                    (size_t)(length - field->cursor + 1));
            --field->cursor;
        }
        Field_AdjustScroll(field);
        return;
    }

    if (character == FIELD_CHAR_HOME) {
        field->cursor = 0;
        field->scroll = 0;
        return;
    }

    if (character == FIELD_CHAR_END) {
        field->cursor = length;
        Field_AdjustScroll(field);
        return;
    }

    if (character < ' ')
        return;

    if (key_overstrikeMode != qfalse) {
        if (field->cursor == CON_INPUT_BUFFER_SIZE - 1)
            return;
        field->buffer[field->cursor] = (char)character;
    } else {
        if (length == CON_INPUT_BUFFER_SIZE - 1)
            return;
        memmove(&field->buffer[field->cursor + 1],
                &field->buffer[field->cursor],
                (size_t)(length - field->cursor + 1));
        field->buffer[field->cursor] = (char)character;
    }

    ++field->cursor;
    ++length;
    if (field->cursor == length)
        field->buffer[field->cursor] = '\0';
    Field_AdjustScroll(field);
}
