// Source: uo_cgame_mp_x86.dll 0x3003e320..0x3003e32d

#include "client/cgame/client_recovered.h"

/* Copy a snapshot; false when it is no longer available. The original wrapper
 * forwards snapshotNumber (ECX) and snapshot (EAX) with syscall 0x51
 * (CG_GET_SNAPSHOT). */
qboolean trap_GetSnapshot(int32_t snapshotNumber, snapshot_t *snapshot)
{
    return (qboolean)cgame_syscall(CG_GET_SNAPSHOT, snapshotNumber, (intptr_t)snapshot);
}
