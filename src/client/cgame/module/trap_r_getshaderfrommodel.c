// Source: uo_cgame_mp_x86.dll 0x3003db60..0x3003db6d

#include "client/cgame/client_recovered.h"

/* Return a model surface's shader. The original wrapper forwards model (ECX)
 * and surfaceIndex (EAX) with syscall 0x31 (CG_R_GET_SHADER_FROM_MODEL). */
qhandle_t trap_R_GetShaderFromModel(qhandle_t model, int32_t surfaceIndex)
{
    return (qhandle_t)cgame_syscall(CG_R_GET_SHADER_FROM_MODEL, model, surfaceIndex);
}
