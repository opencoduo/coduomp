// Source: uo_cgame_mp_x86.dll 0x3003dff0..0x3003e03e

#include "client/cgame/client_recovered.h"

/* Add a corona to the scene. The original wrapper forwards org (stack), r
 * (stack), g (stack), b (stack), scale (stack), id (stack) and visible (stack)
 * with syscall 0x43 (CG_R_ADD_CORONA_TO_SCENE). */
void trap_R_AddCoronaToScene(const vec3_t org, float r, float g, float b, float scale, int32_t id, qboolean visible)
{
    cgame_syscall(CG_R_ADD_CORONA_TO_SCENE, (intptr_t)org, CG_FloatBits(r), CG_FloatBits(g), CG_FloatBits(b),
                  CG_FloatBits(scale), id, visible);
}
