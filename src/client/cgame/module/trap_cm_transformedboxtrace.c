// Source: uo_cgame_mp_x86.dll 0x3003d840..0x3003d86c

#include "client/cgame/client_recovered.h"

/* Trace a box against a positioned and rotated clip model. The original
 * wrapper forwards results (stack), start (stack), end (stack), mins (stack),
 * maxs (stack), model (stack), brushmask (EDX), origin (ECX) and angles (EAX)
 * with syscall 0x27 (CG_CM_TRANSFORMED_BOX_TRACE). */
void trap_CM_TransformedBoxTrace(trace_t *results, const vec3_t start, const vec3_t end, const vec3_t mins,
                                 const vec3_t maxs, int32_t model, int32_t brushmask, const vec3_t origin,
                                 const vec3_t angles)
{
    cgame_syscall(CG_CM_TRANSFORMED_BOX_TRACE, (intptr_t)results, (intptr_t)start, (intptr_t)end,
                  (intptr_t)mins, (intptr_t)maxs, model, brushmask, (intptr_t)origin, (intptr_t)angles);
}
