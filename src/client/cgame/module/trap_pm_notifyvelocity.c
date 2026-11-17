// Source: uo_cgame_mp_x86.dll 0x3003e560..0x3003e56c

#include "client/cgame/client_recovered.h"

/* Snap a movement vector to integers in place. The original wrapper forwards
 * velocity (EAX) with syscall 0x66 (CG_PM_NOTIFY_VELOCITY). */
void trap_PM_NotifyVelocity(vec3_t velocity)
{
    cgame_syscall(CG_PM_NOTIFY_VELOCITY, (intptr_t)velocity);
}
