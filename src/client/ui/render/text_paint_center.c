#include "../module/ui_functions.h"

enum {
    UI_TEXT_UNLIMITED = 0,
    UI_TEXT_NORMAL_STYLE = 0,
    UI_TEXT_PAINT_STYLE = 6
};

// Source: uo_ui_mp_x86.dll 0x40010690..0x400106fe
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40010690_400106fe.mcode
// Exact same-module PPC symbol: Text_PaintCenter.
void Text_PaintCenter(float x, float y, const char *text, float scale, const vec4_t color, int32_t font)
{
    int32_t width = trap_R_Text_Width(text, font, scale, UI_TEXT_UNLIMITED);

    /* width/2 enters the subtract via a bare FILD at 0x400106dc (FILD;
     * FSUBR, no intermediate float store); an explicit (float) cast would
     * round the operand the DLL keeps exact in 80-bit (Class 4). */
    trap_R_Text_Paint(x - width / 2, y, font, scale, color, text, UI_TEXT_NORMAL_STYLE, UI_TEXT_UNLIMITED, UI_TEXT_PAINT_STYLE);
}
