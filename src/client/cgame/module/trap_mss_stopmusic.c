// Source: uo_cgame_mp_x86.dll 0x3003f000..0x3003f00f

#include "client/cgame/client_recovered.h"

/* Stop the music. The original wrapper forwards fadeTime (EAX) with syscall
 * 0xd9 (CG_MSS_STOP_MUSIC). */
void trap_MSS_StopMusic(int32_t fadeTime)
{
    cgame_syscall(CG_MSS_STOP_MUSIC, fadeTime);
}
