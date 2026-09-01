#include "q_math.h"

#include <math.h>

/*
 * Q_rint is byte-for-byte the same source computation in all six retained
 * authoritative images (apart from relocated constants and floor targets):
 *
 *   CoDUOMP.exe                 0x00434b70
 *   uo_cgame_mp_x86.dll        0x3004ccd0
 *   uo_ui_mp_x86.dll           0x40004ce0
 *   uo_game_mp_x86.dll         0x20019d20
 *   coduo_lnxded               0x0806ae2f
 *   game.mp.uo.i386.so         RVA 0x0003ecef
 *
 * Each body adds binary32 0.5 to the binary32 argument, stores the exact sum
 * as binary64 for floor, narrows floor's result to binary32, and returns that
 * binary32 value.  This is not an integer-conversion helper.
 */
float Q_rint(float value)
{
    return (float)floor((double)value + 0.5);
}
