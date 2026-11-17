// Source: uo_cgame_mp_x86.dll 0x3003e5f0..0x3003e5fb

#include "client/cgame/client_recovered.h"

/* Return the hunk bytes in use. The original wrapper pushes only syscall 0x3f
 * (CG_HUNK_USED). */
int32_t trap_Hunk_Used(void)
{
    return (int32_t)cgame_syscall(CG_HUNK_USED);
}
