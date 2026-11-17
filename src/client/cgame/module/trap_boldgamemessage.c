// Source: uo_cgame_mp_x86.dll 0x3003d4c0..0x3003d4cd

#include "client/cgame/client_recovered.h"

/* Print a bold game message; returns the engine result word. The original
 * wrapper forwards message (ECX) and lineWidth (EAX) with syscall 0x3
 * (CG_BOLD_GAME_MESSAGE). */
int32_t trap_BoldGameMessage(const char *message, int32_t lineWidth)
{
    return (int32_t)cgame_syscall(CG_BOLD_GAME_MESSAGE, (intptr_t)message, lineWidth);
}
