// Source: uo_cgame_mp_x86.dll 0x3003d510..0x3003d51e

#include "client/cgame/client_recovered.h"

/* Print a subtitle. The original wrapper forwards reference (EDX), duration
 * (ECX) and lineWidth (EAX) with syscall 0x5 (CG_SUBTITLE). */
void trap_Subtitle(const char *reference, int32_t duration, int32_t lineWidth)
{
    cgame_syscall(CG_SUBTITLE, (intptr_t)reference, duration, lineWidth);
}
