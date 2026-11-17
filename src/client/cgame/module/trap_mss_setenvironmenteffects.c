// Source: uo_cgame_mp_x86.dll 0x3003f070..0x3003f08f

#include "client/cgame/client_recovered.h"

/* Set the sound environment room type and reverb level. The original wrapper
 * forwards roomType (stack), reverbLevel (stack) and fadeTime (ECX) with
 * syscall 0xdd (CG_MSS_SET_ENVIRONMENT_EFFECTS). */
void trap_MSS_SetEnvironmentEffects(const char *roomType, float reverbLevel, int32_t fadeTime)
{
    cgame_syscall(CG_MSS_SET_ENVIRONMENT_EFFECTS, (intptr_t)roomType, CG_FloatBits(reverbLevel), fadeTime);
}
