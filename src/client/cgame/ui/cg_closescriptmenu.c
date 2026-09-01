#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3003a950..0x3003a9ef
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a950_3003a9ef.mcode
//
// CG_CloseScriptMenu — dismiss the two script-popup UI menus and reset all of the
// ui_scriptMenu* / ui_newScriptMenu* / ui_waitingScriptMenu* cvars to their
// defaults. Same-module (cgame_mp) PPC name bank lists CG_CloseScriptMenu in the
// script-menu cluster (alongside CG_OpenScriptMenu / CG_CheckOpenWaitingScriptMenu
// / CG_PrecacheScriptMenu); the behavior here — closing "UIMENU_SCRIPT_POPUP"
// and "UIMENU_SCRIPT_POPUP_NO_MOUSE" then clearing the script-menu cvar state —
// matches that name exactly, so it is adopted.
//
// The mcode .name guess BG_FindAnimTrees is a size-only match (win size 0x9f) and
// is rejected: this function registers nothing anim-tree related; it issues UI
// menu-close traps and cvar resets.
//
// All calls go through the cgame VM syscall pointer *0x30085e9c (cgame_syscall):
//   trap(125, menuName)      -> trap_CloseUIMenu  (2 dwords cleaned: id + ptr)
//   trap(9, name, value)     -> trap_Cvar_Set     (3 dwords cleaned: id + 2 ptrs)
// Args are pushed right-to-left, so the trailing PUSH of the id/count is arg0.
// The two ADD ESP balances (0x40 after the first six calls, 0x24 after the last
// three) confirm the call arities: 2+2+3+3+3+3 = 16 dwords = 0x40; 3+3+3 = 9
// dwords = 0x24. This routine takes no arguments and returns nothing (RET).

void CG_CloseScriptMenu(void)
{
    /* 0x3003a950..0x3003a967: close both script-popup menus by name. */
    trap_CloseUIMenu(g_str_UIMENU_SCRIPT_POPUP);
    trap_CloseUIMenu(g_str_UIMENU_SCRIPT_POPUP_NO_MOUSE);

    /* 0x3003a96a..0x3003a9b2: reset the active/new script-menu cvars. Each
     * trap_Cvar_Set(name, value) pushes value, then name, then the id 9.
     * ADD ESP,0x40 at 0x3003a9b2 balances the four Cvar_Set calls plus the two
     * menu-close calls above. */
    trap_Cvar_Set(g_str_ui_scriptMenu, g_str_empty);
    trap_Cvar_Set(g_str_ui_scriptMenuIndex, g_str_minus_one);
    trap_Cvar_Set(g_str_ui_newScriptMenu, g_str_empty);
    trap_Cvar_Set(g_str_ui_newScriptMenuIndex, g_str_minus_one);

    /* 0x3003a9b5..0x3003a9eb: reset the "waiting" script-menu cvars.
     * ADD ESP,0x24 at 0x3003a9eb balances these three Cvar_Set calls. */
    trap_Cvar_Set(g_str_ui_waitingScriptMenu, g_str_empty);
    trap_Cvar_Set(g_str_ui_waitingScriptMenuIndex, g_str_minus_one);
    trap_Cvar_Set(g_str_ui_waitingScriptMenuNoMouse, g_str_zero);
}
