// Source: uo_cgame_mp_x86.dll 0x3003d7e0..0x3003d7ed

#include "client/cgame/client_recovered.h"

/* Return the contents mask at a point. The original wrapper forwards point
 * (ECX) and model (EAX) with syscall 0x24 (CG_CM_POINT_CONTENTS). */
int32_t trap_CM_PointContents(const vec3_t point, int32_t model)
{
    return (int32_t)cgame_syscall(CG_CM_POINT_CONTENTS, (intptr_t)point, model);
}
