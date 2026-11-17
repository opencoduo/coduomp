// Source: uo_cgame_mp_x86.dll 0x3003e680..0x3003e68f

#include "client/cgame/client_recovered.h"

/* Load an anim file. The original wrapper forwards name (EAX) with syscall
 * 0x83 (CG_XANIM_PRECACHE). */
void trap_XAnimPrecache(const char *name)
{
    cgame_syscall(CG_XANIM_PRECACHE, (intptr_t)name);
}
