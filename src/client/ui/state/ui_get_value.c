#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000b6b0..0x4000b6b7
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b6b0_4000b6b7.mcode
// Exact same-module PPC symbol: UI_GetValue.
float UI_GetValue(int32_t ownerDraw, int32_t colorRangeType)
{
    (void)ownerDraw;
    (void)colorRangeType;
    return 0.0f;
}
