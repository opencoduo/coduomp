// Source: uo_cgame_mp_x86.dll 0x3003d930..0x3003d93c

#include "client/cgame/client_recovered.h"

/* Enable or disable precache error reporting. The original wrapper forwards
 * ignore (EAX) with syscall 0x2f (CG_R_SET_IGNORE_PRECACHE_ERRORS). */
void trap_R_SetIgnorePrecacheErrors(int32_t ignore)
{
    cgame_syscall(CG_R_SET_IGNORE_PRECACHE_ERRORS, ignore);
}
