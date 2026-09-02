#include "../module/ui_functions.h"

enum {
    UI_FONT_INFO_FIRST_GLYPH = 32,
    UI_FONT_INFO_GLYPH_COUNT = 64
};

// Source: uo_ui_mp_x86.dll 0x400092a0..0x400092e3
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400092a0_400092e3.mcode
// Exact same-module PPC symbol: UI_GetFontInfo.
void UI_GetFontInfo(void)
{
    int32_t character;

    Com_Printf("Font Info\n");
    Com_Printf("=========\n");
    for (character = UI_FONT_INFO_FIRST_GLYPH;
         character < UI_FONT_INFO_FIRST_GLYPH + UI_FONT_INFO_GLYPH_COUNT;
         ++character) {
        Com_Printf("Glyph handle %i: %i\n", character,
                   ui_displayContextStorage.context.textFont.glyphs[character].glyph);
    }
}
