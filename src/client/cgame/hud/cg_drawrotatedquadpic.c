// Source: uo_cgame_mp_x86.dll 0x3001c550..0x3001c5c1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c550_3001c5c1.mcode

#include "../client_recovered.h"

/* The Mac CG_DrawRotatedQuadPic performs the corresponding physical-screen
 * scaling, color setup, rotated-quad draw, and color reset. */

void CG_DrawRotatedQuadPic(const vec4_t color, float x, float y, float width, float height, int32_t rotation, int32_t pivot)
{
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
    cgame_syscall(CG_R_DRAW_STRETCH_PIC_ROTATE, CG_FloatBits(x * cgs_screenXScale), CG_FloatBits(y * cgs_screenYScale),
                  CG_FloatBits(width * cgs_screenXScale), CG_FloatBits(height * cgs_screenYScale), 0, 0, 0, 0, cgs_media_whiteShader,
                  rotation, pivot);
    cgame_syscall(CG_R_SETCOLOR, 0);
}
