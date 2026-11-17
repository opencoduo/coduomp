// Source: uo_cgame_mp_x86.dll 0x3003ea20..0x3003ea3a

#include "client/cgame/client_recovered.h"

/* Advance a DObj's animation by dtime seconds. The original wrapper forwards
 * obj (EDX) and dtime (stack) with syscall 0x98 (CG_DOBJ_ADVANCE_SERVER_TIME). */
void trap_DObjAdvanceServerTime(struct DObj_s *obj, float dtime)
{
    cgame_syscall(CG_DOBJ_ADVANCE_SERVER_TIME, (intptr_t)obj, CG_FloatBits(dtime));
}
