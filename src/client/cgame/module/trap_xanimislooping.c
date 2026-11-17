// Source: uo_cgame_mp_x86.dll 0x3003e980..0x3003e995

#include "client/cgame/client_recovered.h"

/* Return whether an anim entry loops. The original wrapper forwards anims
 * (ECX) and animIndex (low word of stack) with syscall 0x93
 * (CG_XANIM_IS_LOOPING). */
qboolean trap_XAnimIsLooping(XAnim *anims, uint16_t animIndex)
{
    return (qboolean)cgame_syscall(CG_XANIM_IS_LOOPING, (intptr_t)anims, animIndex);
}
