// Source: uo_cgame_mp_x86.dll 0x3003f290..0x3003f29e

#include "client/cgame/client_recovered.h"

/* Shut the effect system down. The original wrapper pushes only syscall 0xed
 * (CG_FX_FREE_SYSTEM). */
int32_t trap_FX_FreeSystem(void)
{
    return (int32_t)cgame_syscall(CG_FX_FREE_SYSTEM);
}
