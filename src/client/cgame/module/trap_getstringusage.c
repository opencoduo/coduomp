// Source: uo_cgame_mp_x86.dll 0x3003efa0..0x3003efae

#include "client/cgame/client_recovered.h"

/* Return the script string usage. The original wrapper pushes only syscall
 * 0xd3 (CG_GET_STRING_USAGE). */
int32_t trap_GetStringUsage(void)
{
    return (int32_t)cgame_syscall(CG_GET_STRING_USAGE);
}
