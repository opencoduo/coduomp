// Source: uo_cgame_mp_x86.dll 0x3003ed50..0x3003ed60

#include "client/cgame/client_recovered.h"

/* Fill a per-bone collision table for a DObj. The original wrapper forwards
 * obj (ECX) and table (EAX) with syscall 0xb4
 * (CG_DOBJ_BUILD_PART_COLLISION_TABLE). */
void trap_DObjBuildPartCollisionTable(struct DObj_s *obj, XModelPartColl **table)
{
    cgame_syscall(CG_DOBJ_BUILD_PART_COLLISION_TABLE, (intptr_t)obj, (intptr_t)table);
}
