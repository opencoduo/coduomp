// Source: uo_cgame_mp_x86.dll 0x3003e600..0x3003e60c

#include "client/cgame/client_recovered.h"

/* Load a menu file. The original wrapper forwards menuFile (EAX) with syscall
 * 0x7b (CG_R_REGISTERMENU). */
int32_t trap_R_RegisterMenu(const char *menuFile)
{
    return (int32_t)cgame_syscall(CG_R_REGISTERMENU, (intptr_t)menuFile);
}
