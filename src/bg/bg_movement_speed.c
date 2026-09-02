#include "bg_movement.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    BG_LADDER_JUMP_SPEED_DELAY = 500
};

/*
 * The Windows cgame/game bodies are instruction-identical apart from the
 * relocated zero constant: uo_cgame_mp_x86.dll 0x3000e7d0 and
 * uo_game_mp_x86.dll 0x2000e580.  Both receive playerState_t directly and
 * return the live PC=53 FSQRT result in ST0.
 *
 * Linux game RVA 0x0002e2c7 has the same branches and player-state offsets,
 * but passes a binary64 squared-speed sum through glibc sqrt and stores the
 * public result as binary32.  The complete bodies retain that boundary.
 */
#if defined(WINDOWS_BEHAVIOR)
long double BG_GetSpeed(const playerState_t *ps, int32_t time)
{
    if ((ps->playerStateFlags & PMF_LADDER) != 0) {
        const int32_t jumpAge = coduo_int32_from_bits((uint32_t)time - (uint32_t)ps->lastJumpCommandTime);
        if (jumpAge < BG_LADDER_JUMP_SPEED_DELAY) {
            return 0.0L;
        }
        return (long double)ps->velocity[2];
    }

#if EMULATE_X87
    const x87f squared = x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                  x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1])));
    return (long double)x87f_store_f64(x87f_sqrt(squared));
#else
    return coduo_x87_sqrtl((long double)ps->velocity[0] * (long double)ps->velocity[0] +
                           (long double)ps->velocity[1] * (long double)ps->velocity[1]);
#endif
}
#else
float BG_GetSpeed(const playerState_t *ps, int32_t time)
{
    if ((ps->playerStateFlags & PMF_LADDER) != 0) {
        const int32_t jumpAge = coduo_int32_from_bits((uint32_t)time - (uint32_t)ps->lastJumpCommandTime);
        if (jumpAge < BG_LADDER_JUMP_SPEED_DELAY) {
            return 0.0f;
        }
        return ps->velocity[2];
    }

#if EMULATE_X87
    const x87f squared = x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                  x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1])));
    return (float)CoduoLibm_SqrtGlibc(x87f_store_f64(squared));
#else
    const long double squared =
        (long double)ps->velocity[0] * (long double)ps->velocity[0] + (long double)ps->velocity[1] * (long double)ps->velocity[1];
    return (float)CoduoLibm_SqrtGlibc((double)squared);
#endif
}
#endif
