// Source: uo_cgame_mp_x86.dll 0x3003ef80..0x3003ef8e

#include "client/cgame/client_recovered.h"

/* Return the script variable count. The original wrapper pushes only syscall
 * 0xd1 (CG_GET_NUM_SCRIPT_VARS). */
int32_t trap_GetNumScriptVars(void)
{
    return (int32_t)cgame_syscall(CG_GET_NUM_SCRIPT_VARS);
}
