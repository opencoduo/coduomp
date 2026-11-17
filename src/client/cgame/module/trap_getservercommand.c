// Source: uo_cgame_mp_x86.dll 0x3003e330..0x3003e33c

#include "client/cgame/client_recovered.h"

/* Make a reliable server command current for argument parsing. The original
 * wrapper forwards serverCommandNumber (EAX) with syscall 0x52
 * (CG_GET_SERVER_COMMAND). */
qboolean trap_GetServerCommand(int32_t serverCommandNumber)
{
    return (qboolean)cgame_syscall(CG_GET_SERVER_COMMAND, serverCommandNumber);
}
