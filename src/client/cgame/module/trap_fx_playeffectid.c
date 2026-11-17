// Source: uo_cgame_mp_x86.dll 0x3003f210..0x3003f221

#include "client/cgame/client_recovered.h"

/* Play a registered effect at origin along dir. The original wrapper pushes
 * dir (EAX), origin (ECX), effect (EDX) and syscall 0xe8
 * (CG_PLAY_EFFECT_ORIENTED). */
void trap_FX_PlayEffectID(int32_t effect, const vec3_t origin, const vec3_t dir)
{
    cgame_syscall(CG_PLAY_EFFECT_ORIENTED, effect, (intptr_t)origin, (intptr_t)dir);
}
