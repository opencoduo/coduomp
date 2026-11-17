// Source: uo_cgame_mp_x86.dll 0x3003d730..0x3003d74c

#include "client/cgame/client_recovered.h"

/* Draw the debug console overlay. The original wrapper forwards x (stack), y
 * (EDX) and alpha (stack) with syscall 0x1c (CG_DRAW_DEBUG_FADE_ELEMENT). */
void trap_DrawDebugFadeElement(int32_t x, int32_t y, float alpha)
{
    cgame_syscall(CG_DRAW_DEBUG_FADE_ELEMENT, x, y, CG_FloatBits(alpha));
}
