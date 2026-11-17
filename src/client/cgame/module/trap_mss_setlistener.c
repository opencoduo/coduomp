// Source: uo_cgame_mp_x86.dll 0x3003efb0..0x3003efc1

#include "client/cgame/client_recovered.h"

/* Set the sound listener position and orientation. The original wrapper
 * forwards clientNum (EDX), origin (ECX) and axis (EAX) with syscall 0xd5
 * (CG_MSS_SET_LISTENER). */
void trap_MSS_SetListener(int32_t clientNum, const vec3_t origin, vec3_t axis[3])
{
    cgame_syscall(CG_MSS_SET_LISTENER, clientNum, (intptr_t)origin, (intptr_t)axis);
}
