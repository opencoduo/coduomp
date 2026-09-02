#include "backend.h"

#include "gl_state.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum renderer_console_text_control_e {
    R_CONSOLE_TEXT_RED = 10,
    R_CONSOLE_TEXT_GREEN = 11,
    R_CONSOLE_TEXT_BLUE = 12,
    R_CONSOLE_ICON_RED = 13,
    R_CONSOLE_ICON_GREEN = 14,
    R_CONSOLE_ICON_BLUE = 15,
    R_CONSOLE_ICON_WIDTH = 16,
    R_CONSOLE_ICON_HEIGHT = 17,
    R_CONSOLE_ICON_SHADER = 18
};

enum {
    R_CONSOLE_ICON_NAME_SIZE = 64,
    R_DEFAULT_TEXT_COLOR_CODE = 7,
    R_CONSOLE_ICON_SHADER_LOAD_MODE = 5
};

/* Exact original binary32 constants. The comments retain their semantic
 * source forms without asking each host compiler to re-evaluate them. */
static const float r_consoleByteToUnit = 0.0039215688593685627f; /* 0x3b808081, approximately 1/255 */
static const float r_consoleIconBaseSize = 48.0f; /* 0x42400000 */
static const float r_consoleIconSizeUnit = 0.03125f; /* 0x3d000000, 1/32 */
static const float r_consoleIconBaselineHeight = 38.40000152587890625f; /* 0x4219999a, 48 * 0.8 */
static const float r_textColorByteScale = 255.0f; /* 0x437f0000 */

/* Original 0x0387ba90..0x0387be8f. R_Text_GetConsoleString deliberately
 * returns this shared scratch buffer rather than caller-owned storage. */
static char rendererConsoleTextBuffer[MAX_STRING_CHARS];

/* Source: CoDUOMP.exe 0x004e9470..0x004e9735.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9470_004e9736.mcode.
 * Name and source signature: exact same-module Mac symbol RE_Text_Width.
 * The Windows body inlines SEH_ReadCharFromString and
 * R_GetGlyphHorizAdvance; their language and glyph decisions remain explicit
 * below so the recovered behavior is independently auditable. */
int32_t RE_Text_Width(const char *text, int32_t fontHandle, float scale, float fixedAdvance, int32_t limit)
{
    fontInfo_t *font = R_GetFontInfo(fontHandle, scale);
    const float finalScale = fixedAdvance == 0.0f ? scale * font->glyphScale : fixedAdvance;
    float asianScale = 1.0f;
    long double lineWidth = 0.0L;
    float maximumWidth = 0.0f;
    int32_t glyphCount = 0;
    const char *cursor = text;

    if (rendererMultibyteTextEnabled != qfalse)
        asianScale = R_GetAsianScale(font, scale);

    if (cursor == NULL)
        return 0;
    if (limit <= 0)
        limit = INT_MAX;

    while (*cursor != '\0' && glyphCount < limit) {
        const uint8_t firstByte = (uint8_t)cursor[0];
        int32_t character = firstByte;

        if (rendererMultibyteTextEnabled != qfalse) {
            const uint8_t secondByte = (uint8_t)cursor[1];
            qboolean validPair = qfalse;

            switch ((language_t)cl_language->integer) {
            case LANGUAGE_KOREAN:
                validPair = firstByte >= 0xb0 && firstByte <= 0xc8 && secondByte >= 0xa1 && secondByte <= 0xfe;
                break;
            case LANGUAGE_TAIWANESE:
                validPair = ((firstByte >= 0xa1 && firstByte <= 0xc6) || (firstByte >= 0xc9 && firstByte <= 0xf9)) &&
                            ((secondByte >= 0x40 && secondByte <= 0x7e) || (secondByte >= 0xa1 && secondByte <= 0xfe));
                break;
            case LANGUAGE_JAPANESE:
                validPair = ((firstByte >= 0x81 && firstByte <= 0x9f) || (firstByte >= 0xe0 && firstByte <= 0xef)) &&
                            ((secondByte >= 0x40 && secondByte <= 0x7e) || (secondByte >= 0x80 && secondByte <= 0xfc));
                break;
            case LANGUAGE_CHINESE:
                validPair = firstByte >= 0xa1 && firstByte <= 0xf7 && secondByte >= 0xa1 && secondByte <= 0xfe;
                break;
            default:
                break;
            }

            if (validPair != qfalse) {
                character = ((int32_t)firstByte << 8) | secondByte;
                cursor += 2;
            } else {
                ++cursor;
            }
        } else {
            ++cursor;
        }

        if (character == '\n') {
            lineWidth = 0.0L;
            continue;
        }

        if (character == '^' && *cursor != '^' && *cursor >= '0' && *cursor <= '9') {
            ++cursor;
            continue;
        }

        if (fixedAdvance == 0.0f) {
            long double advance = (long double)R_GetGlyphHorizAdvance(font, character);

            if (character > 255)
                advance *= (long double)asianScale;
            lineWidth += advance;
        } else {
            lineWidth += 1.0L;
        }

        /* 0x004e9702 retains the x87 line-width sum through the maximum
         * comparison and only rounds when the new maximum is stored. */
        if (lineWidth > (long double)maximumWidth)
            maximumWidth = (float)lineWidth;
        ++glyphCount;
    }

    return (int32_t)(maximumWidth * finalScale);
}

