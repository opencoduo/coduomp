// Source: uo_cgame_mp_x86.dll 0x3003e4d0..0x3003e4df

#include "client/cgame/client_recovered.h"

/* Return the cgame-bank sound alias for an index. The original wrapper
 * forwards aliasIndex (EAX) with syscall 0xc5 (CG_COM_GET_SOUND_ALIAS). */
snd_alias_t *trap_Com_GetSoundAlias(int32_t aliasIndex)
{
    return (snd_alias_t *)(uintptr_t)cgame_syscall(CG_COM_GET_SOUND_ALIAS, aliasIndex);
}
