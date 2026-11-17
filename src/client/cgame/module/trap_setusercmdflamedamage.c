// Source: uo_cgame_mp_x86.dll 0x3003e380..0x3003e38c

#include "client/cgame/client_recovered.h"

/* Set the user command flame damage. The original wrapper forwards damage
 * (EAX) with syscall 0x58 (CG_SET_USER_CMD_FLAME_DAMAGE). */
void trap_SetUserCmdFlameDamage(int32_t damage)
{
    cgame_syscall(CG_SET_USER_CMD_FLAME_DAMAGE, damage);
}
