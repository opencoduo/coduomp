// Source: uo_cgame_mp_x86.dll 0x3003e710..0x3003e71f

#include "client/cgame/client_recovered.h"

/* Free a DObj. The original wrapper forwards obj (EAX) with syscall 0x88
 * (CG_DOBJ_DESTROY). */
void trap_DObjDestroy(struct DObj_s *obj)
{
    cgame_syscall(CG_DOBJ_DESTROY, (intptr_t)obj);
}
