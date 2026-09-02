// Source: uo_cgame_mp_x86.dll 0x3003e4e0..0x3003e4f7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e4e0_3003e4f7.mcode

#include "client/cgame/client_recovered.h"

/* The original i386 wrapper receives alias on the stack, entityNum in EDX,
 * origin in ECX, and timeShift in EAX. Mac MSS_PlaySoundAlias and its sample and
 * stream callees prove that the result is playback duration in milliseconds. */
int32_t trap_MSS_PlaySoundAlias(snd_alias_t *alias, int32_t entityNum, const void *soundPosition, int32_t timeShift)
{
    return (int32_t)cgame_syscall(CG_MSS_PLAY_SOUND_ALIAS, (intptr_t)alias, entityNum, (intptr_t)soundPosition, timeShift);
}
