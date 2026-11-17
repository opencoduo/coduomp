// Source: uo_cgame_mp_x86.dll 0x3003f010..0x3003f020

#include "client/cgame/client_recovered.h"

/* Start an ambient sound alias. The original wrapper forwards alias (ECX) and
 * fadeTime (EAX) with syscall 0xda (CG_MSS_PLAY_AMBIENT_ALIAS). */
void trap_MSS_PlayAmbientAlias(snd_alias_t *alias, int32_t fadeTime)
{
    cgame_syscall(CG_MSS_PLAY_AMBIENT_ALIAS, (intptr_t)alias, fadeTime);
}
