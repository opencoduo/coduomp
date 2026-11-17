// Source: uo_cgame_mp_x86.dll 0x3003d8d0..0x3003d90d

#include "client/cgame/client_recovered.h"

/* Clip a projected polygon against the world into mark fragments; returns the
 * fragment count. The original wrapper forwards numPoints (stack), points
 * (stack), projection (stack), axis (stack), radius (stack), maxPoints
 * (stack), pointBuffer (stack), maxFragments (stack), fragmentBuffer (EDX) and
 * markShader (ECX) with syscall 0x2b (CG_CM_MARKFRAGMENTS). */
int32_t trap_CM_MarkFragments(int32_t numPoints, const vec3_t *points, const vec3_t projection,
                              const vec3_t *axis, float radius, int32_t maxPoints, polyVert_t *pointBuffer,
                              int32_t maxFragments, markFragment_t *fragmentBuffer, qhandle_t markShader)
{
    return (int32_t)cgame_syscall(CG_CM_MARKFRAGMENTS, numPoints, (intptr_t)points, (intptr_t)projection,
                                  (intptr_t)axis, CG_FloatBits(radius), maxPoints, (intptr_t)pointBuffer,
                                  maxFragments, (intptr_t)fragmentBuffer, markShader);
}
