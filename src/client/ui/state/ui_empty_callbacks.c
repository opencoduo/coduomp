#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000d570..0x4000d571
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000d570_4000d571.mcode
// Exact same-module PPC symbol: UI_GetTeamColor.
void UI_GetTeamColor(vec4_t color)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)color;
}

// Source: uo_ui_mp_x86.dll 0x4000eb80..0x4000eb81
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000eb80_4000eb81.mcode
// Exact same-module PPC symbol: UI_FeederAddItem.
void UI_FeederAddItem(float feeder, const char *name, int32_t value)
{
    (void)feeder;
    (void)name;
    (void)value;
}

// Source: uo_ui_mp_x86.dll 0x4000ba90..0x4000ba91
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000ba90_4000ba91.mcode
// Exact same-module PPC symbol and loader-cluster position: UI_StartSkirmish.
void UI_StartSkirmish(void)
{
}
