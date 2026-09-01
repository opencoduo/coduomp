#include "client_common.h"

/*
 * The authoritative Windows client bodies are instruction-identical after
 * rebasing:
 *
 *   CoDUOMP.exe           0x0044f2e0
 *   uo_cgame_mp_x86.dll   0x3004e0d0
 *   uo_ui_mp_x86.dll      0x40006100
 *
 * Their ordered x87 comparisons deliberately return an unordered input
 * unchanged.
 */
float Com_ClampFloat(float minimum, float maximum, float value)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}
