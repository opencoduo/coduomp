#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x4000bb70..0x4000bc0e
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000bb70_4000bc0e.mcode
// Exact same-module PPC symbol: UI_CheckExecKey.
qboolean UI_CheckExecKey(int32_t key)
{
    menuDef_t *menu = Menu_GetFocused();

    if (g_editingField) {
        return qtrue;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)key >= (uint32_t)MAX_KEYS) {
        return qfalse;
    }
    if (menu == NULL) {
        if (trap_Cvar_VariableValue("cl_bypassMouseInput") == 0.0f) {
            trap_Cvar_Set("cl_bypassMouseInput", "0");
        }
        return qfalse;
    }
    return menu->onKey[key] != NULL;
}
