#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x3003a810..0x3003a946
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a810_3003a946.mcode
//
// CG_CheckOpenWaitingScriptMenu — poll the "ui_waitingScriptMenu" cvar and, if a
// script menu is pending, promote it into the active "ui_newScriptMenu" cvars, then
// ask the UI whether the corresponding popup menu is already open and clear the
// appropriate cvar set.
//
// Naming: the mcode .name guess GScr_LoadGameTypeScript is a server-side, size-only
// match (win size 0x136) and is REJECTED — this function loads no gametype script;
// it drives the client's ui_*ScriptMenu cvars and issues UI menu traps. The
// same-module (cgame_mp) PPC name bank lists CG_CheckOpenWaitingScriptMenu right
// beside the already-reconstructed CG_CloseScriptMenu (0x3003a950) and the sibling
// CG_OpenScriptMenu in the script-menu cluster; the behavior here — reading
// ui_waitingScriptMenu, copying it into ui_newScriptMenu, and gating on a
// popup-menu open query — matches that name, so it is adopted.
//
// All cvar/menu access goes through the cgame VM syscall pointer *0x30085e9c
// (cgame_syscall), via the existing wrappers:
//   trap_Cvar_VariableStringBuffer(name, buf, size)  -> trap id 0xb
//   trap_Cvar_Set(name, value)                       -> trap id 9
//   trap_UIMenuIsOpen(menuName) -> int               -> trap id 124 (0x7c)
//   Q_atoi(string) -> int                            -> call 0x3005b6ce (thunk)
//
// A single 0x400-byte stack scratch buffer at [ESP+0x00] is reused for every
// trap_Cvar_VariableStringBuffer read (each LEA lands back at the frame base after
// the intervening pushes). The MSVC /GS stack cookie ([0x30081650] snapshotted at
// [ESP+0x400], checked via __security_check_cookie at 0x30061639 on both return
// paths) is an ABI detail, not source-level behavior, and is omitted.
//
// The caller at 0x3003b470 does `push ecx; call 0x3003a810`, but this function reads
// no incoming stack argument (it immediately allocates its own 0x404 frame), so it
// takes no parameters.

void CG_CheckOpenWaitingScriptMenu(void)
{
    char value[MAX_STRING_CHARS];

    /* 0x3003a822: read the pending menu name. If ui_waitingScriptMenu is empty
     * (first byte NUL, tested via MOV AL,[buf]/TEST AL,AL at 0x3003a839), nothing is
     * waiting — fall through to the shared cvar-reset/return tail. */
    trap_Cvar_VariableStringBuffer(g_str_ui_waitingScriptMenu, value, sizeof(value));
    if (value[0] != '\0') {
        /* 0x3003a848..0x3003a853: ui_newScriptMenu = ui_waitingScriptMenu. */
        trap_Cvar_Set(g_str_ui_newScriptMenu, value);

        /* 0x3003a859..0x3003a87c: ui_newScriptMenuIndex = ui_waitingScriptMenuIndex.
         * The waiting-index value is read back into the same scratch buffer. */
        trap_Cvar_VariableStringBuffer(g_str_ui_waitingScriptMenuIndex, value, sizeof(value));
        trap_Cvar_Set(g_str_ui_newScriptMenuIndex, value);

        /* 0x3003a882..0x3003a8a3: read ui_waitingScriptMenuNoMouse and Q_atoi it to
         * pick which popup menu the pending script menu maps to. */
        trap_Cvar_VariableStringBuffer(g_str_ui_waitingScriptMenuNoMouse, value, sizeof(value));

        const char *popupMenu;
        if (coduo_crt_atoi(value) != 0) {
            /* 0x3003a8aa: no-mouse variant. */
            popupMenu = g_str_UIMENU_SCRIPT_POPUP_NO_MOUSE;
        } else {
            /* 0x3003a8b1: normal variant. */
            popupMenu = g_str_UIMENU_SCRIPT_POPUP;
        }

        /* 0x3003a8b6..0x3003a8c8: query whether that popup menu is already open. */
        if (trap_UIMenuIsOpen(popupMenu) != 0) {
            /* 0x3003a8ca..0x3003a8f5: the popup is open — the menu has been shown,
             * so clear the "waiting" cvar set. */
            trap_Cvar_Set(g_str_ui_waitingScriptMenu, g_str_empty);
            trap_Cvar_Set(g_str_ui_waitingScriptMenuIndex, g_str_minus_one);
            trap_Cvar_Set(g_str_ui_waitingScriptMenuNoMouse, g_str_zero);
            return;
        }

        /* 0x3003a911..0x3003a92a: the popup is not open yet — clear the "new" cvar
         * set instead (leaving the waiting cvars in place). */
        trap_Cvar_Set(g_str_ui_newScriptMenu, g_str_empty);
        trap_Cvar_Set(g_str_ui_newScriptMenuIndex, g_str_minus_one);
    }

    /* 0x3003a933..0x3003a945: shared tail (empty-waiting early exit and the
     * not-open branch both land here; cookie check + return). */
}
