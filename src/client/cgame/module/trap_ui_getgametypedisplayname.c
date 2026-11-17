// Source: uo_cgame_mp_x86.dll 0x3003e650..0x3003e65f

#include "client/cgame/client_recovered.h"

/* Return the UI display name of a game type. The original wrapper forwards
 * gameType (EAX) with syscall 0x80 (CG_UI_GET_GAMETYPE_DISPLAY_NAME). */
const char *trap_UI_GetGameTypeDisplayName(const char *gameType)
{
    return (const char *)(uintptr_t)cgame_syscall(CG_UI_GET_GAMETYPE_DISPLAY_NAME, (intptr_t)gameType);
}
