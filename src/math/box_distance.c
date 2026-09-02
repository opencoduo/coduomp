#include "q_math.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>

/*
 * The four Windows bodies have the same instruction stream apart from the
 * relocated zero constant:
 *
 *   CoDUOMP.exe                 0x00434930
 *   uo_cgame_mp_x86.dll        0x3004ca90
 *   uo_ui_mp_x86.dll           0x40004aa0
 *   uo_game_mp_x86.dll         0x20019ae0
 *
 * Linux coduo_lnxded 0x0806ab7e and game.mp.uo.i386.so RVA 0x0003ea0e
 * retain the same binary32 delta, square, selected-sum, and result stores.
 * Compiler scheduling and register allocation differ, but there is no
 * platform-specific computation.  In particular, each same-side product is
 * compared before a binary32 spill, while every square and accumulation is
 * explicitly narrowed to binary32.
 */
qboolean BoxDistSqrdExceeds(const vec3_t mins, const vec3_t maxs,
                            const vec3_t point, float distanceSquared)
{
    vec3_t minDelta;
    vec3_t maxDelta;
    float accumulated = 0.0f;

    for (int32_t component = 0; component < 3; ++component) {
        minDelta[component] = mins[component] - point[component];
        maxDelta[component] = maxs[component] - point[component];
    }

    for (int32_t component = 0; component < 3; ++component) {
#if EMULATE_X87
        const x87f sideProduct = x87f_mul(
            x87f_load_f32(minDelta[component]),
            x87f_load_f32(maxDelta[component]));
        const qboolean sameSide =
            !x87f_le(sideProduct, x87f_load_f32(0.0f));
#else
        const long double sideProduct =
            (long double)minDelta[component] *
            (long double)maxDelta[component];
        const qboolean sameSide = !(sideProduct <= 0.0L);
#endif

        if (sameSide != qfalse) {
#if EMULATE_X87
            const float minDeltaSquared = x87f_store_f32(x87f_mul(
                x87f_load_f32(minDelta[component]),
                x87f_load_f32(minDelta[component])));
            const float maxDeltaSquared = x87f_store_f32(x87f_mul(
                x87f_load_f32(maxDelta[component]),
                x87f_load_f32(maxDelta[component])));
#else
            const float minDeltaSquared = (float)(
                (long double)minDelta[component] *
                (long double)minDelta[component]);
            const float maxDeltaSquared = (float)(
                (long double)maxDelta[component] *
                (long double)maxDelta[component]);
#endif
            const float selected =
                maxDeltaSquared < minDeltaSquared
                    ? maxDeltaSquared
                    : minDeltaSquared;
#if EMULATE_X87
            accumulated = x87f_store_f32(x87f_add(
                x87f_load_f32(accumulated), x87f_load_f32(selected)));
#else
            accumulated = (float)((long double)accumulated +
                                  (long double)selected);
#endif
        }
    }

    return accumulated > distanceSquared ? qtrue : qfalse;
}
