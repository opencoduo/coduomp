// Source: uo_cgame_mp_x86.dll 0x3003df20..0x3003df2c

#include "client/cgame/client_recovered.h"

/* Return the printable length of a string. The original wrapper forwards
 * string (EAX) with syscall 0x3a (CG_SE_PRINT_STRLEN). */
int32_t trap_SE_PrintStrlen(const char *string)
{
    return (int32_t)cgame_syscall(CG_SE_PRINT_STRLEN, (intptr_t)string);
}
