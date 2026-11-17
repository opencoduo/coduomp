// Source: uo_cgame_mp_x86.dll 0x3003d7d0..0x3003d7de

#include "client/cgame/client_recovered.h"

/* Return a temporary capsule clip model. The original wrapper forwards mins
 * (EDX), maxs (ECX) and contents (EAX) with syscall 0x2a
 * (CG_CM_TEMP_CAPSULE_MODEL). */
int32_t trap_CM_TempCapsuleModel(const vec3_t mins, const vec3_t maxs, int32_t contents)
{
    return (int32_t)cgame_syscall(CG_CM_TEMP_CAPSULE_MODEL, (intptr_t)mins, (intptr_t)maxs, contents);
}
