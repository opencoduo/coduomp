// Source: uo_cgame_mp_x86.dll 0x3003e480..0x3003e48f

#include "client/cgame/client_recovered.h"

/* Load the cgame sound aliases for a load spec. The original wrapper forwards
 * loadspec (EAX) with syscall 0xc2 (CG_COM_LOAD_SOUND_ALIASES). */
void trap_Com_LoadSoundAliases(const char *loadspec)
{
    cgame_syscall(CG_COM_LOAD_SOUND_ALIASES, (intptr_t)loadspec);
}
