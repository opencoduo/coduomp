// Source: uo_cgame_mp_x86.dll 0x3003e3c0..0x3003e3cb

#include "client/cgame/client_recovered.h"

/* Return the free hunk bytes. The original wrapper pushes only syscall 0x5a
 * (CG_MEMORY_REMAINING). */
int32_t trap_MemoryRemaining(void)
{
    return (int32_t)cgame_syscall(CG_MEMORY_REMAINING);
}
