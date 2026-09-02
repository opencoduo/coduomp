#include "console.h"

#include "cgame.h"

#include <string.h>

enum {
    CON_MINIMUM_VIDEO_WIDTH = 640,
    CON_GLYPH_WIDTH = 8,
    CON_HORIZONTAL_MARGIN_GLYPHS = 2,
    CON_DEFAULT_LINE_WIDTH = 78,
    CON_DEFAULT_TOTAL_LINES = 840
};

/* Source: CoDUOMP.exe 0x004093f0..0x004095d1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004093f0_004095d2.mcode.
 * Name and signature: exact same-module Mac symbol Con_CheckResize. */
void Con_CheckResize(void)
{
    uint16_t oldText[CON_TEXT_CELL_COUNT];
    int32_t videoWidth = cls.rendererConfig.vidWidth;
    int32_t newLineWidth;

    if (videoWidth < CON_MINIMUM_VIDEO_WIDTH)
        videoWidth = CON_MINIMUM_VIDEO_WIDTH;
    newLineWidth = videoWidth / CON_GLYPH_WIDTH - CON_HORIZONTAL_MARGIN_GLYPHS;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (newLineWidth > CON_TEXT_CELL_COUNT)
        newLineWidth = CON_TEXT_CELL_COUNT;

    if (newLineWidth == con.lineWidth)
        return;

    if (newLineWidth < 1) {
        con.lineWidth = CON_DEFAULT_LINE_WIDTH;
        con.totalLines = CON_DEFAULT_TOTAL_LINES;
        for (int32_t cell = 0; cell < CON_TEXT_CELL_COUNT; ++cell)
            con.text[cell] = CON_EMPTY_TEXT_CELL;
    } else {
        const int32_t oldLineWidth = con.lineWidth;
        const int32_t oldTotalLines = con.totalLines;
        const int32_t newTotalLines = CON_TEXT_CELL_COUNT / newLineWidth;
        const int32_t copyLineCount = oldTotalLines < newTotalLines ? oldTotalLines : newTotalLines;
        const int32_t copyColumnCount = oldLineWidth < newLineWidth ? oldLineWidth : newLineWidth;

        con.lineWidth = newLineWidth;
        con.totalLines = newTotalLines;
        memcpy(oldText, con.text, sizeof(oldText));
        for (int32_t cell = 0; cell < CON_TEXT_CELL_COUNT; ++cell)
            con.text[cell] = CON_EMPTY_TEXT_CELL;

        for (int32_t lineOffset = 0; lineOffset < copyLineCount; ++lineOffset) {
            const int32_t sourceLine = (con.currentLine - lineOffset + oldTotalLines) % oldTotalLines;
            const int32_t destinationLine = newTotalLines - lineOffset - 1;

            for (int32_t column = 0; column < copyColumnCount; ++column) {
                con.text[destinationLine * newLineWidth + column] = oldText[sourceLine * oldLineWidth + column];
            }
        }

        Con_ClearNotify();
        Con_ClearMiniConsole();
        Con_ClearSubtitles();
    }

    con.currentLine = con.totalLines - 1;
    con.displayLine = con.currentLine;
}
