// Source: uo_cgame_mp_x86.dll 0x3003d7f0..0x3003d803

#include "client/cgame/client_recovered.h"

/* Return the contents mask at a point inside a positioned and rotated clip
 * model. The original wrapper forwards point (stack), model (EDX), origin
 * (ECX) and angles (EAX) with syscall 0x25 (CG_CM_TRANSFORMED_POINT_CONTENTS). */
int32_t trap_CM_TransformedPointContents(const vec3_t point, int32_t model, const vec3_t origin, const vec3_t angles)
{
    return (int32_t)cgame_syscall(CG_CM_TRANSFORMED_POINT_CONTENTS, (intptr_t)point, model, (intptr_t)origin,
                                  (intptr_t)angles);
}
