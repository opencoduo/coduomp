// Source: uo_cgame_mp_x86.dll 0x3003d650..0x3003d663

#include "client/cgame/client_recovered.h"

/* Write the named cvars to a text buffer. The original wrapper forwards
 * cvarNames (stack), numCvars (EDX), buffer (ECX) and bufferSize (EAX) with
 * syscall 0x14 (CG_COM_SAVE_CVARS_TO_BUFFER). */
int32_t trap_Com_SaveCvarsToBuffer(const char *const *cvarNames, int32_t numCvars, char *buffer, int32_t bufferSize)
{
    return (int32_t)cgame_syscall(CG_COM_SAVE_CVARS_TO_BUFFER, (intptr_t)cvarNames, numCvars,
                                  (intptr_t)buffer, bufferSize);
}
