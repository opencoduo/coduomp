// Source: uo_cgame_mp_x86.dll 0x3003f2b0..0x3003f2bf

#include "client/cgame/client_recovered.h"

/* Rewind the effect system to time. The original wrapper forwards time (EAX)
 * with syscall 0xf0 (CG_FX_REWIND_TIME). */
void trap_FX_RewindTime(int32_t time)
{
    cgame_syscall(CG_FX_REWIND_TIME, time);
}
