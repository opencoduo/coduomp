#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

// Source: uo_ui_mp_x86.dll 0x400113b0..0x400113ec
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400113b0_400113ec.mcode
// Same-module PPC symbol: UI_RegisterCvars.
void UI_RegisterCvars(void)
{
    for (int32_t index = 0; index < ui_cvarCount; ++index) {
        cvarTable_t *entry = &ui_cvarTable[index];

        trap_Cvar_Register(entry->vmCvar, entry->cvarName, entry->defaultString, entry->cvarFlags);
    }
}

// Source: uo_ui_mp_x86.dll 0x400113f0..0x40011420
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400113f0_40011420.mcode
// Same-module PPC symbol: UI_UpdateCvars.
void UI_UpdateCvars(void)
{
    for (int32_t index = 0; index < ui_cvarCount; ++index) {
        trap_Cvar_Update(ui_cvarTable[index].vmCvar);
    }
}
