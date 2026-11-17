// Source: uo_cgame_mp_x86.dll 0x3003ec40..0x3003ec4f

#include "client/cgame/client_recovered.h"

/* Return the skeleton allocation size of a DObj. The original wrapper forwards
 * obj (EAX) with syscall 0xaa (CG_DOBJ_GET_ALLOC_SKEL_SIZE). */
int32_t trap_DObjGetAllocSkelSize(struct DObj_s *obj)
{
    return (int32_t)cgame_syscall(CG_DOBJ_GET_ALLOC_SKEL_SIZE, (intptr_t)obj);
}
