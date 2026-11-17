// Source: uo_cgame_mp_x86.dll 0x3003f1d0..0x3003f1e6

#include "client/cgame/client_recovered.h"

/* Play a named effect bolted to an entity tag. The original wrapper forwards
 * name (stack), origin (EDX), axis (ECX) and bolt (EAX) with syscall 0xe6
 * (CG_FX_PLAY_ENTITY_EFFECT). */
void trap_FX_PlayEntityEffect(const char *name, const vec3_t origin, vec3_t axis[3], const sfx_bolt_info_t *bolt)
{
    cgame_syscall(CG_FX_PLAY_ENTITY_EFFECT, (intptr_t)name, (intptr_t)origin, (intptr_t)axis, (intptr_t)bolt);
}
