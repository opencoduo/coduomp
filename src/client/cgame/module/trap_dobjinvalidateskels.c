// Source: uo_cgame_mp_x86.dll 0x3003ea10..0x3003ea1c

#include "client/cgame/client_recovered.h"

/* Invalidate every cached client DObj skeleton. The original wrapper pushes
 * only syscall 0x97 (CG_DOBJ_INVALIDATE_SKELS). */
void trap_DObjInvalidateSkels(void)
{
    cgame_syscall(CG_DOBJ_INVALIDATE_SKELS);
}
