#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x4001cad0..0x4001cad6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001cad0_4001cad6.mcode
// Same-module PPC symbol: Menu_Count.
int32_t Menu_Count(void)
{
    return menuCount;
}
