// Source: uo_cgame_mp_x86.dll 0x3003a5b0..0x3003a80d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a5b0_3003a80d.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

//
// CG_OpenScriptMenu (0x3003a5b0) — client-side "mr" (menu response) console-command
// handler. Dispatched by CG_ServerCommand (call site 0x3003b0a6) when the server
// forwards a request to open one of the 32 script-popup UI menus. Reads the requested
// menu index from console argv[1], validates it, resolves the menu-name config string,
// and drives the ui_newScriptMenu* / ui_waitingScriptMenu* cvars plus the
// UIMENU_SCRIPT_POPUP[_NO_MOUSE] menu — the open-side sibling of
// CG_CheckOpenWaitingScriptMenu (0x3003a810) and CG_CloseScriptMenu (0x3003a950).
//
// Name adjudication: the .mcode size-match guess "BG_CalculateWeaponPosition_GunRecoil"
// (a broad game_mp_uo corpus name; win size 0x25d ~ matched 0x25e) is REJECTED — it
// comes from the wrong DLL and there is no recoil/weapon-position math here at all.
// The body is pure string/parse/console-command work in the server-command band, next
// to the voice-chat handlers. The symbolized Mac cgame confirms the corresponding
// handler's source name CG_OpenScriptMenu.
//
// Behavior (proven from machine code):
//   - CG_Argv(1): copy console token 1 into g_textScratchBuffer, Q_atoi -> index.
//     (0x3003a5bd PUSH 0xd/1/&g_textScratchBuffer/0x400 CG_ARGV; 0x3003a5df Q_atoi.)
//   - If index < 0 || index >= CS_SCRIPTMENUS_COUNT (32): out-of-range error path
//     (0x3003a7d6): Com_Printf("Server tried to open a bad script menu index: %i\n",
//     index) then trap(CG_SEND_CONSOLE_COMMAND, va("cmd mr %i bad\n", index)); return.
//   - cfgIndex = index + CS_SCRIPTMENUS (LEA EDI,[ESI+0x535]); shared inlined
//     CG_ConfigString bounds check reports Com_ErrorMessage(cg_configStringBadIndexFmt,
//     cfgIndex) when out of [0, MAX_CONFIGSTRINGS) and proceeds regardless.
//   - menuName = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]].
//   - If menuName is empty (first byte 0): not-loaded path (0x3003a62d):
//     Com_Printf("Server tried to open a non-loaded script menu index: %i\n", index)
//     then trap(CG_SEND_CONSOLE_COMMAND, va("cmd mr %i bad\n", index)); return.
//   - noMouse flag: CG_Argc() > 2 && CG_Argv(2)[0] != 0  (0x3003a665..0x3003a69b).
//   - trap(CG_CVAR_SET, "ui_newScriptMenu", menuName)          (0x3003a6a0)
//   - trap(CG_CVAR_SET, "ui_newScriptMenuIndex", va("%i", index))   (0x3003a6ae)
//   - trap(CG_UI_IS_MENU_OPEN, noMouse ? "UIMENU_SCRIPT_POPUP_NO_MOUSE"
//                               : "UIMENU_SCRIPT_POPUP")  -> already open? (0x3003a6ca)
//     If it returns nonzero (menu already open) -> return (JNZ 0x3003a64f).
//   - Otherwise clear the new-menu cvars:
//       trap(CG_CVAR_SET, "ui_newScriptMenu", g_str_empty)        (0x3003a6ed)
//       trap(CG_CVAR_SET, "ui_newScriptMenuIndex", g_str_minus_one "-1") (0x3003a6ff)
//   - Read the currently-pending ui_waitingScriptMenu into a local buffer
//     (trap CG_CVAR_VARIABLE_STRING_BUFFER, 0x3003a711). If it is non-empty AND names a
//     DIFFERENT menu than this one (Q_stricmpn(menuName, waitingMenu, 0x1869f) != 0,
//     0x3003a733), release that pending menu: read ui_waitingScriptMenuIndex into a
//     local buffer (0x3003a74b) and emit
//     trap(CG_SEND_CONSOLE_COMMAND, va("cmd mr %s noop\n", waitingIndex)) (0x3003a762).
//     If the pending menu is the same one (compare == 0) it returns (JZ 0x3003a64f).
//   - Latch this request as the new pending menu (0x3003a77d):
//       trap(CG_CVAR_SET, "ui_waitingScriptMenu", menuName)
//       trap(CG_CVAR_SET, "ui_waitingScriptMenuIndex", va("%i", index))
//       trap(CG_CVAR_SET, "ui_waitingScriptMenuNoMouse", va("%i", noMouse))
//   - return.
//
// /GS-protected frame: 0x404 bytes of locals; __security_cookie is snapshotted into the
// frame at entry (0x3003a5b6/0x3003a5cb) and verified via __security_check_cookie
// (CALL 0x30061639) on every RET. Modeled here as ordinary local buffers.
//
// Trap ids (CG_TRAP enum): 0xd CG_ARGV, 0xc CG_ARGC, 0x9 CG_CVAR_SET,
// 0xb CG_CVAR_VARIABLE_STRING_BUFFER, 0x16 CG_SEND_CONSOLE_COMMAND, 0x7c CG_UI_IS_MENU_OPEN.
//
void CG_OpenScriptMenu(void)
{
    int index;
    int cfgIndex;
    const char *menuName;
    int noMouse;

    // 0x3003a5bd..0x3003a5e6: index = Q_atoi(CG_Argv(1)).
    index = coduo_crt_atoi(CG_Argv(1));

    // 0x3003a5e9..0x3003a5f4 / 0x3003a7d6: range gate [0, 32).
    if (index < 0 || index >= CS_SCRIPTMENUS_COUNT) {
        Com_Printf(g_str_scriptMenuBadIndexFmt, index);
        cgame_syscall(CG_SEND_CONSOLE_COMMAND,
                      (intptr_t)va(g_str_cmd_mr_bad_fmt, index));
        return;
    }

    // 0x3003a5fb..0x3003a628: cfgIndex = index + CS_SCRIPTMENUS, shared inlined
    // CG_ConfigString bounds report, then resolve the menu-name config string.
    cfgIndex = index + CS_SCRIPTMENUS;
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS) {
        Com_ErrorMessage(cg_configStringBadIndexFmt, cfgIndex);
    }
    menuName = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]];

    // 0x3003a628..0x3003a664: empty config string == menu not loaded on this client.
    if (menuName[0] == '\0') {
        Com_Printf(g_str_scriptMenuNotLoadedFmt, index);
        cgame_syscall(CG_SEND_CONSOLE_COMMAND,
                      (intptr_t)va(g_str_cmd_mr_bad_fmt, index));
        return;
    }

    // 0x3003a665..0x3003a69b: noMouse = (CG_Argc() > 2) && CG_Argv(2)[0] != 0.
    noMouse = 0;
    if (cgame_syscall(CG_ARGC) > 2) {
        if (CG_Argv(2)[0] != '\0') {
            noMouse = 1;
        }
    }

    // 0x3003a6a0..0x3003a6c7: publish the requested menu into the "new" cvar pair.
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_newScriptMenu,
                  (intptr_t)menuName);
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_newScriptMenuIndex,
                  (intptr_t)va("%i", index));

    // 0x3003a6ca..0x3003a6e7: if the popup menu is already open, we are done.
    {
        const char *popupName = noMouse ? g_str_UIMENU_SCRIPT_POPUP_NO_MOUSE
                                        : g_str_UIMENU_SCRIPT_POPUP;
        if (cgame_syscall(CG_UI_IS_MENU_OPEN, (intptr_t)popupName) != 0) {
            return;
        }
    }

    // 0x3003a6ed..0x3003a70e: menu not open yet — clear the "new" cvar pair.
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_newScriptMenu,
                  (intptr_t)g_str_empty);
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_newScriptMenuIndex,
                  (intptr_t)g_str_minus_one);

    // 0x3003a711..0x3003a77b: reconcile any already-pending "waiting" menu.
    {
        char waitingMenu[MAX_STRING_CHARS];

        cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER,
                      (intptr_t)g_str_ui_waitingScriptMenu,
                      (intptr_t)waitingMenu, (int32_t)sizeof(waitingMenu));

        if (waitingMenu[0] != '\0') {
            // A different menu is already pending -> release it with a "noop"; the
            // same menu already pending short-circuits (JZ 0x3003a64f) and returns.
            if (Q_stricmpn(menuName, waitingMenu, 0x1869f) == 0) {
                return;
            }

            {
                char waitingIndex[MAX_STRING_CHARS];

                cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER,
                              (intptr_t)g_str_ui_waitingScriptMenuIndex,
                              (intptr_t)waitingIndex,
                              (int32_t)sizeof(waitingIndex));
                cgame_syscall(CG_SEND_CONSOLE_COMMAND,
                              (intptr_t)va(g_str_cmd_mr_noop_fmt, waitingIndex));
            }
        }
    }

    // 0x3003a77d..0x3003a7bd: latch this request as the new pending menu.
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_waitingScriptMenu,
                  (intptr_t)menuName);
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_waitingScriptMenuIndex,
                  (intptr_t)va("%i", index));
    cgame_syscall(CG_CVAR_SET, (intptr_t)g_str_ui_waitingScriptMenuNoMouse,
                  (intptr_t)va("%i", noMouse));
}
