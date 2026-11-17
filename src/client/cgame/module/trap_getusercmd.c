// Source: uo_cgame_mp_x86.dll 0x3003e350..0x3003e35d

#include "client/cgame/client_recovered.h"

/* Copy a user command; false when it is no longer buffered. The original
 * wrapper forwards cmdNumber (ECX) and ucmd (EAX) with syscall 0x54
 * (CG_GET_USER_CMD). */
qboolean trap_GetUserCmd(int32_t cmdNumber, usercmd_t *ucmd)
{
    return (qboolean)cgame_syscall(CG_GET_USER_CMD, cmdNumber, (intptr_t)ucmd);
}
