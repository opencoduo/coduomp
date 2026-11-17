// Source: uo_cgame_mp_x86.dll 0x3003d780..0x3003d78c

#include "client/cgame/client_recovered.h"

/* Pass one integer to the engine's say-line draw service. The original wrapper
 * forwards value (EAX) with syscall 0x1e (CG_NOTIFY_PLAYER_SPAWNED). */
void trap_NotifyPlayerSpawned(int32_t value)
{
    cgame_syscall(CG_NOTIFY_PLAYER_SPAWNED, value);
}
