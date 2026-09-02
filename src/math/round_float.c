#include "q_math.h"

#include "compat/coduo_x87emu.h"

#include <math.h>

/*
 * The complete fixed-decimal rounding body is retained in every authoritative
 * Windows component and both Linux server components:
 *
 *   CoDUOMP.exe                 0x00434f00
 *   uo_cgame_mp_x86.dll        0x3004d060
 *   uo_ui_mp_x86.dll           0x40005070
 *   uo_game_mp_x86.dll         0x2001a0b0
 *   coduo_lnxded               0x0806b2e2
 *   game.mp.uo.i386.so         RVA 0x0003f200
 *
 * The four Windows bodies are instruction-identical apart from call and data
 * relocations.  Both Linux bodies use the same arithmetic and store graph:
 * convert decimals to binary64, scale by pow(10), store the product to
 * binary64, split it with modf, adjust the binary64 whole part by one at the
 * two half thresholds, then multiply by pow(0.1) and narrow once to binary32.
 * Native x87 builds inherit their selected process precision control; the
 * emulated path performs the two unspilled x87 multiplies explicitly.
 *
 * Linux game exports the canonical RoundFloat name.  The supporting Mac
 * engine instead exports round_decimal for the engine copy; that is a naming
 * difference only, so the shared reconstruction uses RoundFloat throughout.
 */
float RoundFloat(float value, int32_t decimals)
{
    const double exponent = (double)decimals;
    double scaled;
    double whole;
    double fraction;

#if EMULATE_X87
    scaled = x87f_store_f64(x87f_mul(
        x87f_load_f64(pow(10.0, exponent)), x87f_load_f32(value)));
#else
    scaled = (double)((long double)pow(10.0, exponent) *
                      (long double)value);
#endif
    fraction = modf(scaled, &whole);

    if (fraction >= 0.5) {
        whole += 1.0;
    } else if (fraction <= -0.5) {
        whole -= 1.0;
    }

#if EMULATE_X87
    return x87f_store_f32(x87f_mul(
        x87f_load_f64(pow(0.1, exponent)), x87f_load_f64(whole)));
#else
    return (float)((long double)pow(0.1, exponent) *
                   (long double)whole);
#endif
}
