// Source: uo_cgame_mp_x86.dll 0x3003d750..0x3003d771

#include "client/cgame/client_recovered.h"

/* Draw the subtitle lines. The original wrapper forwards x (stack), y (stack),
 * alpha (stack) and mode (ECX) with syscall 0x1d (CG_DRAW_FIXED_FADE_ELEMENT). */
void trap_DrawFixedFadeElement(int32_t x, int32_t y, float alpha, int32_t mode)
{
    cgame_syscall(CG_DRAW_FIXED_FADE_ELEMENT, x, y, CG_FloatBits(alpha), mode);
}
