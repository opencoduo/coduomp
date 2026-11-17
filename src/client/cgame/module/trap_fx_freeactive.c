// Source: uo_cgame_mp_x86.dll 0x3003f280..0x3003f28e

#include "client/cgame/client_recovered.h"

/* Free every active effect. The original wrapper pushes only syscall 0xec
 * (CG_FX_FREE_ACTIVE). */
int32_t trap_FX_FreeActive(void)
{
    return (int32_t)cgame_syscall(CG_FX_FREE_ACTIVE);
}
