// Source: uo_cgame_mp_x86.dll 0x3003efd0..0x3003efdc

#include "client/cgame/client_recovered.h"

/* Commit this frame's looping sounds. The original wrapper pushes only syscall
 * 0xd6 (CG_MSS_UPDATE_LOOPING_SOUNDS). */
void trap_MSS_UpdateLoopingSounds(void)
{
    cgame_syscall(CG_MSS_UPDATE_LOOPING_SOUNDS);
}
