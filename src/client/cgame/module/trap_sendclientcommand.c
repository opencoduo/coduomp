// Source: uo_cgame_mp_x86.dll 0x3003d6b0..0x3003d6bc

#include "client/cgame/client_recovered.h"

/* Queue a reliable client command. The original wrapper forwards command (EAX)
 * with syscall 0x18 (CG_SEND_CLIENT_COMMAND). */
void trap_SendClientCommand(const char *command)
{
    cgame_syscall(CG_SEND_CLIENT_COMMAND, (intptr_t)command);
}
