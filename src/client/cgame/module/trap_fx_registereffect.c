// Source: uo_cgame_mp_x86.dll 0x3003f160..0x3003f16f

#include "client/cgame/client_recovered.h"

/* Register an effect file and return its handle. The original wrapper forwards
 * name (EAX) with syscall 0xe2 (CG_FX_REGISTER_EFFECT). */
qhandle_t trap_FX_RegisterEffect(const char *name)
{
    return (qhandle_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)name);
}
