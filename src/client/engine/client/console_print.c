#include "console.h"

#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "../platform/punkbuster_boundary.h"
#include "../server/server.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    CON_DEFAULT_COLOR = 7,
    CON_PRINT_LINE_BUFFER_SIZE = 4096,
    CON_COLOR_COMPONENT_BYTE_MAX = 255,
    CON_SUBTITLE_DEFAULT_TIME_MSEC = 5000,
    CON_DEATH_COLOR_FIRST_TAG = 10,
    CON_DEATH_VICTIM_COLOR_FIRST_TAG = 13,
    CON_DEATH_VICTIM_NAME_TAG = 18,
    CON_DEATH_ICON_SIZE_SCALE = 32,
    CON_DEATH_ICON_WIDTH_TAG = 0x1000,
    CON_DEATH_ICON_HEIGHT_TAG = 0x1100
};

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
static qboolean coduomp_console_add_encoded_cell(uint16_t cell)
{
    uint64_t cellIndex;
    int32_t row;

    if (con.lineWidth <= 0 || con.totalLines <= 0 || con.lineCursor < 0 || con.lineCursor >= con.lineWidth) {
        return qfalse;
    }

    row = con.currentLine % con.totalLines;
    if (row < 0) {
        return qfalse;
    }
    cellIndex = (uint64_t)(uint32_t)row * (uint32_t)con.lineWidth + (uint32_t)con.lineCursor;
    if (cellIndex >= CON_TEXT_CELL_COUNT) {
        return qfalse;
    }

    con.text[(size_t)cellIndex] = cell;
    ++con.lineCursor;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0040a360..0x0040a390.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a360_0040a391.mcode.
 * Role name: the Mac traceback table has no separate symbol. The helper
 * appends one packed console metadata cell; MSVC inlines it into the two
 * retained CL_AddConsoleInfo functions below. */
static void CL_AddConsoleInfoCell(uint8_t value, uint8_t tag)
{
    const uint16_t cell = (uint16_t)(((uint16_t)tag << 8) | (uint16_t)value);

    (void)coduomp_console_add_encoded_cell(cell);
}

/* Source: CoDUOMP.exe 0x0040a3a0..0x0040a49a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a3a0_0040a49b.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_AddConsoleInfoColor. Death-message records encode each RGB component in
 * the low byte of a console cell and its consecutive metadata tag in the high
 * byte. */
void CL_AddConsoleInfoColor(const vec3_t color, int32_t firstComponentTag)
{
    for (int32_t component = 0; component < 3; ++component) {
        /* 0x0040a3a0..0x0040a454 leaves the float product live in x87 and
         * calls the MSVC _ftol2 helper, which truncates toward zero. */
        int32_t byteValue = coduo_fp_to_i32_extended((long double)color[component] * (long double)CON_COLOR_COMPONENT_BYTE_MAX);

        if (byteValue < 0)
            byteValue = 0;
        else if (byteValue > CON_COLOR_COMPONENT_BYTE_MAX)
            byteValue = CON_COLOR_COMPONENT_BYTE_MAX;

        const uint8_t tag = (uint8_t)(firstComponentTag + component);
        CL_AddConsoleInfoCell((uint8_t)byteValue, tag);
    }
}

/* Source: CoDUOMP.exe 0x0040a4a0..0x0040a53e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a4a0_0040a53f.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_AddConsoleInfoChar. A negative initial color enables inline ^n color
 * changes and otherwise the caller-supplied color remains fixed. Newline and
 * carriage-return bytes are metadata separators and are not stored. */
void CL_AddConsoleInfoChar(const char *text, int32_t initialColor)
{
    int32_t color = initialColor < 0 ? CON_DEFAULT_COLOR : initialColor;

    while (*text != '\0') {
        const int32_t character = (int32_t)(int8_t)*text;

        if (character == '^' && text[1] != '\0' && text[1] != '^' && text[1] >= '0' && text[1] <= '9') {
            if (initialColor < 0)
                color = ColorIndex(text[1]);
            text += 2;
            continue;
        }

        ++text;
        if (character == '\n' || character == '\r')
            continue;

        CL_AddConsoleInfoCell((uint8_t)character, (uint8_t)color);
    }
}

/* Source: CoDUOMP.exe 0x00409dc0..0x00409fd7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409dc0_00409fd8.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_ConsolePrint_AddLine. Each text cell stores the unsigned input byte in
 * its low byte and the active color index in its high byte. */
int32_t CL_ConsolePrint_AddLine(const char *text, console_message_destination_t destination, int32_t messageTime, int32_t lineWidth,
                                int32_t initialColor)
{
    int32_t balancedLineWidth;
    int32_t color = initialColor;
    qboolean lineWrapped = qfalse;

    if (lineWidth <= 0 || lineWidth > con.lineWidth)
        lineWidth = con.lineWidth;
    balancedLineWidth = lineWidth;

    if (destination == CON_DEST_GAME_MESSAGE || destination == CON_DEST_BOLD_GAME_MESSAGE) {
        const int32_t visibleLength = SEH_PrintStrlen(text);
        if (visibleLength > lineWidth) {
            const float visibleLengthFloat = (float)visibleLength;
            const int32_t lineCount = (int32_t)ceil((double)visibleLengthFloat / (double)lineWidth);
            balancedLineWidth = (int32_t)rint((double)visibleLengthFloat / (double)lineCount);
        }
    }

    if (destination != con.lastMessageDestination && con.lineCursor > 0) {
        Con_Linefeed(con.lastMessageDestination, messageTime);
    }

    const uint8_t *cursor = (const uint8_t *)text;
    while (*cursor != '\0') {
        const uint8_t character = *cursor;

        if (character == '^' && cursor[1] != '\0' && cursor[1] != '^' && cursor[1] >= '0' && cursor[1] <= '9') {
            color = ColorIndex((char)cursor[1]);
            cursor += 2;
            continue;
        }

        int32_t wordLength = 0;
        while (wordLength < lineWidth && cursor[wordLength] > (uint8_t)' ') {
            ++wordLength;
        }
        if (wordLength != lineWidth && con.lineCursor + wordLength > lineWidth) {
            Con_Linefeed(destination, messageTime);
            lineWrapped = qtrue;
        }

        ++cursor;
        if (character == '\n') {
            Con_Linefeed(destination, messageTime);
            lineWrapped = qtrue;
            continue;
        }
        if (character == '\r') {
            con.lineCursor = 0;
            continue;
        }
        if (con.lineCursor == 0 && character == ' ' && lineWrapped != qfalse) {
            continue;
        }

        const int32_t row = con.currentLine % con.totalLines;
        con.text[row * con.lineWidth + con.lineCursor] = (uint16_t)(((uint16_t)color << 8) | character);
        ++con.lineCursor;

        if (con.lineCursor >= lineWidth || (con.lineCursor >= balancedLineWidth && character == ' ')) {
            Con_Linefeed(destination, messageTime);
            lineWrapped = qtrue;
        }
    }

    if (con.lineCursor > 0) {
        if (destination != CON_DEST_MINICONSOLE) {
            Con_Linefeed(destination, messageTime);
        } else {
            Con_UpdateNotifyLine(CON_DEST_MINICONSOLE, qfalse, 0);
        }
    }

    con.lastMessageDestination = destination;
    return color;
}

/* Source: CoDUOMP.exe 0x00409fe0..0x0040a15b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409fe0_0040a15c.mcode.
 * Name and signature: exact same-module Mac symbol CL_ConsolePrint. Game and
 * bold-game messages are split at newlines so each segment retains the color
 * left active by the preceding segment. */
void CL_ConsolePrint(const char *text, console_message_destination_t destination, int32_t messageTime, int32_t lineWidth)
{
    if (cl_noprint != NULL && cl_noprint->integer != 0)
        return;
    if (destination == CON_DEST_NONE)
        return;
    if (con.initialized == qfalse)
        Con_OneTimeInit();

    if (messageTime == 0) {
        switch (destination) {
        case CON_DEST_MINICONSOLE:
            messageTime = FastRound(con_miniconTime->value * 1000.0f);
            break;
        case CON_DEST_GAME_MESSAGE:
            messageTime = FastRound(con_gameMessageTime->value * 1000.0f);
            break;
        case CON_DEST_BOLD_GAME_MESSAGE:
            messageTime = FastRound(con_boldGameMessageTime->value * 1000.0f);
            break;
        case CON_DEST_SUBTITLE:
            messageTime = CON_SUBTITLE_DEFAULT_TIME_MSEC;
            break;
        default:
            break;
        }
    }
    if (messageTime < 0)
        messageTime = 0;

    int32_t color = CON_DEFAULT_COLOR;
    if (destination == CON_DEST_GAME_MESSAGE || destination == CON_DEST_BOLD_GAME_MESSAGE) {
        const char *newline = strchr(text, '\n');
        while (newline != NULL) {
            char line[CON_PRINT_LINE_BUFFER_SIZE];
            size_t byteCount = (size_t)(newline - text) + 1;
            if (byteCount >= sizeof(line))
                byteCount = sizeof(line) - 1;
            memcpy(line, text, byteCount);
            line[byteCount] = '\0';

            color = CL_ConsolePrint_AddLine(line, destination, messageTime, lineWidth, color);
            text = newline + 1;
            newline = strchr(text, '\n');
        }
    }

    (void)CL_ConsolePrint_AddLine(text, destination, messageTime, lineWidth, color);
}

/* Source: CoDUOMP.exe 0x0040a540..0x0040a705.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a540_0040a706.mcode.
 * Name and signature: exact same-module Mac symbol CL_DeathMessagePrint.
 * MSVC also inlines this helper in CL_CgameSystemCalls at
 * 0x004021aa..0x0040235f. The two scalar metadata cells use the original x87
 * truncation path and the 0x10/0x11 console-cell tags. */
void CL_DeathMessagePrint(console_message_destination_t destination, const char *attackerName, const float *attackerColor,
                          const char *weaponName, const float *weaponColor, const char *victimName, float weaponIconWidth,
                          float weaponIconHeight, const float *victimColor, int32_t messageTime)
{
    if (cl_noprint != NULL && cl_noprint->integer != 0)
        return;
    if (destination == CON_DEST_NONE)
        return;
    if (con.initialized == qfalse)
        Con_OneTimeInit();

    if (messageTime == 0) {
        switch (destination) {
        case CON_DEST_MINICONSOLE:
            messageTime = FastRound(con_miniconTime->value * 1000.0f);
            break;
        case CON_DEST_GAME_MESSAGE:
            messageTime = FastRound(con_gameMessageTime->value * 1000.0f);
            break;
        case CON_DEST_BOLD_GAME_MESSAGE:
            messageTime = FastRound(con_boldGameMessageTime->value * 1000.0f);
            break;
        case CON_DEST_SUBTITLE:
            messageTime = CON_SUBTITLE_DEFAULT_TIME_MSEC;
            break;
        default:
            break;
        }
    }
    if (messageTime < 0)
        messageTime = 0;

    if (con.lineCursor > 0)
        Con_Linefeed(con.lastMessageDestination, messageTime);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (attackerName[0] != '\0') {
        CL_AddConsoleInfoColor(attackerColor, CON_DEATH_COLOR_FIRST_TAG);
        CL_AddConsoleInfoChar(attackerName, CON_DEFAULT_COLOR);
        CL_AddConsoleInfoChar(" ", CON_DEFAULT_COLOR);
    }

    CL_AddConsoleInfoColor(victimColor, CON_DEATH_VICTIM_COLOR_FIRST_TAG);

    const uint16_t widthCell =
        (uint16_t)((uint16_t)coduo_fp_to_i32_extended((long double)weaponIconWidth * (long double)CON_DEATH_ICON_SIZE_SCALE) |
                   CON_DEATH_ICON_WIDTH_TAG);
    (void)coduomp_console_add_encoded_cell(widthCell);

    const uint16_t heightCell =
        (uint16_t)((uint16_t)coduo_fp_to_i32_extended((long double)weaponIconHeight * (long double)CON_DEATH_ICON_SIZE_SCALE) |
                   CON_DEATH_ICON_HEIGHT_TAG);
    (void)coduomp_console_add_encoded_cell(heightCell);

    CL_AddConsoleInfoChar(victimName, CON_DEATH_VICTIM_NAME_TAG);
    CL_AddConsoleInfoChar(" ", CON_DEFAULT_COLOR);
    CL_AddConsoleInfoColor(weaponColor, CON_DEATH_COLOR_FIRST_TAG);
    CL_AddConsoleInfoChar(weaponName, CON_DEFAULT_COLOR);

    if (destination != CON_DEST_MINICONSOLE) {
        Con_Linefeed(destination, messageTime);
    } else {
        Con_UpdateNotifyLine(CON_DEST_MINICONSOLE, qfalse, messageTime);
    }
    con.lastMessageDestination = destination;
}

/* Source: CoDUOMP.exe 0x0040a170..0x0040a2c3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a170_0040a2c4.mcode.
 * Name and signature: exact same-module Mac symbol PbMsgToScreen. The client
 * and server PunkBuster message adapters pass their persistent label buffer
 * first and the incoming message second. */
void PbMsgToScreen(const char *prefix, const char *message)
{
    if (dedicated != NULL && dedicated->integer != 0) {
        Com_Printf("%s: %s\n", prefix, message);
        return;
    }

    CL_ConsolePrint(va("%s: %s", prefix, message), CON_DEST_GAME_MESSAGE, 0, 0);
}

/* Source: CoDUOMP.exe 0x0040a2d0..0x0040a35a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a2d0_0040a35b.mcode.
 * Name and signature: exact same-module Mac symbol CL_ConsoleFixPosition.
 * Printing the newline can advance currentLine; the display position is then
 * fixed relative to that resulting line even when cl_noprint suppresses it. */
void CL_ConsoleFixPosition(void)
{
    CL_ConsolePrint("\n", CON_DEST_MINICONSOLE, 0, 0);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (coduomp_console_manually_scrolled == qfalse)
        con.displayLine = con.currentLine;
}
