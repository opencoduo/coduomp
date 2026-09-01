#include "../abi/ui_module_abi.h"
#include "ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000ff30..0x4000ff36
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000ff30_4000ff36.mcode
// Exact same-module PPC symbol: _UI_GetActiveMenu.
int32_t _UI_GetActiveMenu(void)
{
    return ui_activeMenu;
}

// Source: uo_ui_mp_x86.dll 0x40008bf0..0x40008c25
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008bf0_40008c25.mcode
// The Windows optimizer expands Menus_CloseAll into a loop over the 0x810-byte
// menu array; the same-module PPC call graph retains the source-level call.
void UI_Shutdown(void)
{
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): do not leave the engine
     * routing its reserved console key into an unloaded UI capture. */
    trap_Cvar_Set(UI_COMPAT_CONSOLE_BIND_CAPTURE_CVAR, "0");
    Menus_CloseAll();
    trap_LAN_SaveCachedServers();
}

// Source: uo_ui_mp_x86.dll 0x40010410..0x40010444
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40010410_40010444.mcode
// Windows inlines the reverse menu-stack scan and tests window flags 0x4 plus
// the menu +0xbc full-screen field; PPC retains Menus_AnyFullScreenVisible.
qboolean UI_IsFullscreen(void)
{
    return Menus_AnyFullScreenVisible();
}
