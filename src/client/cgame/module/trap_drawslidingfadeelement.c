// Source: uo_cgame_mp_x86.dll 0x3003d6d0..0x3003d6f1

#include "client/cgame/client_recovered.h"

/* Draw the notify lines. The original wrapper forwards x (stack), y (stack),
 * alpha (stack) and mode (ECX) with syscall 0x1a
 * (CG_DRAW_SLIDING_FADE_ELEMENT). */
void trap_DrawSlidingFadeElement(int32_t x, int32_t y, float alpha, int32_t mode)
{
    cgame_syscall(CG_DRAW_SLIDING_FADE_ELEMENT, x, y, CG_FloatBits(alpha), mode);
}
