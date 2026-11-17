// Source: uo_cgame_mp_x86.dll 0x3003e340..0x3003e34b

#include "client/cgame/client_recovered.h"

/* Return the number of the most recent user command. The original wrapper
 * pushes only syscall 0x53 (CG_GET_CURRENT_CMD_NUMBER). */
int32_t trap_GetCurrentCmdNumber(void)
{
    return (int32_t)cgame_syscall(CG_GET_CURRENT_CMD_NUMBER);
}
