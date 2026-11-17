// Source: uo_cgame_mp_x86.dll 0x3003f240..0x3003f256

#include "client/cgame/client_recovered.h"

/* Play a registered effect on an entity tag. The original wrapper takes
 * effect on the stack, origin in EDX, axis in ECX and the bolt in EAX, and
 * forwards them in that order with syscall 0xe9 (CG_PLAY_EFFECT_ON_TAG). */
void trap_FX_PlayEntityEffectID(int32_t effect, const vec3_t origin, vec3_t axis[3], const sfx_bolt_info_t *bolt)
{
    cgame_syscall(CG_PLAY_EFFECT_ON_TAG, effect, (intptr_t)origin, (intptr_t)axis, (intptr_t)bolt);
}
