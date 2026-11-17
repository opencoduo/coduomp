// Source: uo_cgame_mp_x86.dll 0x3003e660..0x3003e66e

#include "client/cgame/client_recovered.h"

/* Return the connected server's address string. The original wrapper pushes
 * only syscall 0x81 (CG_CL_GET_SERVER_IP_ADDRESS). */
const char *trap_CL_GetServerIPAddress(void)
{
    return (const char *)(uintptr_t)cgame_syscall(CG_CL_GET_SERVER_IP_ADDRESS);
}
