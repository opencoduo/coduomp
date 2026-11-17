// Source: uo_cgame_mp_x86.dll 0x3003e310..0x3003e31d

#include "client/cgame/client_recovered.h"

/* Return the latest snapshot number and its server time. The original wrapper
 * forwards snapshotNumber (ECX) and serverTime (EAX) with syscall 0x50
 * (CG_GET_CURRENT_SNAPSHOT_NUMBER). */
void trap_GetCurrentSnapshotNumber(int32_t *snapshotNumber, int32_t *serverTime)
{
    cgame_syscall(CG_GET_CURRENT_SNAPSHOT_NUMBER, (intptr_t)snapshotNumber, (intptr_t)serverTime);
}
