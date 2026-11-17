// Source: uo_cgame_mp_x86.dll 0x3003f1f0..0x3003f200

#include "client/cgame/client_recovered.h"

/* Play a registered effect at origin. The original wrapper forwards effect
 * (ECX) and origin (EAX) with syscall 0xe7 (CG_PLAY_EFFECT_ORIGIN). */
void trap_PlayEffectOrigin(int32_t effect, const vec3_t origin)
{
    cgame_syscall(CG_PLAY_EFFECT_ORIGIN, effect, (intptr_t)origin);
}
