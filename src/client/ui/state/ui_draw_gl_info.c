#include "../module/ui_functions.h"

enum {
    UI_GL_INFO_X_INSET = 2,
    UI_GL_INFO_VERSION_Y = 15,
    UI_GL_INFO_PIXEL_FORMAT_Y = 30,
    UI_GL_INFO_EXTENSIONS_Y = 45,
    UI_GL_INFO_ROW_HEIGHT = 10,
    UI_GL_INFO_BOTTOM_MARGIN = 11,
    UI_GL_INFO_HEADER_LIMIT = 80,
    UI_GL_INFO_EXTENSION_LIMIT = 36,
    UI_GL_INFO_TEXT_STYLE = 0,
    UI_GL_INFO_MAX_WORDS = 256,
    UI_GL_INFO_EXTENSION_BUFFER_SIZE = 4096
};

// Source: uo_ui_mp_x86.dll 0x4000a890..0x4000abe5
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000a890_4000abe5.mcode
// Exact same-module PPC symbol: UI_DrawGLInfo.
void UI_DrawGLInfo(const rectDef_t *rect, int32_t font, float scale,
                   const vec4_t color, int32_t textStyle)
{
    char extensions[UI_GL_INFO_EXTENSION_BUFFER_SIZE];
    char *words[UI_GL_INFO_MAX_WORDS];
    int32_t wordCount = 0;
    int32_t wordIndex = 0;
    float y;
    char *cursor;

    trap_R_Text_Paint(rect->x + UI_GL_INFO_X_INSET, rect->y, font, scale,
                      color,
                      va("VENDOR: %s",
                         ui_displayContextStorage.context.glConfig.vendorString),
                      UI_GL_INFO_TEXT_STYLE, UI_GL_INFO_HEADER_LIMIT,
                      textStyle);
    trap_R_Text_Paint(
        rect->x + UI_GL_INFO_X_INSET, rect->y + UI_GL_INFO_VERSION_Y,
        font, scale, color,
        va("VERSION: %s: %s",
           ui_displayContextStorage.context.glConfig.versionString,
           ui_displayContextStorage.context.glConfig.rendererString),
        UI_GL_INFO_TEXT_STYLE, UI_GL_INFO_HEADER_LIMIT, textStyle);
    trap_R_Text_Paint(
        rect->x + UI_GL_INFO_X_INSET, rect->y + UI_GL_INFO_PIXEL_FORMAT_Y,
        font, scale, color,
        va("PIXELFORMAT: color(%d-bits) Z(%d-bits) stencil(%d-bits)",
           ui_displayContextStorage.context.glConfig.colorBits,
           ui_displayContextStorage.context.glConfig.depthBits,
           ui_displayContextStorage.context.glConfig.stencilBits),
        UI_GL_INFO_TEXT_STYLE, UI_GL_INFO_HEADER_LIMIT, textStyle);

    /* strncpy(dst, src, 0xfff) + dst[0xfff] = 0: 4095 characters survive. */
    Q_strncpyz(extensions,
               ui_displayContextStorage.context.glConfig.extensionsString,
               UI_GL_INFO_EXTENSION_BUFFER_SIZE);
    y = rect->y + UI_GL_INFO_EXTENSIONS_Y;

    cursor = extensions;
    while (y < rect->y + rect->h && *cursor != '\0' &&
           wordCount < UI_GL_INFO_MAX_WORDS) {
        while (*cursor == ' ') {
            *cursor++ = '\0';
        }
        if (*cursor == '\0') {
            break;
        }
        words[wordCount++] = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            ++cursor;
        }
    }

    while (wordIndex < wordCount) {
        trap_R_Text_Paint(rect->x + UI_GL_INFO_X_INSET, y, font, scale,
                          color, words[wordIndex++], UI_GL_INFO_TEXT_STYLE,
                          UI_GL_INFO_EXTENSION_LIMIT, textStyle);
        if (wordIndex < wordCount) {
            trap_R_Text_Paint(rect->x + rect->w * 0.33333334f, y, font,
                              scale, color, words[wordIndex++],
                              UI_GL_INFO_TEXT_STYLE,
                              UI_GL_INFO_EXTENSION_LIMIT, textStyle);
        }
        if (wordIndex < wordCount) {
            trap_R_Text_Paint(rect->x + (rect->w * 0.33333334f) * 2.0f, y,
                              font, scale, color, words[wordIndex++],
                              UI_GL_INFO_TEXT_STYLE,
                              UI_GL_INFO_EXTENSION_LIMIT, textStyle);
        }

        y += UI_GL_INFO_ROW_HEIGHT;
        /* The row loop keeps painting while y <= rect->y + rect->h - 11;
         * it stops only when the limit is strictly below y. */
        if (rect->y + rect->h - UI_GL_INFO_BOTTOM_MARGIN < y) {
            break;
        }
    }
}
