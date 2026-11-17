// Source: uo_cgame_mp_x86.dll 0x3003e300..0x3003e30c

#include "client/cgame/client_recovered.h"

/* Copy the current game state. The original wrapper forwards gameState (EAX)
 * with syscall 0x4f (CG_GET_GAME_STATE). */
void trap_GetGameState(gameState_t *gameState)
{
    cgame_syscall(CG_GET_GAME_STATE, (intptr_t)gameState);
}
