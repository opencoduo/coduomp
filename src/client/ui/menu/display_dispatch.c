#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4001ce20..0x4001ce2b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001ce20_4001ce2b.mcode
void Menu_Reset(void)
{
    menuCount = 0;
}

// Source: uo_ui_mp_x86.dll 0x4001ce30..0x4001ce36
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001ce30_4001ce36.mcode
displayContextDef_t *Display_GetContext(void)
{
    return DC;
}
