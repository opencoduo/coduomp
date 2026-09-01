#include "q_shared_misc.h"

#include <stdint.h>

enum {
    Q_COLOR_DIGIT_MAX = 9,
    Q_COLOR_DEFAULT_INDEX = 7
};

/*
 * The retained original bodies implement the same byte-domain mapping:
 *
 *   CoDUOMP.exe              0x0044f2d0
 *   coduo_lnxded             0x08086268
 *   game.mp.uo.i386.so       RVA 0x00092c9c
 *
 * The Mac client and game-module traceback symbols both retain the canonical
 * ColorIndex name.  CoDUOMP's former Q_ColorIndex spelling was a recovery-only
 * mismatch.
 */
char ColorIndex(char value)
{
    uint8_t index = (uint8_t)(value - '0');

    if (index > Q_COLOR_DIGIT_MAX) {
        index = Q_COLOR_DEFAULT_INDEX;
    }
    return (char)index;
}

/*
 * CoDUOMP.exe 0x0044f2e0, uo_cgame_mp_x86.dll 0x3004e0d0,
 * uo_ui_mp_x86.dll 0x40006100 and 0x40007820, uo_game_mp_x86.dll
 * 0x200578f0, coduo_lnxded 0x08086298, and game.mp.uo.i386.so RVA
 * 0x00092ccc all return minimum below the lower bound, maximum above the
 * upper bound, and the original value otherwise.  The two Linux bodies are
 * byte-identical to one another; the listed Windows bodies are likewise
 * byte-identical.  The ordered comparisons also agree for NaN: neither bound
 * is selected.
 */
float Com_Clamp(float minimum, float maximum, float value)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}
