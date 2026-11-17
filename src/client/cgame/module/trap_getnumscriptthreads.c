// Source: uo_cgame_mp_x86.dll 0x3003ef90..0x3003ef9e

#include "client/cgame/client_recovered.h"

/* Return the script thread count. The original wrapper pushes only syscall
 * 0xd2 (CG_GET_NUM_SCRIPT_THREADS). */
int32_t trap_GetNumScriptThreads(void)
{
    return (int32_t)cgame_syscall(CG_GET_NUM_SCRIPT_THREADS);
}
