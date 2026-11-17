// Source: uo_cgame_mp_x86.dll 0x3003e290..0x3003e29e

#include "client/cgame/client_recovered.h"

/* Draw a screen quad from four {x,y} vertices. The original wrapper forwards
 * verts (EDX), shaderParams (ECX) and hShader (EAX) with syscall 0x4c
 * (CG_R_DRAW_ROTATED_QUAD). */
void trap_R_DrawRotatedQuad(const float *verts, const float *shaderParams, qhandle_t hShader)
{
    cgame_syscall(CG_R_DRAW_ROTATED_QUAD, (intptr_t)verts, (intptr_t)shaderParams, hShader);
}
