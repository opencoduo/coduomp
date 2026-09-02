#include "console.h"

#include "cgame.h"
#include "../math/vector_math.h"
#include "../renderer/renderer_api.h"

#include <string.h>

enum {
    CON_VIRTUAL_WIDTH = 640,
    CON_VIRTUAL_HEIGHT = 480,
    CON_SMALL_CHAR_WIDTH = 8,
    CON_SMALL_CHAR_HEIGHT = 16,
    CON_VERSION_BASELINE_OFFSET = 18,
    CON_TEXT_BOTTOM_OFFSET = 48,
    CON_INPUT_BOTTOM_OFFSET = 32,
    CON_BACKSCROLL_INDICATOR_STEP = 4,
    CON_TEXT_FONT = 5,
    CON_TEXT_STYLE = 0
};

/* Exact stored Win32 float constants. SCREEN_SCALE_X is semantically 1/640;
 * CONSOLE_TEXT_SCALE is semantically 1/3. */
#define SCREEN_SCALE_X 0.0015625000232830644f /* 0x3acccccd */
#define CONSOLE_TEXT_SCALE 0.3333333432674408f /* 0x3eaaaaab */
#define CONSOLE_SEPARATOR_ALPHA 0.6000000238418579f /* 0x3f19999a */

/* Source: CoDUOMP.exe 0x0040a720..0x0040a77c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a720_0040a77d.mcode.
 * Name and source boundary: exact same-module Mac symbol Con_DrawInput.
 * MSVC also inlines this helper at the end of Con_DrawSolidConsole. */
void Con_DrawInput(void)
{
    if (cls.state != CA_DISCONNECTED && (cls.keyCatchers & KEYCATCH_CONSOLE) == 0) {
        return;
    }

    const int32_t y = con.visiblePixelHeight - CON_INPUT_BOTTOM_OFFSET;

    rendererExports.SetColor(con_miniconColor);
    SCR_DrawSmallChar(FastRound(con.xAdjust), y, ']');
    Field_Draw(&con_inputField, FastRound(con.xAdjust) + CON_SMALL_CHAR_WIDTH, y);
}

/* Source: CoDUOMP.exe 0x0040afa0..0x0040b290.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040afa0_0040b291.mcode.
 * Name and argument: exact same-module Mac symbol Con_DrawSolidConsole. The
 * Win32 optimizer inlines the console-string and input-line drawing wrappers. */
void Con_DrawSolidConsole(float fraction)
{
    int32_t pixelHeight = (int32_t)((float)cls.rendererConfig.vidHeight * fraction);
    if (pixelHeight <= 0)
        return;
    if (pixelHeight > cls.rendererConfig.vidHeight)
        pixelHeight = cls.rendererConfig.vidHeight;

    con.xAdjust = (float)cls.rendererConfig.vidWidth * SCREEN_SCALE_X * (float)CON_SMALL_CHAR_WIDTH;

    int32_t backgroundHeight = (int32_t)(fraction * (float)CON_VIRTUAL_HEIGHT - 2.0f);
    if (backgroundHeight < 1) {
        backgroundHeight = 0;
    } else {
        SCR_DrawPic(0.0f, 0.0f, (float)CON_VIRTUAL_WIDTH, (float)backgroundHeight, cls.consoleShader);
    }

    const vec4_t separatorColor = {0.0f, 0.0f, 0.0f, CONSOLE_SEPARATOR_ALPHA};
    SCR_FillRect(0.0f, (float)backgroundHeight, (float)CON_VIRTUAL_WIDTH, 2.0f, separatorColor);

    vec4_t color;
    const char *version = com_version->string;
    const int32_t versionLength = (int32_t)strlen(version);
    CL_LookupColor('7', color);
    SCR_DrawSmallStringExt(cls.rendererConfig.vidWidth - versionLength * CON_SMALL_CHAR_WIDTH, pixelHeight - CON_VERSION_BASELINE_OFFSET,
                           version, color);

    int32_t rowCount = (pixelHeight - CON_SMALL_CHAR_WIDTH) / CON_SMALL_CHAR_WIDTH;
    int32_t y = pixelHeight - CON_TEXT_BOTTOM_OFFSET;
    con.visiblePixelHeight = pixelHeight;

    if (con.displayLine != con.currentLine) {
        CL_LookupColor('7', color);
        rendererExports.SetColor(color);
        for (int32_t column = 0; column < con.lineWidth; column += CON_BACKSCROLL_INDICATOR_STEP) {
            SCR_DrawSmallChar((int32_t)con.xAdjust + column * CON_SMALL_CHAR_WIDTH + CON_SMALL_CHAR_WIDTH, y, '^');
        }
        y -= CON_SMALL_CHAR_HEIGHT;
        --rowCount;
    }

    int32_t displayLine = con.displayLine;
    if (con.lineCursor == 0)
        --displayLine;
    CL_LookupColor('7', color);

    const float inverseXScale = (float)CON_VIRTUAL_WIDTH / (float)cls.rendererConfig.vidWidth;
    const float inverseYScale = (float)CON_VIRTUAL_HEIGHT / (float)cls.rendererConfig.vidHeight;
    for (int32_t row = 0; row < rowCount && displayLine >= 0; ++row, --displayLine, y -= CON_SMALL_CHAR_HEIGHT) {
        if (con.currentLine - displayLine >= con.totalLines)
            continue;

        const int32_t textRow = displayLine % con.totalLines;
        const uint16_t *text = &con.text[textRow * con.lineWidth];
        rendererExports.TextConsolePaint((float)(int32_t)con.xAdjust * inverseXScale, (float)(y + CON_SMALL_CHAR_WIDTH) * inverseYScale,
                                         CON_TEXT_FONT, inverseYScale * CONSOLE_TEXT_SCALE, color, text,
                                         inverseXScale * (float)CON_SMALL_CHAR_WIDTH, con.lineWidth, CON_TEXT_STYLE);
    }

    Con_DrawInput();

    rendererExports.SetColor(NULL);
}
