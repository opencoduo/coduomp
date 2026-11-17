// Source: uo_cgame_mp_x86.dll 0x3003f380..0x3003f38f

#include "client/cgame/client_recovered.h"

/* Execute a console command string immediately. The original wrapper forwards
 * command (EAX) with syscall 0xfa (CG_EXECUTE_COMMAND). */
void trap_ExecuteCommand(const char *command)
{
    cgame_syscall(CG_EXECUTE_COMMAND, (intptr_t)command);
}
