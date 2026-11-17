// Source: uo_cgame_mp_x86.dll 0x3003f190..0x3003f1a0

#include "client/cgame/client_recovered.h"

/* Play a named effect at origin. The original wrapper forwards name (ECX) and
 * origin (EAX) with syscall 0xe4 (CG_FX_PLAY_SIMPLE_EFFECT). */
void trap_FX_PlaySimpleEffect(const char *name, const vec3_t origin)
{
    cgame_syscall(CG_FX_PLAY_SIMPLE_EFFECT, (intptr_t)name, (intptr_t)origin);
}
