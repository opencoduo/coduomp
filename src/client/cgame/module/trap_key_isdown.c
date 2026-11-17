// Source: uo_cgame_mp_x86.dll 0x3003e3d0..0x3003e3dc

#include "client/cgame/client_recovered.h"

/* Return whether a key is held. The original wrapper forwards keynum (EAX)
 * with syscall 0x5b (CG_KEY_IS_DOWN). */
qboolean trap_Key_IsDown(int32_t keynum)
{
    return (qboolean)cgame_syscall(CG_KEY_IS_DOWN, keynum);
}
