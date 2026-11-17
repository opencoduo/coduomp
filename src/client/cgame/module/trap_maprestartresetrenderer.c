// Source: uo_cgame_mp_x86.dll 0x3003e630..0x3003e639

#include "client/cgame/client_recovered.h"

/* Reset the UI for a map restart. The original wrapper pushes only syscall
 * 0x7e (CG_MAP_RESTART_RESET_RENDERER). */
void trap_MapRestartResetRenderer(void)
{
    cgame_syscall(CG_MAP_RESTART_RESET_RENDERER);
}
