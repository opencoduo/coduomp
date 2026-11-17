// Source: uo_cgame_mp_x86.dll 0x3003f1b0..0x3003f1c1

#include "client/cgame/client_recovered.h"

/* Play a named effect at origin with an orientation. The original wrapper
 * forwards name (EDX), origin (ECX) and axis (EAX) with syscall 0xe5
 * (CG_FX_PLAY_EFFECT). */
void trap_FX_PlayEffect(const char *name, const vec3_t origin, vec3_t axis[3])
{
    cgame_syscall(CG_FX_PLAY_EFFECT, (intptr_t)name, (intptr_t)origin, (intptr_t)axis);
}
