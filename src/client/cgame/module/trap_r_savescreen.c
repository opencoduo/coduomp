// Source: uo_cgame_mp_x86.dll 0x3003e0b0..0x3003e0b9

#include "client/cgame/client_recovered.h"

/* Save a copy of the current frame. The original wrapper pushes only syscall
 * 0x46 (CG_R_SAVE_SCREEN). */
void trap_R_SaveScreen(void)
{
    cgame_syscall(CG_R_SAVE_SCREEN);
}
