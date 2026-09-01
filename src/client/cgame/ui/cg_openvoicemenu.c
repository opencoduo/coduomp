// Source: uo_cgame_mp_x86.dll 0x30032840..0x30032875
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032840_30032875.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_OpenVoiceMenu(void)
{
    menuDef_t *menu = Menus_FindByName(g_str_voiceMenu);

    if (menu != NULL) {
        Menus_Open(menu);
    }
    trap_Cvar_Set(g_str_cl_conXOffset, "72");
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    s_voiceMenuStartTime = cg_time;
}
