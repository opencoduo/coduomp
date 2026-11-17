// Source: uo_cgame_mp_x86.dll 0x3003f2a0..0x3003f2af

#include "client/cgame/client_recovered.h"

/* Set the effect system's time. The original wrapper forwards time (EAX) with
 * syscall 0xee (CG_FX_ADJUST_TIME). */
void trap_FX_AdjustTime(int32_t time)
{
    cgame_syscall(CG_FX_ADJUST_TIME, time);
}
