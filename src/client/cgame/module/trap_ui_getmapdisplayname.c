// Source: uo_cgame_mp_x86.dll 0x3003e640..0x3003e64c

#include "client/cgame/client_recovered.h"

/* Return the UI display name of a map. The original wrapper forwards mapName
 * (EAX) with syscall 0x7f (CG_UI_GET_MAP_DISPLAY_NAME). */
const char *trap_UI_GetMapDisplayName(const char *mapName)
{
    return (const char *)(uintptr_t)cgame_syscall(CG_UI_GET_MAP_DISPLAY_NAME, (intptr_t)mapName);
}
