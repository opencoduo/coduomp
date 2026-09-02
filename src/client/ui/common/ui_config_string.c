#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40060000..0x400603ff.
static char ui_configStringBuffer[MAX_STRING_CHARS];

// Source: uo_ui_mp_x86.dll 0x40011720..0x40011740
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40011720_40011740.mcode
// Exact same-module PPC symbol: UI_ConfigString.
const char *UI_ConfigString(int32_t index)
{
    trap_GetConfigString(index, ui_configStringBuffer, (int32_t)sizeof(ui_configStringBuffer));
    return ui_configStringBuffer;
}
