// Source: uo_cgame_mp_x86.dll 0x3003d4b0..0x3003d4bd

#include "client/cgame/client_recovered.h"

/* Print a game message. The original wrapper forwards message (ECX) and
 * lineWidth (EAX) with syscall 0x2 (CG_GAME_MESSAGE). */
void trap_GameMessage(const char *message, int32_t lineWidth)
{
    cgame_syscall(CG_GAME_MESSAGE, (intptr_t)message, lineWidth);
}
