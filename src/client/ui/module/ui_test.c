#include "../abi/ui_module_abi.h"
#include "ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40007850..0x40007863
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007850_40007863.mcode
// Role name: UI_Test_f; exact command text and execute mode identify the dormant
// UI test command leaf.
void UI_Test_f(void)
{
    trap_Cmd_ExecuteText(EXEC_APPEND, "d1\n");
}
