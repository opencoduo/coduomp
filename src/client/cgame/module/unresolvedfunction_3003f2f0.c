// Source: uo_cgame_mp_x86.dll 0x3003f2f0..0x3003f2ff

#include "client/cgame/client_recovered.h"

/* Forward one word to a reserved syscall slot that the engine dispatcher does
 * not service. The original wrapper forwards arg (EAX) with syscall 0xf2
 * (CG_RESERVED_242). */
void UnresolvedFunction_3003f2f0(int32_t arg)
{
    cgame_syscall(CG_RESERVED_242, arg);
}
