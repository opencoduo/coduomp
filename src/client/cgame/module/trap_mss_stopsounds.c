// Source: uo_cgame_mp_x86.dll 0x3003efe0..0x3003efef

#include "client/cgame/client_recovered.h"

/* Stop every sound. The original wrapper forwards fadeTime (EAX) with syscall
 * 0xd7 (CG_MSS_STOP_SOUNDS). */
void trap_MSS_StopSounds(int32_t fadeTime)
{
    cgame_syscall(CG_MSS_STOP_SOUNDS, fadeTime);
}
