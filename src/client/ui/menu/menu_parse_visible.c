#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4001c220..0x4001c23a
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4001c220_4001c23a.mcode
// Exact same-module PPC symbol and keyword-table entry: MenuParse_visible.
qboolean MenuParse_visible(menuDef_t *menu, int32_t sourceHandle)
{
    int32_t visible;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)menu;
    return PC_Int_Parse(sourceHandle, &visible) != 0;
}
