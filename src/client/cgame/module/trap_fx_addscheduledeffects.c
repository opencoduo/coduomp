// Source: uo_cgame_mp_x86.dll 0x3003f260..0x3003f26c

#include "client/cgame/client_recovered.h"

/* Add the scheduled effects to the scene. The original wrapper pushes only
 * syscall 0xea (CG_FX_ADD_SCHEDULED_EFFECTS). */
void trap_FX_AddScheduledEffects(void)
{
    cgame_syscall(CG_FX_ADD_SCHEDULED_EFFECTS);
}
