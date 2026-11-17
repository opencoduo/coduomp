// Source: uo_cgame_mp_x86.dll 0x3003e6e0..0x3003e70a

#include "client/cgame/client_recovered.h"

/* Make an anim entry a blend node over its children. The original wrapper
 * forwards anims (stack), animIndex (low word of stack), name (stack),
 * children (low word of EDX), numChildren (low word of ECX) and flags (low
 * word of EAX) with syscall 0x87 (CG_XANIM_BLEND). */
void trap_XAnimBlend(XAnim *anims, uint16_t animIndex, const char *name, uint16_t children,
                     uint16_t numChildren, uint16_t flags)
{
    cgame_syscall(CG_XANIM_BLEND, (intptr_t)anims, animIndex, (intptr_t)name, children, numChildren, flags);
}