/* Source: CoDUOMP.exe 0x004e9760..0x004e9781.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9760_004e9782.mcode.
 * This source function was absent from the initial Ghidra function export;
 * its entry, import call, x87 multiply, ftol tail call, and ending are direct
 * Windows-disassembly facts. Name: exact same-module Mac symbol. */
int32_t RE_Text_Height(int32_t fontHandle, float scale)
{
    fontInfo_t *font = R_GetFontInfo(fontHandle, scale);

    return (int32_t)(scale * font->lineHeight);
}

/* Source: CoDUOMP.exe 0x004e9790..0x004e97c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9790_004e97ca.mcode.
 * Name and wrapper shape: exact same-module Mac symbol RE_Text_Paint. */
void RE_Text_Paint(float x, float y, int32_t fontHandle, float scale, const float color[4], const char *text, float fixedAdvance,
                   int32_t limit, int32_t textStyle)
{
    RE_Text_PaintWithCursor(x, y, fontHandle, scale, color, text, -1, 0, fixedAdvance, limit, textStyle);
}

/* Source: CoDUOMP.exe 0x004e97d0..0x004e9960.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e97d0_004e9961.mcode.
 * Name and five-argument source signature: exact same-module Mac symbol
 * R_Text_GetConsoleString. */
static qboolean R_Text_GetConsoleString(const uint16_t *encoded, int32_t *remainingCount, const char **outText, const uint16_t **outNext,
                                        float color[4])
{
    int32_t outputIndex = 0;
    int32_t sourceIndex = 0;
    int32_t trailingSpaceStart = -1;
    int32_t currentColorCode = R_DEFAULT_TEXT_COLOR_CODE;
    qboolean stoppedAtIcon = qfalse;
    qboolean outputFull = qfalse;

    *outText = rendererConsoleTextBuffer;
    if (*remainingCount > MAX_STRING_CHARS - 1)
        *remainingCount = MAX_STRING_CHARS - 1;
    rendererConsoleTextBuffer[0] = '\0';

    while (sourceIndex < *remainingCount) {
        const uint16_t codeUnit = encoded[sourceIndex];
        const int32_t control = (codeUnit >> 8) & 0xff;
        const uint8_t value = (uint8_t)codeUnit;

        if (control == currentColorCode) {
            if (outputFull != qfalse || outputIndex >= MAX_STRING_CHARS - 1) {
                outputFull = qtrue;
                ++sourceIndex;
                continue;
            }
            rendererConsoleTextBuffer[outputIndex] = (char)value;
        } else if (control == R_CONSOLE_ICON_RED || control == R_CONSOLE_ICON_WIDTH || control == R_CONSOLE_ICON_HEIGHT ||
                   control == R_CONSOLE_ICON_SHADER) {
            trailingSpaceStart = -1;
            *outNext = &encoded[sourceIndex];
            stoppedAtIcon = qtrue;
            break;
        } else if (control == R_CONSOLE_TEXT_RED) {
            if (color != NULL)
                color[0] = (float)value * r_consoleByteToUnit;
            ++sourceIndex;
            continue;
        } else if (control == R_CONSOLE_TEXT_GREEN) {
            if (color != NULL)
                color[1] = (float)value * r_consoleByteToUnit;
            ++sourceIndex;
            continue;
        } else if (control == R_CONSOLE_TEXT_BLUE) {
            if (color != NULL)
                color[2] = (float)value * r_consoleByteToUnit;
            ++sourceIndex;
            continue;
        } else {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (outputFull != qfalse || outputIndex > MAX_STRING_CHARS - 4) {
                outputFull = qtrue;
                ++sourceIndex;
                continue;
            }
            rendererConsoleTextBuffer[outputIndex++] = '^';
            rendererConsoleTextBuffer[outputIndex++] = (char)(control + '0');
            currentColorCode = control;
            rendererConsoleTextBuffer[outputIndex] = (char)value;
        }

        if (value == ' ') {
            if (trailingSpaceStart == -1)
                trailingSpaceStart = outputIndex;
        } else {
            trailingSpaceStart = -1;
        }
        ++outputIndex;
        ++sourceIndex;
    }

    rendererConsoleTextBuffer[outputIndex] = '\0';
    if (trailingSpaceStart >= 0)
        rendererConsoleTextBuffer[trailingSpaceStart] = '\0';

    if (sourceIndex == *remainingCount)
        *outNext = NULL;
    else
        *outNext = &encoded[sourceIndex];
    *remainingCount -= sourceIndex;
    return stoppedAtIcon;
}

