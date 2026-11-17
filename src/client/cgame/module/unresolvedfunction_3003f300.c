// Source: uo_cgame_mp_x86.dll 0x3003f300..0x3003f30f

#include "client/cgame/client_recovered.h"

/* Forward one word to a reserved syscall slot that the engine dispatcher does
 * not service. The original wrapper forwards arg (EAX) with syscall 0xf3
 * (CG_RESERVED_243). */
void UnresolvedFunction_3003f300(int32_t arg)
{
    cgame_syscall(CG_RESERVED_243, arg);
}
