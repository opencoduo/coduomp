#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x400078b0..0x400078da
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400078b0_400078da.mcode
// The ui_cache console dispatch, 0x810 menu stride, and same-module PPC call
// graph identify the inlined Display_CacheAll body as UI_Cache_f.
void UI_Cache_f(void)
{
    int32_t index;

    for (index = 0; index < menuCount; ++index) {
        Menu_CacheContents(&Menus[index]);
    }
}
