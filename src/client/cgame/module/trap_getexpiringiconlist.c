// Source: uo_cgame_mp_x86.dll 0x3003ee90..0x3003eea0

#include "client/cgame/client_recovered.h"

/* Return the status-monitor icon array and its length. The original wrapper
 * forwards list (ECX) and count (EAX) with syscall 0xbf
 * (CG_GET_EXPIRING_ICON_LIST). */
void trap_GetExpiringIconList(const statmon_entry_t **list, int32_t *count)
{
    cgame_syscall(CG_GET_EXPIRING_ICON_LIST, (intptr_t)list, (intptr_t)count);
}
