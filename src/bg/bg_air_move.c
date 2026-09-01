#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* The unoptimized Linux module loads this exported constant through the GOT.
 * Both optimized Windows modules propagate the same binary32 value directly
 * into the PM_Accelerate call. */
const float pm_airaccelerate = 1.0f;

/*
 * The authoritative Windows bodies are instruction-identical apart from
 * relocated globals and callees:
 *
 *   uo_cgame_mp_x86.dll  0x30009060..0x300091dd
 *   uo_game_mp_x86.dll   0x20008e10..0x20008f8d
 *
 * Linux game retains the same movement decisions at
 * 0x000249cc..0x00024b7b. Its unoptimized body stores the normalized wish
 * length before scaling and calls its independently shared PM_ClipVelocity;
 * Windows keeps those values live and inlines the target-specific clip graph.
 */
#if defined(WINDOWS_BEHAVIOR)
void PM_AirMove(void)
{
    PM_Friction();

    pmove_t *const move = pm;
    const float forwardMove = (float)move->command.forwardmove;
    const float rightMove = (float)move->command.rightmove;
    usercmd_t command = move->command;
    const float commandScale = PM_CmdScale(&command);

    pml.forward[2] = 0.0f;
    pml.right[2] = 0.0f;
    (void)VectorNormalize(pml.forward);
    (void)VectorNormalize(pml.right);

    vec3_t wishDirection;
#if EMULATE_X87
    wishDirection[0] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(pml.right[0]),
                 x87f_load_f32(rightMove)),
        x87f_mul(x87f_load_f32(pml.forward[0]),
                 x87f_load_f32(forwardMove))));
    wishDirection[1] = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(pml.right[1]),
                 x87f_load_f32(rightMove)),
        x87f_mul(x87f_load_f32(pml.forward[1]),
                 x87f_load_f32(forwardMove))));
#else
    wishDirection[0] = (float)(
        (long double)pml.right[0] * (long double)rightMove +
        (long double)pml.forward[0] * (long double)forwardMove);
    wishDirection[1] = (float)(
        (long double)pml.right[1] * (long double)rightMove +
        (long double)pml.forward[1] * (long double)forwardMove);
#endif
    wishDirection[2] = 0.0f;

    const float normalizedLength = VectorNormalize(wishDirection);
#if EMULATE_X87
    const float wishSpeed = x87f_store_f32(x87f_mul(
        x87f_load_f32(normalizedLength),
        x87f_load_f32(commandScale)));
#else
    const float wishSpeed = (float)(
        (long double)normalizedLength * (long double)commandScale);
#endif
    PM_Accelerate(wishDirection, wishSpeed, pm_airaccelerate);

    if (pml.groundPlane != 0) {
        playerState_t *const ps = move->ps;

#if EMULATE_X87
        x87f backoff = x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(pml.groundTrace.normal[1]),
                         x87f_load_f32(ps->velocity[1])),
                x87f_mul(x87f_load_f32(pml.groundTrace.normal[2]),
                         x87f_load_f32(ps->velocity[2]))),
            x87f_mul(x87f_load_f32(pml.groundTrace.normal[0]),
                     x87f_load_f32(ps->velocity[0])));
        const float overbounce = x87f_lt_signaling(
            backoff, x87f_load_f32(0.0f)) ? 1.001f : 0.99900097f;
        backoff = x87f_mul(backoff, x87f_load_f32(overbounce));

        ps->velocity[0] = x87f_store_f32(x87f_sub(
            x87f_load_f32(ps->velocity[0]),
            x87f_mul(x87f_load_f32(pml.groundTrace.normal[0]), backoff)));
        ps->velocity[1] = x87f_store_f32(x87f_sub(
            x87f_load_f32(ps->velocity[1]),
            x87f_mul(x87f_load_f32(pml.groundTrace.normal[1]), backoff)));
        ps->velocity[2] = x87f_store_f32(x87f_sub(
            x87f_load_f32(ps->velocity[2]),
            x87f_mul(x87f_load_f32(pml.groundTrace.normal[2]), backoff)));
#else
        long double backoff =
            ((long double)pml.groundTrace.normal[1] *
                 (long double)ps->velocity[1] +
             (long double)pml.groundTrace.normal[2] *
                 (long double)ps->velocity[2]) +
            (long double)pml.groundTrace.normal[0] *
                (long double)ps->velocity[0];
        const float overbounce = backoff < 0.0L ? 1.001f : 0.99900097f;
        backoff *= (long double)overbounce;

        ps->velocity[0] = (float)(
            (long double)ps->velocity[0] -
            (long double)pml.groundTrace.normal[0] * backoff);
        ps->velocity[1] = (float)(
            (long double)ps->velocity[1] -
            (long double)pml.groundTrace.normal[1] * backoff);
        ps->velocity[2] = (float)(
            (long double)ps->velocity[2] -
            (long double)pml.groundTrace.normal[2] * backoff);
#endif
    }

    PM_StepSlideMove(1);
    PM_SetMovementDir();
}
#else
void PM_AirMove(void)
{
    vec3_t wishVelocity;
    vec3_t wishDirection;

    PM_Friction();

    const float forwardMove = (float)pm->command.forwardmove;
    const float rightMove = (float)pm->command.rightmove;
    usercmd_t command = pm->command;
    const float commandScale = PM_CmdScale(&command);

    pml.forward[2] = 0.0f;
    pml.right[2] = 0.0f;
    (void)VectorNormalize(pml.forward);
    (void)VectorNormalize(pml.right);

    for (int32_t lane = 0; lane < 2; ++lane) {
#if EMULATE_X87
        wishVelocity[lane] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(pml.forward[lane]),
                     x87f_load_f32(forwardMove)),
            x87f_mul(x87f_load_f32(pml.right[lane]),
                     x87f_load_f32(rightMove))));
#else
        wishVelocity[lane] = (float)(
            (long double)pml.forward[lane] * (long double)forwardMove +
            (long double)pml.right[lane] * (long double)rightMove);
#endif
    }
    wishVelocity[2] = 0.0f;

    wishDirection[0] = wishVelocity[0];
    wishDirection[1] = wishVelocity[1];
    wishDirection[2] = 0.0f;
    float wishSpeed = VectorNormalize(wishDirection);
#if EMULATE_X87
    wishSpeed = x87f_store_f32(x87f_mul(
        x87f_load_f32(wishSpeed), x87f_load_f32(commandScale)));
#else
    wishSpeed = (float)((long double)wishSpeed *
                        (long double)commandScale);
#endif

    PM_Accelerate(wishDirection, wishSpeed, pm_airaccelerate);

    if (pml.groundPlane != 0) {
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.normal,
                        pm->ps->velocity, 1.001f);
    }

    PM_StepSlideMove(1);
    PM_SetMovementDir();
}
#endif
