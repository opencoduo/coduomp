#include "client_math.h"

#include "compat/coduo_x87emu.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

/*
 * The Windows cgame body at 0x3003b4b0 and UI body at 0x40011850 are
 * instruction-identical after rebasing.  Both load the binary64 constant
 * 0x3fdfffffff000000, subtract it from the binary32 argument in x87, and use
 * the active round-to-nearest FISTP conversion without an intervening store.
 */
int32_t Script_BiasedRoundToInt(float value)
{
    static const double bias = 0.499999999068677425384521484375;

#if EMULATE_X87
    return x87f_store_i32(x87f_sub(x87f_load_f32(value), x87f_load_f64(bias)));
#else
    const long double rounded = nearbyintl((long double)value - (long double)bias);

    /* Masked x87 FISTP returns the signed indefinite value for NaN and
     * out-of-range inputs; an ISO C integer cast would be undefined. */
    if (!(rounded >= (long double)INT32_MIN && rounded <= (long double)INT32_MAX)) {
        return INT32_MIN;
    }
    return (int32_t)rounded;
#endif
}
