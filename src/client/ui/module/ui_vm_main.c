#include "../abi/ui_module_abi.h"
#include "ui_functions.h"
#include "ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x400085a0..0x40008698
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400085a0_40008698.mcode
// Dispatch table: uo_ui_mp_x86.dll 0x40008698..0x400086dc.
UI_EXPORT intptr_t UI_ABI_CDECL vmMain(int32_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4,
                                       intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10,
                                       intptr_t arg11)
{
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    (void)arg8;
    (void)arg9;
    (void)arg10;
    (void)arg11;

    if ((uint32_t)command > (uint32_t)UIVM_LAST_COMMAND) {
        return -1;
    }

    switch ((uiVmCommand_t)command) {
    case UIVM_GET_API_VERSION:
        return UIVM_API_VERSION;
    case UIVM_INIT:
        UI_Init();
        return 0;
    case UIVM_SHUTDOWN:
        UI_Shutdown();
        return 0;
    case UIVM_KEY_EVENT:
        UI_KeyEvent((int32_t)arg0, (qboolean)arg1);
        return 0;
    case UIVM_MOUSE_EVENT:
        UI_MouseEvent((int32_t)arg0, (int32_t)arg1);
        return 0;
    case UIVM_REFRESH:
        UI_Refresh((int32_t)arg0);
        return 0;
    case UIVM_IS_FULLSCREEN:
        return UI_IsFullscreen();
    case UIVM_SET_ACTIVE_MENU:
        return UI_SetActiveMenu((int32_t)arg0);
    case UIVM_GET_ACTIVE_MENU:
        return ui_activeMenu;
    case UIVM_GET_MAP_DISPLAY_NAME:
        return (intptr_t)UI_GetMapDisplayName((const char *)arg0);
    case UIVM_GET_GAMETYPE_DISPLAY_NAME:
        return (intptr_t)UI_GetGameTypeDisplayName((const char *)arg0);
    case UIVM_CONSOLE_COMMAND:
        return UI_ConsoleCommand((int32_t)arg0);
    case UIVM_DRAW_CONNECT_SCREEN:
        UI_DrawConnectScreen((qboolean)arg0);
        return 0;
    case UIVM_USES_UNIQUE_CD_KEY:
        return 0;
    case UIVM_CHECK_EXEC_KEY:
        return UI_CheckExecKey((int32_t)arg0);
    case UIVM_LOAD_SCRIPT_MENU:
        return Load_ScriptMenu((const char *)arg0, (int32_t)arg1);
    case UIVM_GET_FONT:
        /* The DLL FILDs arg1 straight into the FMUL by 0.01f (0x40008675);
         * an explicit (float) cast would round the integer first. */
        return (intptr_t)Text_GetFont((int32_t)arg0, (int32_t)arg1 * 0.01f);
    }

    return -1;
}
