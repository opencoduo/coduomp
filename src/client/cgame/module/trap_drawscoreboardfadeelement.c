// Source: uo_cgame_mp_x86.dll 0x3003d700..0x3003d721

#include "client/cgame/client_recovered.h"

/* Draw the bold message lines. The original wrapper forwards x (stack), y
 * (stack), alpha (stack) and mode (ECX) with syscall 0x1b
 * (CG_DRAW_SCOREBOARD_FADE_ELEMENT). */
void trap_DrawScoreboardFadeElement(int32_t x, int32_t y, float alpha, int32_t mode)
{
    cgame_syscall(CG_DRAW_SCOREBOARD_FADE_ELEMENT, x, y, CG_FloatBits(alpha), mode);
}
