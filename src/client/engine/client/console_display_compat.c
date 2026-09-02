#include "console.h"


#include "cgame.h"
#include "console_display_compat.h"
#include "widescreen_2d_compat.h"
#include "../renderer/renderer_api.h"

#include <string.h>

enum {
    CODUOMP_CONSOLE_STOCK_VIRTUAL_HEIGHT = 480,
    CODUOMP_CONSOLE_MINIMUM_VIRTUAL_WIDTH = 640,
    CODUOMP_CONSOLE_MINIMUM_VIRTUAL_HEIGHT = 480,
    CODUOMP_CONSOLE_GLYPH_WIDTH = 8,
    CODUOMP_CONSOLE_GLYPH_HEIGHT = 16,
    CODUOMP_CONSOLE_HORIZONTAL_MARGIN_GLYPHS = 2,
    CODUOMP_CONSOLE_DEFAULT_LINE_WIDTH = 78,
    CODUOMP_CONSOLE_DEFAULT_TOTAL_LINES = 840,
    CODUOMP_CONSOLE_TEXT_FONT = 5,
    CODUOMP_CONSOLE_TEXT_STYLE = 0,
    CODUOMP_CONSOLE_TEXT_BOTTOM_OFFSET = 48,
    CODUOMP_CONSOLE_INPUT_BOTTOM_OFFSET = 32,
    CODUOMP_CONSOLE_BACKSCROLL_STEP = 4,
    CODUOMP_CONSOLE_FIXED_INSERT_CURSOR = 10,
    CODUOMP_CONSOLE_FIXED_OVERSTRIKE_CURSOR = 11
};

#define CODUOMP_CONSOLE_TEXT_SCALE 0.3333333432674408f
#define CODUOMP_CONSOLE_SEPARATOR_ALPHA 0.6000000238418579f

/* NOT_FROM_ORIGINAL_SOURCE: uniform console canvas scale derived purely from
 * the drawable pixel height, so the fixed 8-by-16 glyph cells hold a constant
 * apparent size on high-resolution displays.  1080 pixels of height is the
 * reference where the stock drawable-pixel cells still read comfortably; at
 * or below it the scale is exactly 1.  Deriving from drawable pixels (never
 * from an SDL backing-scale query) applies the factor exactly once - the
 * earlier Retina double-scaling came from mixing the two sources. */
enum {
    CODUOMP_CONSOLE_REFERENCE_HEIGHT = 1080
};

float coduomp_console_canvas_scale_compat(void)
{
    if (cls.rendererConfig.vidHeight <= CODUOMP_CONSOLE_REFERENCE_HEIGHT)
        return 1.0f;

    return (float)cls.rendererConfig.vidHeight / (float)CODUOMP_CONSOLE_REFERENCE_HEIGHT;
}
/* NOT_FROM_ORIGINAL_SOURCE: return the dimensions of the console's uniform,
 * full-drawable canvas. */
static float coduomp_console_virtual_width(void)
{
    if (cls.rendererConfig.vidWidth <= 0 || cls.rendererConfig.vidHeight <= 0) {
        return (float)CODUOMP_CONSOLE_MINIMUM_VIRTUAL_WIDTH;
    }

    return (float)cls.rendererConfig.vidWidth / coduomp_console_canvas_scale_compat();
}

static float coduomp_console_virtual_height(void)
{
    if (cls.rendererConfig.vidWidth <= 0 || cls.rendererConfig.vidHeight <= 0) {
        return (float)CODUOMP_CONSOLE_MINIMUM_VIRTUAL_HEIGHT;
    }

    return (float)cls.rendererConfig.vidHeight / coduomp_console_canvas_scale_compat();
}

/* NOT_FROM_ORIGINAL_SOURCE: resize the console ring for the native-width,
 * height-scaled canvas. The copying and cursor rules intentionally match the
 * recovered stock Con_CheckResize path; only the source of the line width is
 * changed. */
