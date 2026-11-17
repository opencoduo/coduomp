// Source: uo_cgame_mp_x86.dll 0x3003e2d0..0x3003e2ed

#include "client/cgame/client_recovered.h"

/* Describe the surface along a view ray; nonzero on a hit. The original
 * wrapper forwards origin (stack), direction (stack), materialName (stack),
 * surfaceFlags (EDX), contents (ECX) and size (EAX) with syscall 0x6e
 * (CG_GET_VIEW_INFO). */
int32_t trap_GetViewInfo(const vec3_t origin, const vec3_t direction, char *materialName, char *surfaceFlags,
                         char *contents, int32_t size)
{
    return (int32_t)cgame_syscall(CG_GET_VIEW_INFO, (intptr_t)origin, (intptr_t)direction,
                                  (intptr_t)materialName, (intptr_t)surfaceFlags, (intptr_t)contents, size);
}
