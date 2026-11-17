// Source: uo_cgame_mp_x86.dll 0x3003e690..0x3003e6a0

#include "client/cgame/client_recovered.h"

/* Allocate an anim set of size entries. The original wrapper forwards
 * debugName (ECX) and size (EAX) with syscall 0x84 (CG_XANIM_CREATE_ANIMS). */
XAnim *trap_XAnimCreateAnims(const char *debugName, int32_t size)
{
    return (XAnim *)(uintptr_t)cgame_syscall(CG_XANIM_CREATE_ANIMS, (intptr_t)debugName, size);
}
