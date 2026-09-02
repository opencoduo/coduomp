// Source: uo_cgame_mp_x86.dll 0x3003de90..0x3003dee7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003de90_3003dee7.mcode
//
// trap_R_Text_PaintWithCursor — original cgame syscall wrapper installed in
// displayContextDef_t::drawTextWithCursor at +0x78. The i386 routine copies all
// ten caller dwords to syscall 55 in their original order. The cursor character
// is the sole narrowed argument: MOVSX at 0x3003deba sign-extends its low byte.
//
// The intptr_t parameters retain the original VM/syscall word representation.
// ui_shared callers use semantic float arguments, so native register ABIs reach
// this recovered wrapper through OpenCoDUO_UI_DrawTextWithCursorAdapter.

#include "../client_recovered.h"

int32_t trap_R_Text_PaintWithCursor(intptr_t xBits, intptr_t yBits, intptr_t font, intptr_t scaleBits, intptr_t color, intptr_t text,
                                    intptr_t cursorPos, intptr_t cursorChar, intptr_t limit, intptr_t textStyle)
{
    return (int32_t)cgame_syscall(CG_R_TEXT_PAINT_WITH_CURSOR, xBits, yBits, font, scaleBits, color, text, cursorPos,
                                  (int32_t)(int8_t)cursorChar, limit, textStyle);
}