/* Source: CoDUOMP.exe 0x004e9970..0x004e9acc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9970_004e9acd.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * R_Text_GetConsoleIcon. The cursor is an owning typed view of the pointer
 * slot that the i386 body advances by two bytes per consumed code unit. */
static void R_Text_GetConsoleIcon(const uint16_t *encoded, int32_t *remainingCount, const uint16_t **cursor, float scale, float *width,
                                  float *height, int32_t *shaderHandle, float color[4])
{
    char shaderName[R_CONSOLE_ICON_NAME_SIZE];
    int32_t sourceIndex = 0;

    *width = r_consoleIconBaseSize * scale;
    *height = r_consoleIconBaseSize * scale;
    *shaderHandle = 0;
    memset(shaderName, 0, sizeof(shaderName));

    while (sourceIndex < *remainingCount) {
        uint16_t codeUnit = encoded[sourceIndex];
        int32_t control = (codeUnit >> 8) & 0xff;
        uint8_t value = (uint8_t)codeUnit;

        switch (control) {
        case R_CONSOLE_ICON_RED:
            if (color != NULL)
                color[0] = (float)value * r_consoleByteToUnit;
            ++*cursor;
            break;
        case R_CONSOLE_ICON_GREEN:
            if (color != NULL)
                color[1] = (float)value * r_consoleByteToUnit;
            ++*cursor;
            break;
        case R_CONSOLE_ICON_BLUE:
            if (color != NULL)
                color[2] = (float)value * r_consoleByteToUnit;
            ++*cursor;
            break;
        case R_CONSOLE_ICON_WIDTH:
            *width *= (float)value * r_consoleIconSizeUnit;
            ++*cursor;
            break;
        case R_CONSOLE_ICON_HEIGHT:
            *height *= (float)value * r_consoleIconSizeUnit;
            ++*cursor;
            break;
        case R_CONSOLE_ICON_SHADER: {
            char *nameCursor = shaderName;
            const uint16_t *shaderCursor = &encoded[sourceIndex];
            qboolean nameTooLong = qfalse;

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            while (control == R_CONSOLE_ICON_SHADER && value != 0 && sourceIndex < *remainingCount - 1) {
                if (nameCursor < shaderName + sizeof(shaderName) - 1) {
                    *nameCursor++ = (char)value;
                } else {
                    nameTooLong = qtrue;
                }
                ++shaderCursor;
                ++sourceIndex;
                ++*cursor;
                codeUnit = *shaderCursor;
                control = (codeUnit >> 8) & 0xff;
                value = (uint8_t)codeUnit;
            }

            if (nameTooLong == qfalse) {
                *shaderHandle = RE_RegisterShaderNoMip(shaderName, R_CONSOLE_ICON_SHADER_LOAD_MODE);
            }
            *remainingCount -= sourceIndex;
            return;
        }
        default:
            break;
        }

        ++sourceIndex;
    }
}

/* Source: CoDUOMP.exe 0x004e9af0..0x004e9b21.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9af0_004e9b22.mcode.
 * Name: exact same-module Mac symbol R_DrawStrlen. */
int32_t R_DrawStrlen(const char *text)
{
    int32_t length = 0;

    while (*text != '\0') {
        if (*text == '^' && text[1] != '\0' && text[1] != '^' && text[1] >= '0' && text[1] <= '9') {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            text += 2;
        } else {
            ++length;
            ++text;
        }
    }

    return length;
}

/* Source: CoDUOMP.exe 0x004e9b30..0x004e9c1c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9b30_004e9c1d.mcode.
 * Name and source signature: exact same-module Mac symbol
 * RE_Text_ConsoleWidth. */
