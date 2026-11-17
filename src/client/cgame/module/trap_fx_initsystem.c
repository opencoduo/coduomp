// Source: uo_cgame_mp_x86.dll 0x3003f270..0x3003f27e

#include "client/cgame/client_recovered.h"

/* Initialize the effect system. The original wrapper pushes only syscall 0xeb
 * (CG_FX_INIT_SYSTEM). */
int32_t trap_FX_InitSystem(void)
{
    return (int32_t)cgame_syscall(CG_FX_INIT_SYSTEM);
}
