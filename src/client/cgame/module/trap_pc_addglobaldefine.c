// Source: uo_cgame_mp_x86.dll 0x3003e430..0x3003e43c

#include "client/cgame/client_recovered.h"

/* Add a global script-parser define. The original wrapper forwards define
 * (EAX) with syscall 0x60 (CG_PC_ADD_GLOBAL_DEFINE). */
int32_t trap_PC_AddGlobalDefine(const char *define)
{
    return (int32_t)cgame_syscall(CG_PC_ADD_GLOBAL_DEFINE, (intptr_t)define);
}
