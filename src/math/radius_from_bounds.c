#include "q_math.h"

#include <math.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The four authoritative Windows bodies are instruction-identical apart from
 * their image-local sqrt helper:
 *
 *   CoDUOMP.exe                 0x00433e80
 *   uo_cgame_mp_x86.dll        0x3004bfe0
 *   uo_ui_mp_x86.dll           0x40003ff0
 *   uo_game_mp_x86.dll         0x20019030
 *
 * They select fabs(first[lane]) only when it is ordered-greater than
 * fabs(second[lane]); equality and unordered select the second operand.  Their
 * squared sum is (z*z + y*y) + x*x under Windows PC=53, then the CRT sqrt
 * helper narrows that sum to binary64 and the function narrows the result to
 * binary32.
 *
 * Both Linux bodies make the same per-lane selection at coduo_lnxded
 * 0x0806a0c3 and game.mp.uo.i386.so RVA 0x0003de80.  They instead sum
 * (x*x + y*y) + z*z under PC=64 before storing binary64 for sqrt(double).
 */
#if defined(WINDOWS_BEHAVIOR)
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
    vec3_t corner;

    for (int32_t axis = 0; axis < 3; ++axis) {
        const float minMagnitude = fabsf(mins[axis]);
        const float maxMagnitude = fabsf(maxs[axis]);

        /* RECONSTRUCTION_FIX: earlier CoDUOMP/UI source reversed this
         * predicate.  The four Windows bodies load/compare the first operand
         * and retain it only for ordered-greater; this spelling also preserves
         * the original second-operand choice for equality and NaNs. */
        corner[axis] = minMagnitude > maxMagnitude ? minMagnitude : maxMagnitude;
    }

    /* Volatile separates the two original FADDP operations and prevents an
     * SSE/NEON compiler from reassociating or contracting the graph. */
    volatile double zy = (double)corner[2] * (double)corner[2] + (double)corner[1] * (double)corner[1];
    const double squared = zy + (double)corner[0] * (double)corner[0];

    return (float)sqrt(squared);
}
#else
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
    vec3_t corner;

    for (int32_t axis = 0; axis < 3; ++axis) {
        const float minMagnitude = fabsf(mins[axis]);
        const float maxMagnitude = fabsf(maxs[axis]);
        corner[axis] = minMagnitude > maxMagnitude ? minMagnitude : maxMagnitude;
    }

#if EMULATE_X87
    x87f squared = x87f_add(x87f_mul(x87f_load_f32(corner[0]), x87f_load_f32(corner[0])),
                            x87f_mul(x87f_load_f32(corner[1]), x87f_load_f32(corner[1])));
    squared = x87f_add(squared, x87f_mul(x87f_load_f32(corner[2]), x87f_load_f32(corner[2])));
    return (float)sqrt(x87f_store_f64(squared));
#else
    const long double xy = (long double)corner[0] * (long double)corner[0] + (long double)corner[1] * (long double)corner[1];
    const long double squared = xy + (long double)corner[2] * (long double)corner[2];

    return (float)sqrt((double)squared);
#endif
}
#endif
