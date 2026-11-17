#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3003f400..0x3003f424
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f400_3003f424.mcode

void CG_FxRestart(void)
{
    Com_Printf("FX Restarting so off-line changes are loaded.\n");
    trap_FX_FreeActive();
    trap_FX_InitSystem();
}
