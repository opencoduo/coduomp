// Source: uo_cgame_mp_x86.dll 0x3003d790..0x3003d79c

#include "client/cgame/client_recovered.h"

/* Load the collision map. The original wrapper forwards mapname (EAX) with
 * syscall 0x1f (CG_CM_LOAD_MAP). */
void trap_CM_LoadMap(const char *mapname)
{
    cgame_syscall(CG_CM_LOAD_MAP, (intptr_t)mapname);
}
