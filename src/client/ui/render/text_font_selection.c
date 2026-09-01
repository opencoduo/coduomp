#include "../module/ui_functions.h"

typedef enum {
    UI_FONT_BIG = 2,
    UI_FONT_SMALL = 3,
    UI_FONT_BOLD = 4,
    UI_FONT_CONSOLE = 5
} uiFontMode_t;

// Source: uo_ui_mp_x86.dll 0x40008930..0x400089b4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008930_400089b4.mcode
// Role name: Text_GetFont.
fontInfo_t *Text_GetFont(int32_t font, float scale)
{
    float scaledSize;

    if (font == UI_FONT_BIG) return &ui_displayContextStorage.context.bigFont;
    if (font == UI_FONT_SMALL) return &ui_displayContextStorage.context.smallFont;
    if (font == UI_FONT_CONSOLE) return &ui_displayContextStorage.context.consoleFont;

    scaledSize = ui_displayContextStorage.context.yscale * scale;
    if (font == UI_FONT_BOLD) {
        if (scaledSize <= ui_smallFontThreshold) {
            return &ui_displayContextStorage.context.smallFont;
        }
        if (!(scaledSize >= ui_bigFontThreshold)) {
            return &ui_displayContextStorage.context.textFont;
        }
        return &ui_displayContextStorage.context.boldFont;
    }

    if (scaledSize <= ui_smallFontThreshold) {
        return &ui_displayContextStorage.context.smallFont;
    }
    if (scaledSize >= ui_extraBigFontThreshold) {
        return &ui_displayContextStorage.context.extraBigFont;
    }
    if (scaledSize >= ui_bigFontThreshold) {
        return &ui_displayContextStorage.context.bigFont;
    }
    return &ui_displayContextStorage.context.textFont;
}

// Source: uo_ui_mp_x86.dll 0x400089c0..0x400089ca
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400089c0_400089ca.mcode
// Exact same-module PPC symbol: Text_SetActiveFont.
void Text_SetActiveFont(int32_t font)
{
    ui_activeFont = font;
}
