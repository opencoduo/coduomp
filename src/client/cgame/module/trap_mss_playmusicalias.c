// Source: uo_cgame_mp_x86.dll 0x3003eff0..0x3003efff

#include "client/cgame/client_recovered.h"

/* Start a music sound alias. The original wrapper forwards alias (EAX) with
 * syscall 0xd8 (CG_MSS_PLAY_MUSIC_ALIAS). */
void trap_MSS_PlayMusicAlias(snd_alias_t *alias)
{
    cgame_syscall(CG_MSS_PLAY_MUSIC_ALIAS, (intptr_t)alias);
}
