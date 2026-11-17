// Source: uo_cgame_mp_x86.dll 0x3003d5e0..0x3003d5ed

#include "client/cgame/client_recovered.h"

/* Copy the current command's argument string into buffer. The original wrapper
 * forwards buffer (ECX) and bufferLength (EAX) with syscall 0xe (CG_ARGS). */
void trap_Args(char *buffer, int32_t bufferLength)
{
    cgame_syscall(CG_ARGS, (intptr_t)buffer, bufferLength);
}
