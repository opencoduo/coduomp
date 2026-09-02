// Source: uo_cgame_mp_x86.dll 0x3003e4b0..0x3003e4c1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e4b0_3003e4c1.mcode

#include "client/cgame/client_recovered.h"

/* The original i386 wrapper receives name in ECX and origin in EAX.  PUSH EAX,
 * PUSH ECX, PUSH 0xc4 therefore presents (command, name, origin) to the syscall.
 * The named Mac wrapper and engine dispatcher prove that the result is an
 * engine-owned snd_alias_t, not an integer handle. */
snd_alias_t *trap_Com_PickSoundAlias(const char *name, const vec3_t origin)
{
    return (snd_alias_t *)(uintptr_t)cgame_syscall(CG_COM_PICK_SOUND_ALIAS, (intptr_t)name, (intptr_t)origin);
}
