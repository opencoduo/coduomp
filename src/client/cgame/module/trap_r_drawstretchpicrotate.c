// Source: uo_cgame_mp_x86.dll 0x3003e170..0x3003e1f2

#include "client/cgame/client_recovered.h"

/* Draw a stretched picture with two trailing mode words. The original wrapper
 * forwards x (stack), y (stack), w (stack), h (stack), s1 (stack), t1 (stack),
 * s2 (stack), t2 (stack), hShader (stack), rotation (stack) and pivot (stack)
 * with syscall 0x4a (CG_R_DRAW_STRETCH_PIC_ROTATE). */
void trap_R_DrawStretchPicRotate(float x, float y, float w, float h, float s1, float t1, float s2, float t2,
                                 qhandle_t hShader, int32_t rotation, int32_t pivot)
{
    cgame_syscall(CG_R_DRAW_STRETCH_PIC_ROTATE, CG_FloatBits(x), CG_FloatBits(y), CG_FloatBits(w),
                  CG_FloatBits(h), CG_FloatBits(s1), CG_FloatBits(t1), CG_FloatBits(s2), CG_FloatBits(t2),
                  hShader, rotation, pivot);
}
