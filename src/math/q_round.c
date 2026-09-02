#include "q_math.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#include <limits.h>
#include <math.h>

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00408e90..0x00408eb0.
 * Evidence: the exact qword at 0x005b9d00 is 2^-30. The x87 body adds that
 * binary64 bias to the binary32 input and converts with FISTP under the
 * process round-to-nearest policy. Name: exact Mac client symbol FastRound. */
int32_t FastRound(float value)
{
    const double rounded = rint((double)value + 0x1p-30);

    if (!(rounded >= (double)INT32_MIN && rounded <= (double)INT32_MAX))
        return INT32_MIN;

    return (int32_t)rounded;
}
#else
/* Source: coduo_lnxded 0x080b0834..0x080b085b. The Linux body adds the
 * binary32 0.5 value while the intermediate remains in x87, temporarily sets
 * the x87 conversion mode to truncate, executes FISTP, and restores the
 * caller's control word. This differs observably from the Windows algorithm
 * for negative inputs and therefore remains a complete behavior-specific
 * function. */
int32_t FastRound(float value)
{
#if EMULATE_X87
    return x87f_store_i32_trunc(
        x87f_add(x87f_load_f32(value), x87f_load_f32(0.5f)));
#elif defined(__x86_64__)
    static const float half = 0.5f;
    return CODUO_X87_TRUNCATE_I32(
        (long double)value + (long double)half);
#else
    return (int32_t)(value + 0.5f);
#endif
}
#endif