int32_t RE_Text_ConsoleWidth(const uint16_t *encoded, int32_t fontHandle, float scale, float fixedAdvance, int32_t encodedCount)
{
    const uint16_t *cursor = encoded;
    float width = 0.0f;

    while (cursor != NULL) {
        const char *text;
        const qboolean hasIcon = R_Text_GetConsoleString(cursor, &encodedCount, &text, &cursor, NULL);

        if (text != NULL && *text != '\0') {
            if (fixedAdvance == 0.0f) {
                width += (float)RE_Text_Width(text, fontHandle, scale, fixedAdvance, encodedCount);
            } else {
                width += (float)R_DrawStrlen(text) * fixedAdvance;
            }
        }

        if (hasIcon != qfalse) {
            float iconWidth;
            float iconHeight;
            int32_t shaderHandle;

            R_Text_GetConsoleIcon(cursor, &encodedCount, &cursor, scale, &iconWidth, &iconHeight, &shaderHandle, NULL);
            width += iconWidth;
        }
    }

    return (int32_t)width;
}

/* Source: CoDUOMP.exe 0x004e9c20..0x004e9c69.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9c20_004e9c6a.mcode.
 * This was a third real function hidden in the original executable-gap record.
 * Name and source signature: exact same-module Mac symbol
 * R_Text_PaintConsoleIcon. */
static void R_Text_PaintConsoleIcon(float x, float y, float width, float height, int32_t shaderHandle)
{
    ri.AdjustFrom640(&x, &y, &width, &height);
    RE_StretchPic(x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, shaderHandle);
}

/* Source: CoDUOMP.exe 0x004e9c70..0x004e9e1e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9c70_004e9e1f.mcode.
 * Name and source signature: exact same-module Mac symbol
 * RE_Text_ConsolePaint. Windows LTCG inlines R_Text_PaintConsoleIcon here. */
void RE_Text_ConsolePaint(float x, float y, int32_t fontHandle, float scale, const float color[4], const uint16_t *encoded,
                          float fixedAdvance, int32_t encodedCount, int32_t textStyle)
{
    float currentColor[4] = {color[0], color[1], color[2], color[3]};
    const uint16_t *cursor = encoded;
    float xOffset = 0.0f;

    while (cursor != NULL) {
        const char *text;
        const qboolean hasIcon = R_Text_GetConsoleString(cursor, &encodedCount, &text, &cursor, currentColor);

        if (text != NULL && *text != '\0') {
            RE_Text_Paint(x + xOffset, y, fontHandle, scale, currentColor, text, fixedAdvance, 0, textStyle);
            if (hasIcon != qfalse) {
                xOffset += (float)RE_Text_Width(text, fontHandle, scale, fixedAdvance, encodedCount);
            }
        }

        if (hasIcon != qfalse) {
            float iconWidth;
            float iconHeight;
            int32_t shaderHandle;
            float iconY;

            R_Text_GetConsoleIcon(cursor, &encodedCount, &cursor, scale, &iconWidth, &iconHeight, &shaderHandle, currentColor);
            RE_SetColor(currentColor);
            iconY = y - (iconHeight + scale * r_consoleIconBaselineHeight) * 0.5f;
            R_Text_PaintConsoleIcon(x + xOffset, iconY, iconWidth, iconHeight, shaderHandle);
            xOffset += iconWidth;
        }
    }
}

/* Source: CoDUOMP.exe 0x004e9e20..0x004e9f04.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9e20_004e9f05.mcode.
 * Name and complete source signature: exact same-module Mac symbol
 * RE_Text_PaintWithCursor. */
void RE_Text_PaintWithCursor(float x, float y, int32_t fontHandle, float scale, const float color[4], const char *text,
                             int32_t cursorPosition, uint8_t cursorCharacter, float fixedAdvance, int32_t limit, int32_t textStyle)
{
    uint32_t textLength;
    uint32_t commandSize;
    text_paint_command_t *command;

    if (text == NULL)
        return;

    textLength = (uint32_t)strlen(text);
    if (limit > 0 && (int32_t)textLength > limit)
        textLength = (uint32_t)limit;

    commandSize = (uint32_t)offsetof(text_paint_command_t, text) + textLength + 1u;
    commandSize = (commandSize + 3u) & ~3u;
    command = (text_paint_command_t *)R_GetCommandBuffer((int32_t)commandSize);
    if (command == NULL)
        return;

    command->commandId = RC_TEXT_PAINT_WITH_CURSOR;
    command->x = x;
    command->y = y;
    command->fontHandle = fontHandle;
    command->scale = scale;
    command->color.components[0] = (uint8_t)(color[0] * r_textColorByteScale);
    command->color.components[1] = (uint8_t)(color[1] * r_textColorByteScale);
    command->color.components[2] = (uint8_t)(color[2] * r_textColorByteScale);
    command->color.components[3] = (uint8_t)(color[3] * r_textColorByteScale);
    command->fixedAdvance = fixedAdvance;
    command->textStyle = textStyle;
    command->cursorPosition = cursorPosition;
    command->cursorCharacter = cursorCharacter;
    memcpy(command->text, text, textLength);
    command->text[textLength] = '\0';
}
