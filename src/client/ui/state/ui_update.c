#include "../module/ui_functions.h"

enum {
    UI_RATE_HIGH_THRESHOLD = 5000,
    UI_RATE_MEDIUM_THRESHOLD = 4000,
    UI_COLOR_BITS_DEFAULT = 0,
    UI_COLOR_BITS_16 = 16,
    UI_COLOR_BITS_32 = 32,
    UI_LOD_BIAS_HIGH = 0,
    UI_LOD_BIAS_MEDIUM = 1,
    UI_LOD_BIAS_LOW = 2,
    UI_UPDATE_NAME_COMPARE_LIMIT = 99999
};

// Source: uo_ui_mp_x86.dll 0x4000bc10..0x4000bee9
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000bc10_4000bee9.mcode
// Exact same-module PPC symbol: UI_Update.
void UI_Update(const char *name)
{
    int32_t value = coduo_fp_to_i32_extended(
        (long double)trap_Cvar_VariableValue(name));

    if (name == NULL) {
        return;
    }
    if (Q_stricmpn("ui_SetName", name, UI_UPDATE_NAME_COMPARE_LIMIT) == 0) {
        trap_Cvar_Set("name", UI_Cvar_VariableString("ui_Name"));
        return;
    }
    if (Q_stricmpn("ui_setRate", name, UI_UPDATE_NAME_COMPARE_LIMIT) == 0) {
        float rate = trap_Cvar_VariableValue("rate");

        if (rate >= (float)UI_RATE_HIGH_THRESHOLD) {
            trap_Cvar_Set("cl_maxpackets", "30");
            trap_Cvar_Set("cl_packetdup", "1");
        } else if (rate >= (float)UI_RATE_MEDIUM_THRESHOLD) {
            trap_Cvar_Set("cl_maxpackets", "15");
            trap_Cvar_Set("cl_packetdup", "2");
        } else {
            trap_Cvar_Set("cl_maxpackets", "15");
            trap_Cvar_Set("cl_packetdup", "1");
        }
        return;
    }
    if (Q_stricmpn("ui_GetName", name, UI_UPDATE_NAME_COMPARE_LIMIT) == 0) {
        trap_Cvar_Set("ui_Name", UI_Cvar_VariableString("name"));
        return;
    }
    if (Q_stricmpn("r_colorbits", name, UI_UPDATE_NAME_COMPARE_LIMIT) == 0) {
        if (value == UI_COLOR_BITS_DEFAULT) {
            trap_Cvar_SetValue("r_depthbits", 0.0f);
            trap_Cvar_SetValue("r_stencilbits", 0.0f);
        } else if (value == UI_COLOR_BITS_16) {
            trap_Cvar_SetValue("r_depthbits", 16.0f);
            trap_Cvar_SetValue("r_stencilbits", 0.0f);
        } else if (value == UI_COLOR_BITS_32) {
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            trap_Cvar_SetValue("r_depthbits", 24.0f);
        }
        return;
    }
    if (Q_stricmpn("r_lodbias", name, UI_UPDATE_NAME_COMPARE_LIMIT) == 0) {
        if (value == UI_LOD_BIAS_HIGH) {
            trap_Cvar_SetValue("r_subdivisions", 4.0f);
        } else if (value == UI_LOD_BIAS_MEDIUM) {
            trap_Cvar_SetValue("r_subdivisions", 12.0f);
        } else if (value == UI_LOD_BIAS_LOW) {
            trap_Cvar_SetValue("r_subdivisions", 20.0f);
        }
        return;
    }
    if (Q_stricmpn("ui_mousePitch", name,
                   UI_UPDATE_NAME_COMPARE_LIMIT) == 0) {
        trap_Cvar_SetValue("m_pitch", value == 0 ? 0.022f : -0.022f);
    }
}
