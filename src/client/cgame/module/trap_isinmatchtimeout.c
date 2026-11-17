// Source: uo_cgame_mp_x86.dll 0x3003e670..0x3003e67e

#include "client/cgame/client_recovered.h"

/* Return nonzero while the match is in a timeout. The original wrapper pushes
 * only syscall 0x82 (CG_IS_IN_MATCH_TIMEOUT). */
int32_t trap_IsInMatchTimeout(void)
{
    return (int32_t)cgame_syscall(CG_IS_IN_MATCH_TIMEOUT);
}
