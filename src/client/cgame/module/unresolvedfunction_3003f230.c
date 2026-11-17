// Source: uo_cgame_mp_x86.dll 0x3003f230..0x3003f23f

#include "client/cgame/client_recovered.h"

/* A one-argument wrapper for the oriented effect-play syscall, retained beside
 * trap_FX_PlayEffectID. The original wrapper forwards effect (EAX) with
 * syscall 0xe8 (CG_PLAY_EFFECT_ORIENTED). */
void UnresolvedFunction_3003f230(int32_t effect)
{
    cgame_syscall(CG_PLAY_EFFECT_ORIENTED, effect);
}
