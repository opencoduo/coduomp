// Source: uo_cgame_mp_x86.dll 0x3003d910..0x3003d91c

#include "client/cgame/client_recovered.h"

/* Load the render world. The original wrapper forwards mapname (EAX) with
 * syscall 0x2c (CG_R_LOAD_WORLD_MAP). */
void trap_R_LoadWorldMap(const char *mapname)
{
    cgame_syscall(CG_R_LOAD_WORLD_MAP, (intptr_t)mapname);
}
