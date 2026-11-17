// Source: uo_cgame_mp_x86.dll 0x3003e6b0..0x3003e6c4

#include "client/cgame/client_recovered.h"

/* Make an anim entry a leaf bound to an anim file. The original wrapper
 * forwards anims (EDX), animIndex (low word of ECX) and name (EAX) with
 * syscall 0x85 (CG_XANIM_CREATE). */
void trap_XAnimCreate(XAnim *anims, uint16_t animIndex, const char *name)
{
    cgame_syscall(CG_XANIM_CREATE, (intptr_t)anims, animIndex, (intptr_t)name);
}
