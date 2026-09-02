#include "q_math.h"

#include <math.h>

/*
 * The one-argument floor helper is retained in all three Windows client
 * components and the Linux dedicated engine:
 *
 *   CoDUOMP.exe          0x00434b70
 *   uo_cgame_mp_x86.dll 0x3004ccd0
 *   uo_ui_mp_x86.dll    0x40004ce0
 *   coduo_lnxded         0x0806ae2f
 *
 * Every body adds the same binary32 0.5 constant in x87, stores the sum to a
 * binary64 floor argument, narrows floor's result to binary32, and returns the
 * reloaded binary32 value.  The Windows bodies are instruction-identical apart
 * from constant/call relocations.  Linux uses its normal frame and PLT call but
 * has the same operation and store graph.  Since both operands of the addition
 * are binary32 and one is exactly 0.5, the sum is exact under both original
 * x87 precision-control settings; there is no platform behavior split.
 */
float FloatRoundNearest(float value)
{
    const double floorArgument =
        (double)((long double)value + (long double)0.5f);

    return (float)floor(floorArgument);
}
