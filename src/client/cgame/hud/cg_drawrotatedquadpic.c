// Source: uo_cgame_mp_x86.dll 0x3001c550..0x3001c5c1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c550_3001c5c1.mcode

#include "../client_recovered.h"

/* The Mac CG_FillRotatedRect performs the corresponding physical-screen
 * scaling, color setup, rotated-quad draw, and color reset. */

void CG_FillRotatedRect(const vec4_t color, float x, float y,
                           float width, float height,
                           int32_t rotation, int32_t pivot)
{
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
    trap_R_DrawStretchPicRotate(x * cgs.screenXScale, y * cgs.screenYScale,
                                width * cgs.screenXScale, height * cgs.screenYScale,
                                0.0f, 0.0f, 0.0f, 0.0f, cgs.media.whiteShader, rotation, pivot);
    cgame_syscall(CG_R_SETCOLOR, 0);
}
