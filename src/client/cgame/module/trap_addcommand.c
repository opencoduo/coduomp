// Source: uo_cgame_mp_x86.dll 0x3003d6a0..0x3003d6ac

#include "client/cgame/client_recovered.h"

/* Register a console command handled by the cgame. The original wrapper
 * forwards cmdName (EAX) with syscall 0x17 (CG_ADD_COMMAND). */
void trap_AddCommand(const char *cmdName)
{
    cgame_syscall(CG_ADD_COMMAND, (intptr_t)cmdName);
}