static void coduomp_console_check_resize_compat(void)
{
    uint16_t oldText[CON_TEXT_CELL_COUNT];
    int32_t newLineWidth =
        (int32_t)(coduomp_console_virtual_width() / (float)CODUOMP_CONSOLE_GLYPH_WIDTH) - CODUOMP_CONSOLE_HORIZONTAL_MARGIN_GLYPHS;
    /* Keep this non-original native-width adapter inside the same complete
     * text-store domain enforced by Con_CheckResize. */
    if (newLineWidth > CON_TEXT_CELL_COUNT)
        newLineWidth = CON_TEXT_CELL_COUNT;

    if (newLineWidth == con.lineWidth)
        return;

    if (newLineWidth < 1) {
        con.lineWidth = CODUOMP_CONSOLE_DEFAULT_LINE_WIDTH;
        con.totalLines = CODUOMP_CONSOLE_DEFAULT_TOTAL_LINES;
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
    coduomp_console_manually_scrolled = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: draw one fixed console glyph directly in the
 * native-width virtual canvas. The stock small-character helper converts from
 * physical pixels, which would cancel the resolution-aware scale. */
static void coduomp_console_draw_glyph_compat(float x, float y, int32_t character, const float color[4])
{
    char text[2] = {(char)character, '\0'};

    rendererExports.TextPaint(x, y + (float)CODUOMP_CONSOLE_GLYPH_HEIGHT, CODUOMP_CONSOLE_TEXT_FONT, CODUOMP_CONSOLE_TEXT_SCALE, color,
                              text, (float)CODUOMP_CONSOLE_GLYPH_WIDTH, 0, CODUOMP_CONSOLE_TEXT_STYLE);
}

/* NOT_FROM_ORIGINAL_SOURCE: draw the editable console field using the same
 * canvas as the history. Compute a cursor-visible slice here because stock
 * Field_AdjustScroll measures its fixed-size field in physical 8-pixel cells. */
static void coduomp_console_draw_input_compat(float visibleHeight)
{
    const vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    const int32_t maximumVisible = con.lineWidth > 4 ? con.lineWidth - 4 : 1;
    int32_t firstCharacter = 0;
    const float y = visibleHeight - (float)CODUOMP_CONSOLE_INPUT_BOTTOM_OFFSET;

    if (cls.state != CA_DISCONNECTED && (cls.keyCatchers & KEYCATCH_CONSOLE) == 0) {
        return;
    }

    if (con_inputField.cursor > maximumVisible)
        firstCharacter = con_inputField.cursor - maximumVisible;

    rendererExports.SetColor(con_miniconColor);
    coduomp_console_draw_glyph_compat((float)CODUOMP_CONSOLE_GLYPH_WIDTH, y, ']', color);
    rendererExports.TextPaintWithCursor(
        (float)(CODUOMP_CONSOLE_GLYPH_WIDTH * 2), y + (float)CODUOMP_CONSOLE_GLYPH_HEIGHT, CODUOMP_CONSOLE_TEXT_FONT,
        CODUOMP_CONSOLE_TEXT_SCALE, color, &con_inputField.buffer[firstCharacter], con_inputField.cursor - firstCharacter,
        key_overstrikeMode != qfalse ? CODUOMP_CONSOLE_FIXED_OVERSTRIKE_CURSOR : CODUOMP_CONSOLE_FIXED_INSERT_CURSOR,
        (float)CODUOMP_CONSOLE_GLYPH_WIDTH, maximumVisible, CODUOMP_CONSOLE_TEXT_STYLE);
}

/* NOT_FROM_ORIGINAL_SOURCE: resolution-aware solid-console composition. Its
 * background still uses the stock full-640 picture helper (which spans the
 * drawable); text and input use the native-width uniform canvas selected by
 * the surrounding renderer command markers. */
static void coduomp_console_draw_solid_compat(float fraction)
{
    const float virtualWidth = coduomp_console_virtual_width();
    const float virtualHeight = coduomp_console_virtual_height();
    const float canvasScale = coduomp_console_canvas_scale_compat();
    float visibleHeight = fraction * virtualHeight;
    float stockBackgroundHeight = fraction * (float)CODUOMP_CONSOLE_STOCK_VIRTUAL_HEIGHT;
    int32_t backgroundHeight;
    int32_t rowCount;
    int32_t y;
    vec4_t color;

    if (visibleHeight <= 0.0f)
        return;
    if (cls.rendererConfig.vidHeight <= 0)
        return;
    if (visibleHeight > virtualHeight)
        visibleHeight = virtualHeight;
    if (stockBackgroundHeight > (float)CODUOMP_CONSOLE_STOCK_VIRTUAL_HEIGHT) {
        stockBackgroundHeight = (float)CODUOMP_CONSOLE_STOCK_VIRTUAL_HEIGHT;
    }

    con.xAdjust = (float)CODUOMP_CONSOLE_GLYPH_WIDTH * canvasScale;
    con.visiblePixelHeight = (int32_t)(visibleHeight * canvasScale);

    /* SCR_DrawPic and SCR_FillRect retain the stock 640x480 screen helpers;
     * keep their height in that coordinate space while console text uses the
     * separate resolution-aware canvas above. */
    backgroundHeight = (int32_t)(stockBackgroundHeight - 2.0f);
    if (backgroundHeight < 1) {
        backgroundHeight = 0;
    } else {
        SCR_DrawPic(0.0f, 0.0f, 640.0f, (float)backgroundHeight, cls.consoleShader);
    }

    {
        const vec4_t separatorColor = {0.0f, 0.0f, 0.0f, CODUOMP_CONSOLE_SEPARATOR_ALPHA};
        SCR_FillRect(0.0f, (float)backgroundHeight, 640.0f, 2.0f, separatorColor);
    }

    {
        const char *version = com_version->string;
        const int32_t versionLength = (int32_t)strlen(version);

        CL_LookupColor('7', color);
        rendererExports.TextPaint(virtualWidth - (float)(versionLength * CODUOMP_CONSOLE_GLYPH_WIDTH), visibleHeight - 2.0f,
                                  CODUOMP_CONSOLE_TEXT_FONT, CODUOMP_CONSOLE_TEXT_SCALE, color, version, (float)CODUOMP_CONSOLE_GLYPH_WIDTH,
                                  0, CODUOMP_CONSOLE_TEXT_STYLE);
    }

    rowCount = ((int32_t)visibleHeight - CODUOMP_CONSOLE_GLYPH_WIDTH) / CODUOMP_CONSOLE_GLYPH_WIDTH;
    y = (int32_t)visibleHeight - CODUOMP_CONSOLE_TEXT_BOTTOM_OFFSET;

    if (con.displayLine != con.currentLine) {
        CL_LookupColor('7', color);
        rendererExports.SetColor(color);
        for (int32_t column = 0; column < con.lineWidth; column += CODUOMP_CONSOLE_BACKSCROLL_STEP) {
            coduomp_console_draw_glyph_compat((float)((column + 2) * CODUOMP_CONSOLE_GLYPH_WIDTH), (float)y, '^', color);
        }
        y -= CODUOMP_CONSOLE_GLYPH_HEIGHT;
        --rowCount;
    }

    {
        int32_t displayLine = con.displayLine;

        if (con.lineCursor == 0)
            --displayLine;
        CL_LookupColor('7', color);

        for (int32_t row = 0; row < rowCount && displayLine >= 0; ++row, --displayLine, y -= CODUOMP_CONSOLE_GLYPH_HEIGHT) {
            const uint16_t *text;
            int32_t textRow;

            if (con.currentLine - displayLine >= con.totalLines)
                continue;

            textRow = displayLine % con.totalLines;
            text = &con.text[textRow * con.lineWidth];
            rendererExports.TextConsolePaint((float)CODUOMP_CONSOLE_GLYPH_WIDTH, (float)(y + CODUOMP_CONSOLE_GLYPH_WIDTH),
                                             CODUOMP_CONSOLE_TEXT_FONT, CODUOMP_CONSOLE_TEXT_SCALE, color, text,
                                             (float)CODUOMP_CONSOLE_GLYPH_WIDTH, con.lineWidth, CODUOMP_CONSOLE_TEXT_STYLE);
        }
    }

    coduomp_console_draw_input_compat(visibleHeight);
    rendererExports.SetColor(NULL);
}

/* NOT_FROM_ORIGINAL_SOURCE: default-build replacement for Con_DrawConsole.
 * It retains the recovered state/fraction policy while submitting one explicit
 * native-width console scope to the deferred renderer. */
void coduomp_console_draw_compat(void)
{
    float fraction = 0.0f;

    coduomp_console_check_resize_compat();

    if (cls.state == CA_DISCONNECTED) {
        if ((cls.keyCatchers & (KEYCATCH_UI | KEYCATCH_CGAME)) == 0)
            fraction = 1.0f;
    } else if (cls.state == CA_LOADING) {
        if (con_debug->integer != 0 && (cls.keyCatchers & KEYCATCH_UI) == 0) {
            fraction = 1.0f;
        }
    } else if (cls.state == CA_ACTIVE) {
        if (con.displayFrac == 0.0f)
            return;
        if (con_debug->integer == 2)
            fraction = con.displayFrac * 2.0f;
    }

    if (fraction == 0.0f)
        fraction = con.displayFrac;
    if (fraction == 0.0f)
        return;

    coduomp_queue_console_2d_presentation(qtrue);
    coduomp_console_rendering_compat_active = qtrue;
    coduomp_console_draw_solid_compat(fraction);
    coduomp_console_rendering_compat_active = qfalse;
    coduomp_queue_console_2d_presentation(qfalse);
}
