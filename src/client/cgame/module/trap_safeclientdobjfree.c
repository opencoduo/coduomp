// Source: uo_cgame_mp_x86.dll 0x3003ec10..0x3003ec20

#include "client/cgame/client_recovered.h"

/* Free a client DObj handle. The original wrapper forwards handle (ECX) and
 * releaseRuntimeTree (EAX) with syscall 0xa8 (CG_SAFE_CLIENT_DOBJ_FREE). */
void trap_SafeClientDObjFree(int32_t handle, qboolean releaseRuntimeTree)
{
    cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, handle, releaseRuntimeTree);
}
