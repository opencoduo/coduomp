// Source: uo_cgame_mp_x86.dll 0x3003f050..0x3003f060

#include "client/cgame/client_recovered.h"

/* Fade each sound channel toward its listed volume. The original wrapper
 * forwards channelVolumes (ECX) and fadeTime (EAX) with syscall 0xdc
 * (CG_MSS_FADE_SELECT_SOUNDS). */
void trap_MSS_FadeSelectSounds(const float *channelVolumes, int32_t fadeTime)
{
    cgame_syscall(CG_MSS_FADE_SELECT_SOUNDS, (intptr_t)channelVolumes, fadeTime);
}
