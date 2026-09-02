#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#define PM_DEAD_MOVE_SPEED_DROP 20.0f

/*
 * Canonical Quake3/CoD grounded dead-player velocity decay. The Windows
 * modules are instruction-identical apart from relocations:
 *
 *   uo_cgame_mp_x86.dll  0x30009660..0x300096f3
 *   uo_game_mp_x86.dll   0x20009410..0x200094a3
 *
 * Linux game has the same decisions at 0x00025246..0x0002538a. Windows keeps
 * its square-root/subtraction chain live; Linux passes a binary64 squared sum
 * to glibc sqrt and stores both the root and reduced speed as binary32.
 */
#if defined(WINDOWS_BEHAVIOR)
void PM_DeadMove(void)
{
    if (pml.walking == 0) {
        return;
    }

    playerState_t *ps = pm->ps;
#if EMULATE_X87
    x87f reducedSpeed = x87f_sub(x87f_sqrt(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                                             x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1]))),
                                                    x87f_mul(x87f_load_f32(ps->velocity[2]), x87f_load_f32(ps->velocity[2])))),
                                 x87f_load_f32(PM_DEAD_MOVE_SPEED_DROP));
    const float storedSpeed = x87f_store_f32(reducedSpeed);
    if (x87f_le_signaling(reducedSpeed, x87f_load_f32(0.0f))) {
#else
    const long double reducedSpeed =
        sqrtl(((long double)ps->velocity[0] * (long double)ps->velocity[0] + (long double)ps->velocity[1] * (long double)ps->velocity[1]) +
              (long double)ps->velocity[2] * (long double)ps->velocity[2]) -
        (long double)PM_DEAD_MOVE_SPEED_DROP;
    const float storedSpeed = (float)reducedSpeed;
    if (reducedSpeed <= 0.0L) {
#endif
        ps->velocity[2] = 0.0f;
        pm->ps->velocity[1] = 0.0f;
        pm->ps->velocity[0] = 0.0f;
        return;
    }

    (void)VectorNormalize(ps->velocity);
#if EMULATE_X87
    pm->ps->velocity[0] = x87f_store_f32(x87f_mul(x87f_load_f32(storedSpeed), x87f_load_f32(pm->ps->velocity[0])));
    pm->ps->velocity[1] = x87f_store_f32(x87f_mul(x87f_load_f32(storedSpeed), x87f_load_f32(pm->ps->velocity[1])));
    pm->ps->velocity[2] = x87f_store_f32(x87f_mul(x87f_load_f32(storedSpeed), x87f_load_f32(pm->ps->velocity[2])));
#else
    pm->ps->velocity[0] = (float)((long double)storedSpeed * (long double)pm->ps->velocity[0]);
    pm->ps->velocity[1] = (float)((long double)storedSpeed * (long double)pm->ps->velocity[1]);
    pm->ps->velocity[2] = (float)((long double)storedSpeed * (long double)pm->ps->velocity[2]);
#endif
}
#else
void PM_DeadMove(void)
{
    if (pml.walking == 0) {
        return;
    }

    playerState_t *const ps = pm->ps;
#if EMULATE_X87
    const double squaredSpeed = x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                                                 x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1]))),
                                                        x87f_mul(x87f_load_f32(ps->velocity[2]), x87f_load_f32(ps->velocity[2]))));
    float speed = x87f_store_f32(x87f_load_f64(CoduoLibm_SqrtGlibc(squaredSpeed)));
    speed = x87f_store_f32(x87f_sub(x87f_load_f32(speed), x87f_load_f32(PM_DEAD_MOVE_SPEED_DROP)));
#else
    const long double squaredSpeed =
        ((long double)ps->velocity[0] * (long double)ps->velocity[0] + (long double)ps->velocity[1] * (long double)ps->velocity[1]) +
        (long double)ps->velocity[2] * (long double)ps->velocity[2];
    float speed = (float)CoduoLibm_SqrtGlibc((double)squaredSpeed);
    speed = (float)((long double)speed - (long double)PM_DEAD_MOVE_SPEED_DROP);
#endif

    if (speed <= 0.0f) {
        ps->velocity[2] = 0.0f;
        pm->ps->velocity[1] = 0.0f;
        pm->ps->velocity[0] = 0.0f;
        return;
    }

    (void)VectorNormalize(ps->velocity);
#if EMULATE_X87
    pm->ps->velocity[0] = x87f_store_f32(x87f_mul(x87f_load_f32(pm->ps->velocity[0]), x87f_load_f32(speed)));
    pm->ps->velocity[1] = x87f_store_f32(x87f_mul(x87f_load_f32(pm->ps->velocity[1]), x87f_load_f32(speed)));
    pm->ps->velocity[2] = x87f_store_f32(x87f_mul(x87f_load_f32(pm->ps->velocity[2]), x87f_load_f32(speed)));
#else
    pm->ps->velocity[0] = (float)((long double)pm->ps->velocity[0] * (long double)speed);
    pm->ps->velocity[1] = (float)((long double)pm->ps->velocity[1] * (long double)speed);
    pm->ps->velocity[2] = (float)((long double)pm->ps->velocity[2] * (long double)speed);
#endif
}
#endif
