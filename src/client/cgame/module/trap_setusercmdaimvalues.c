// Source: uo_cgame_mp_x86.dll 0x3003e360..0x3003e36c

#include "client/cgame/client_recovered.h"

/* Set the user command aim offset. The original wrapper forwards angles (EAX)
 * with syscall 0x56 (CG_SET_USER_CMD_AIM_VALUES). */
void trap_SetUserCmdAimValues(const vec3_t angles)
{
    cgame_syscall(CG_SET_USER_CMD_AIM_VALUES, (intptr_t)angles);
}
