// Source: uo_cgame_mp_x86.dll 0x3003ee70..0x3003ee81

#include "client/cgame/client_recovered.h"

/* Raise a status-monitor warning. The original wrapper forwards type (EDX),
 * duration (ECX) and material (EAX) with syscall 0xbe (CG_STATMON_WARNING). */
void trap_StatMon_Warning(int32_t type, int32_t duration, const char *material)
{
    cgame_syscall(CG_STATMON_WARNING, type, duration, (intptr_t)material);
}
