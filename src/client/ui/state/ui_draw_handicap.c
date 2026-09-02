#include "../module/ui_functions.h"

enum {
    UI_HANDICAP_MINIMUM = 5,
    UI_HANDICAP_MAXIMUM = 100,
    UI_HANDICAP_STEP = 5
};

/* Source: uo_ui_mp_x86.dll data 0x400402ac..0x40040303.
 * PE_RELOCATION_VALUES_VERIFIED: all 20 non-NULL pointers target the listed
 * labels in order. The original indexes backward from the final NULL slot at
 * 0x40040300. */
const char *const ui_handicapLabels[22] = {
    NULL, "None", "95", "90", "85", "80", "75", "70", "65", "60",
    "55", "50", "45", "40", "35", "30", "25", "20", "15", "10",
    "5", NULL
};

// Source: uo_ui_mp_x86.dll 0x400099e0..0x40009a9d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400099e0_40009a9d.mcode
// Exact same-module PPC symbol: UI_DrawHandicap.
void UI_DrawHandicap(const rectDef_t *rect, int32_t font, float scale,
                     const vec4_t color, int32_t textStyle)
{
    float handicap = trap_Cvar_VariableValue("handicap");
    int32_t labelIndex;

    if (handicap < (float)UI_HANDICAP_MINIMUM) {
        handicap = (float)UI_HANDICAP_MINIMUM;
    } else if (handicap > (float)UI_HANDICAP_MAXIMUM) {
        handicap = (float)UI_HANDICAP_MAXIMUM;
    }

    /* 0x40009a54 calls _ftol2 and consumes its low dword. */
    labelIndex =
        coduo_fp_to_i32_extended((long double)handicap) / UI_HANDICAP_STEP;
    trap_R_Text_Paint(rect->x, rect->y, font, scale, color,
                      ui_handicapLabels[21 - labelIndex], 0, 0, textStyle);
}
