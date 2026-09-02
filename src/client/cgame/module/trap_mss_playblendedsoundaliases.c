// Source: uo_cgame_mp_x86.dll 0x3003e500..0x3003e52d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e500_3003e52d.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_MSS_PlayBlendedSoundAliases — wrapper for syscall 0xc7 (199).
 *
 * The i386 entry receives alias0, alias1, blend, and entityNum on the stack,
 * with origin in EDX and timeShift in ECX. Its initial PUSH ECX / PUSH EDX
 * preserve those last two arguments below the four stack arguments before the
 * syscall, producing six user arguments in total. The prior reconstruction
 * incorrectly classified the register values as scratch saves. The named Mac
 * wrapper and CL_CgameSystemCalls independently prove the same six-slot contract.
 */
void trap_MSS_PlayBlendedSoundAliases(snd_alias_t *alias0, snd_alias_t *alias1, float blend, int32_t entityNum, const vec3_t origin,
                                      int32_t timeShift)
{
    cgame_syscall(CG_MSS_PLAY_BLENDED_SOUND_ALIASES, (intptr_t)alias0, (intptr_t)alias1, CG_FloatBits(blend), entityNum, (intptr_t)origin,
                  timeShift);
}
