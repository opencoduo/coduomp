// Source: uo_cgame_mp_x86.dll 0x3003f0b0..0x3003f0c7

#include "client/cgame/client_recovered.h"

/* Set the user command weapon value and sensitivity scale. The original
 * wrapper forwards stateValue (EDX) and sensitivityScale (stack) with syscall
 * 0x55 (CG_SET_USER_CMD_VALUE). */
void trap_SetUserCmdValue(int32_t stateValue, float sensitivityScale)
{
    cgame_syscall(CG_SET_USER_CMD_VALUE, stateValue, CG_FloatBits(sensitivityScale));
}
