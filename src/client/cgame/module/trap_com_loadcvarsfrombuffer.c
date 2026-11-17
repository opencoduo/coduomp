// Source: uo_cgame_mp_x86.dll 0x3003d670..0x3003d683

#include "client/cgame/client_recovered.h"

/* Set the named cvars from a text buffer. The original wrapper forwards
 * cvarNames (stack), numCvars (EDX), buffer (ECX) and sourceName (EAX) with
 * syscall 0x15 (CG_COM_LOAD_CVARS_FROM_BUFFER). */
int32_t trap_Com_LoadCvarsFromBuffer(const char *const *cvarNames, int32_t numCvars, char *buffer,
                                     const char *sourceName)
{
    return (int32_t)cgame_syscall(CG_COM_LOAD_CVARS_FROM_BUFFER, (intptr_t)cvarNames, numCvars,
                                  (intptr_t)buffer, (intptr_t)sourceName);
}
