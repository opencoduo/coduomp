#include "../client_recovered.h"

#include <stddef.h>

enum {
    CG_VOICE_MENU_TIMEOUT_MS = 2500
};

// Source: uo_cgame_mp_x86.dll 0x3001ab00..0x3001ab50
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001ab00_3001ab50.mcode
//
// Naming: the .mcode bank's PM_Weapon_FinishWeaponBreakdown is a size-only false
// match and is rejected. The machine code is a timed menu-housekeeping routine:
// once s_voiceMenuStartTime is armed, 2500 ms later it closes the "voiceMenu"
// menu and resets the "cl_conXOffset" cvar to "0". A defensible name is
// CG_VoiceMenuTimeout; the exact original symbol is unproven, so the C symbol is
// kept address-shaped with the working name recorded here.
//
// Menus_FindByName (0x300518e0) and Menus_Close (0x30051970) both take the menu
// argument in EBX; their signatures live in client_recovered.h.

void CG_VoiceMenuTimeout(void)
{
    /*
     * 0x3001ab00..0x3001ab07: load the armed start time; zero means the timer
     * is inactive, so return with no work.
     */
    uint32_t startTime = s_voiceMenuStartTime;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (startTime == 0) {
        return;
    }

    /*
     * 0x3001ab09..0x3001ab17: elapsed = currentTime - startTime, compared
     * signed against 2500 (0x9c4). JLE returns, so work runs only when the
     * signed elapsed value is strictly greater than 2500 ms.
     */
    int32_t elapsed = coduo_int32_from_bits((uint32_t)cg_time - startTime);
    if (elapsed <= CG_VOICE_MENU_TIMEOUT_MS) {
        return;
    }

    /*
     * 0x3001ab1a..0x3001ab2a: find the "voiceMenu" menu and, if it is open,
     * close it. Both callees take the menu argument in EBX.
     */
    menuDef_t *found = Menus_FindByName(g_str_voiceMenu);
    if (found != NULL) {
        Menus_Close(found);
    }

    /*
     * 0x3001ab2f..0x3001ab41: trap_Cvar_Set("cl_conXOffset", "0"). Push order is
     * value then name then id 9; the trap receives (9, name, value) and
     * ADD ESP,0xc balances the three pushed dwords.
     */
    trap_Cvar_Set(g_str_cl_conXOffset, g_str_zero);

    /*
     * 0x3001ab44: clear the stored start time so the timer does not fire again.
     */
    s_voiceMenuStartTime = 0;
}
