#include "../module/ui_functions.h"

enum {
    UI_HANDICAP_MINIMUM = 5,
    UI_HANDICAP_MAXIMUM = 100,
    UI_HANDICAP_STEP = 5
};

// Source: uo_ui_mp_x86.dll 0x4000b210..0x4000b2c7
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b210_4000b2c7.mcode
// Exact same-module PPC symbol: UI_Handicap_HandleKey.
qboolean UI_Handicap_HandleKey(int32_t flags, float *special, int32_t key)
{
    float handicap;
    int32_t value;

    (void)flags;
    (void)special;

    if (key != K_MOUSE1 && key != K_MOUSE2 && key != K_ENTER && key != K_KP_ENTER) {
        return qfalse;
    }

    handicap = trap_Cvar_VariableValue("handicap");
    if (handicap < (float)UI_HANDICAP_MINIMUM) {
        handicap = (float)UI_HANDICAP_MINIMUM;
    } else if (handicap > (float)UI_HANDICAP_MAXIMUM) {
        handicap = (float)UI_HANDICAP_MAXIMUM;
    }

    /* 0x4000b27a calls _ftol2 and consumes its low dword. */
    value = coduo_fp_to_i32_extended((long double)handicap);
    value += key == K_MOUSE2 ? -UI_HANDICAP_STEP : UI_HANDICAP_STEP;
    if (value > UI_HANDICAP_MAXIMUM) {
        value = UI_HANDICAP_MINIMUM;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    } else if (value < UI_HANDICAP_MINIMUM) {
        value = UI_HANDICAP_MAXIMUM;
    }

    trap_Cvar_Set("handicap", va("%i", value));
    return qtrue;
}
