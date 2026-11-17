// Source: uo_cgame_mp_x86.dll 0x3003e400..0x3003e40c

#include "client/cgame/client_recovered.h"

/* Return the key bound to a command. The original wrapper forwards binding
 * (EAX) with syscall 0x5e (CG_KEY_GET_KEY). */
int32_t trap_Key_GetKey(const char *binding)
{
    return (int32_t)cgame_syscall(CG_KEY_GET_KEY, (intptr_t)binding);
}
