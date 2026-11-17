// Source: uo_cgame_mp_x86.dll 0x3003f370..0x3003f37c

#include "client/cgame/client_recovered.h"

/* Resynchronize the client clock to the first snapshot. The original wrapper
 * pushes only syscall 0xf8 (CG_SYNC_TIMES). */
void trap_SyncTimes(void)
{
    cgame_syscall(CG_SYNC_TIMES);
}
