// Sources: uo_cgame_mp_x86.dll 0x300098c0..0x30009bb8,
//          uo_game_mp_x86.dll  0x20009670..0x20009968,
//          game.mp.uo.i386.so  0x000256ed..0x00025b9a

#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#if defined(LINUX_BEHAVIOR)
extern const float pm_stopspeed;
extern const float pm_friction;
extern const float pm_accelerate;
#endif

/*
 * PM_UFOMove — pmType UO_PM_TYPE_UFO's collision-free flight step. The exact
 * spelling comes from the same-module PPC name bank; PmoveSingle dispatches this
 * address for pmType 3, matching the isolated server pmType_t bank.
 *
 * The Windows cgame and game bodies have the same instruction stream apart
 * from relocated globals and callees. Linux retains the same decisions but
 * stores the speed and every friction/wish/integration stage as binary32.
 */
#if defined(WINDOWS_BEHAVIOR)
void PM_UFOMove(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    usercmd_t *cmd = &move->command;
    float speed;

    ps->viewHeightTarget = bg_viewheight_standing.integer;
    move->ps->standViewHeight = bg_viewheight_standing.integer;

    /*
     * The Windows compiler combines both button contributions into one byte
     * expression. The uint8_t operations preserve the original modulo-256
     * IMUL/ADD behavior without signed-overflow assumptions.
     */
    {
        const uint8_t wbuttons = cmd->wbuttons;
        const uint8_t buttonDelta = (uint8_t)((wbuttons & PM_WBUTTON_LEAN_LEFT) - (wbuttons & PM_WBUTTON_LEAN_RIGHT));
        const uint8_t buttonProduct = (uint8_t)(buttonDelta * 127u);

        cmd->upmove = (int8_t)((uint8_t)cmd->upmove + buttonProduct);
    }

    if (cmd->forwardmove == 0 && cmd->rightmove == 0) {
        if (cmd->upmove == 0) {
            speed = 0.0f;
        } else {
            move->ps->velocity[1] = 0.0f;
            move->ps->velocity[0] = 0.0f;
            move->ps->velocity[2] = (float)cmd->upmove;
            speed = 127.0f;
        }
    } else {
        /*
         * _CIsqrt consumes the live X/Y/Z x87 sum and the caller then stores
         * its result as binary32. There is no binary64 argument boundary.
         */
#if EMULATE_X87
        speed = x87f_store_f32(
            x87f_sqrt(x87f_add(x87f_add(x87f_mul(x87f_load_f32(move->ps->velocity[0]), x87f_load_f32(move->ps->velocity[0])),
                                        x87f_mul(x87f_load_f32(move->ps->velocity[1]), x87f_load_f32(move->ps->velocity[1]))),
                               x87f_mul(x87f_load_f32(move->ps->velocity[2]), x87f_load_f32(move->ps->velocity[2])))));
#else
        long double speedSquared = (long double)move->ps->velocity[0] * move->ps->velocity[0];
        speedSquared += (long double)move->ps->velocity[1] * move->ps->velocity[1];
        speedSquared += (long double)move->ps->velocity[2] * move->ps->velocity[2];
        speed = (float)coduo_x87_sqrtl(speedSquared);
#endif
    }

    if (speed < 1.0f) {
        move->ps->velocity[0] = 0.0f;
        move->ps->velocity[1] = 0.0f;
        move->ps->velocity[2] = 0.0f;
    } else {
        const float control = speed < 100.0f ? 100.0f : speed;
        float drop;
        float newSpeed;
        float frictionScale;

        /*
         * Drop is stored as binary32 before the subtraction; newSpeed and its
         * quotient are also stored before the next stage consumes them.
         */
#if EMULATE_X87
        drop = x87f_store_f32(
            x87f_add(x87f_mul(x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(control)), x87f_load_f32(8.25f)), x87f_load_f32(0.0f)));
        newSpeed = x87f_store_f32(x87f_sub(x87f_load_f32(speed), x87f_load_f32(drop)));
#else
        drop = (float)((long double)pml.frametime * (long double)control * 8.25L + 0.0L);
        newSpeed = (float)((long double)speed - (long double)drop);
#endif
        if (newSpeed < 0.0f) {
            newSpeed = 0.0f;
        }
#if EMULATE_X87
        frictionScale = x87f_store_f32(x87f_div(x87f_load_f32(newSpeed), x87f_load_f32(speed)));
        for (int32_t lane = 0; lane < 3; ++lane) {
            move->ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(frictionScale), x87f_load_f32(move->ps->velocity[lane])));
        }
#else
        frictionScale = (float)((long double)newSpeed / (long double)speed);
        for (int32_t lane = 0; lane < 3; ++lane) {
            move->ps->velocity[lane] = (float)((long double)frictionScale * (long double)move->ps->velocity[lane]);
        }
#endif
    }

    {
        const float commandScale = PM_CmdScale(cmd);
        const float forwardMove = (float)cmd->forwardmove;
        const float rightMove = (float)cmd->rightmove;
        const float upMove = (float)cmd->upmove;
        vec3_t forwardBasis;
        vec3_t wishdir;
        float wishSpeed;

        /*
         * The optimized Windows bodies inline CrossProduct({0,0,1},
         * pml.right): each multiply/subtract result is stored as binary32.
         * Retain those operations because signed zero and NaN behavior are
         * observable even though the ordinary finite result is {-right.y,
         * right.x, 0}.
         */
#if EMULATE_X87
        const float zeroFromRightZ = x87f_store_f32(x87f_mul(x87f_load_f32(pml.right[2]), x87f_load_f32(0.0f)));
        forwardBasis[0] = x87f_store_f32(x87f_sub(x87f_load_f32(zeroFromRightZ), x87f_load_f32(pml.right[1])));
        forwardBasis[1] = x87f_store_f32(x87f_sub(x87f_load_f32(pml.right[0]), x87f_load_f32(zeroFromRightZ)));
        forwardBasis[2] = x87f_store_f32(x87f_sub(x87f_mul(x87f_load_f32(pml.right[1]), x87f_load_f32(0.0f)),
                                                  x87f_mul(x87f_load_f32(pml.right[0]), x87f_load_f32(0.0f))));
#else
        const float zeroFromRightZ = (float)((long double)pml.right[2] * 0.0L);
        forwardBasis[0] = (float)((long double)zeroFromRightZ - (long double)pml.right[1]);
        forwardBasis[1] = (float)((long double)pml.right[0] - (long double)zeroFromRightZ);
        forwardBasis[2] = (float)((long double)pml.right[1] * 0.0L - (long double)pml.right[0] * 0.0L);
#endif

        for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
            wishdir[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.right[lane]), x87f_load_f32(rightMove)),
                                                    x87f_mul(x87f_load_f32(forwardBasis[lane]), x87f_load_f32(forwardMove))));
#else
            wishdir[lane] =
                (float)((long double)pml.right[lane] * (long double)rightMove + (long double)forwardBasis[lane] * (long double)forwardMove);
#endif
        }

        /*
         * The right/forward Z partial is already binary32 before the doubled
         * up-move is added.
         */
#if EMULATE_X87
        wishdir[2] = x87f_store_f32(x87f_add(x87f_add(x87f_load_f32(upMove), x87f_load_f32(upMove)), x87f_load_f32(wishdir[2])));
#else
        wishdir[2] = (float)(((long double)upMove + (long double)upMove) + (long double)wishdir[2]);
#endif

        wishSpeed = VectorNormalize(wishdir);
#if EMULATE_X87
        wishSpeed = x87f_store_f32(x87f_mul(x87f_load_f32(wishSpeed), x87f_load_f32(commandScale)));
#else
        wishSpeed = (float)((long double)wishSpeed * (long double)commandScale);
#endif
        PM_Accelerate(wishdir, wishSpeed, 9.0f);
    }

    for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        move->ps->psOrigin[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(move->ps->velocity[lane])),
                                                           x87f_load_f32(move->ps->psOrigin[lane])));
#else
        move->ps->psOrigin[lane] =
            (float)((long double)pml.frametime * (long double)move->ps->velocity[lane] + (long double)move->ps->psOrigin[lane]);
#endif
    }
}
#else
void PM_UFOMove(void)
{
    float speed;
    float frictionScale;
    float control;
    float drop;
    float newSpeed;
    vec3_t forwardBasis;
    vec3_t wishvel;
    vec3_t wishdir;

    pm->ps->viewHeightTarget = bg_viewheight_standing.integer;
    pm->ps->standViewHeight = bg_viewheight_standing.integer;

    /*
     * Linux emits the two button contributions as separate byte stores. The
     * modulo-256 expressions reproduce its shift/subtract and byte add/sub
     * without invoking signed overflow.
     */
    {
        const uint8_t wbuttons = pm->command.wbuttons;
        uint8_t upmove = (uint8_t)pm->command.upmove;

        upmove = (uint8_t)(upmove - (uint8_t)((uint8_t)(wbuttons & PM_WBUTTON_LEAN_RIGHT) * 127u));
        pm->command.upmove = (int8_t)upmove;
        upmove = (uint8_t)(upmove + (uint8_t)((uint8_t)(wbuttons & PM_WBUTTON_LEAN_LEFT) * 127u));
        pm->command.upmove = (int8_t)upmove;
    }

    if (pm->command.forwardmove == 0 && pm->command.rightmove == 0) {
        if (pm->command.upmove == 0) {
            speed = 0.0f;
        } else {
            pm->ps->velocity[1] = 0.0f;
            pm->ps->velocity[0] = 0.0f;
            speed = 127.0f;
            pm->ps->velocity[2] = (float)pm->command.upmove;
        }
    } else {
        /*
         * Linux narrows the PC=64 X/Y/Z sum to the binary64 glibc-sqrt
         * argument, then stores the returned root as binary32.
         */
#if EMULATE_X87
        speed = (float)CoduoLibm_SqrtGlibc(
            x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(pm->ps->velocity[0]), x87f_load_f32(pm->ps->velocity[0])),
                                             x87f_mul(x87f_load_f32(pm->ps->velocity[1]), x87f_load_f32(pm->ps->velocity[1]))),
                                    x87f_mul(x87f_load_f32(pm->ps->velocity[2]), x87f_load_f32(pm->ps->velocity[2])))));
#else
        const long double speedSquared =
            ((long double)pm->ps->velocity[0] * pm->ps->velocity[0] + (long double)pm->ps->velocity[1] * pm->ps->velocity[1]) +
            (long double)pm->ps->velocity[2] * pm->ps->velocity[2];
        speed = (float)CoduoLibm_SqrtGlibc((double)speedSquared);
#endif
    }

    if (speed < 1.0f) {
        pm->ps->velocity[0] = 0.0f;
        pm->ps->velocity[1] = 0.0f;
        pm->ps->velocity[2] = 0.0f;
    } else {
#if EMULATE_X87
        frictionScale = x87f_store_f32(x87f_mul(x87f_load_f32(pm_friction), x87f_load_f32(1.5f)));
#else
        frictionScale = (float)((long double)pm_friction * 1.5L);
#endif
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
        drop = 0.0f;
#if EMULATE_X87
        drop = x87f_store_f32(x87f_add(
            x87f_mul(x87f_mul(x87f_load_f32(control), x87f_load_f32(frictionScale)), x87f_load_f32(pml.frametime)), x87f_load_f32(drop)));
        newSpeed = x87f_store_f32(x87f_sub(x87f_load_f32(speed), x87f_load_f32(drop)));
#else
        drop = (float)((long double)control * (long double)frictionScale * (long double)pml.frametime + (long double)drop);
        newSpeed = (float)((long double)speed - (long double)drop);
#endif
        if (newSpeed < 0.0f) {
            newSpeed = 0.0f;
        }
#if EMULATE_X87
        newSpeed = x87f_store_f32(x87f_div(x87f_load_f32(newSpeed), x87f_load_f32(speed)));
        for (int32_t lane = 0; lane < 3; ++lane) {
            pm->ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(pm->ps->velocity[lane]), x87f_load_f32(newSpeed)));
        }
#else
        newSpeed = (float)((long double)newSpeed / (long double)speed);
        for (int32_t lane = 0; lane < 3; ++lane) {
            pm->ps->velocity[lane] = (float)((long double)pm->ps->velocity[lane] * (long double)newSpeed);
        }
#endif
    }

    const float commandScale = PM_CmdScale(&pm->command);
    const float forwardMove = (float)pm->command.forwardmove;
    const float rightMove = (float)pm->command.rightmove;
    const float upMove = (float)pm->command.upmove;
    const vec3_t upAxis = {0.0f, 0.0f, 1.0f};

    CrossProduct(upAxis, pml.right, forwardBasis);
    for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        wishvel[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(forwardBasis[lane]), x87f_load_f32(forwardMove)),
                                                x87f_mul(x87f_load_f32(pml.right[lane]), x87f_load_f32(rightMove))));
#else
        wishvel[lane] =
            (float)((long double)forwardBasis[lane] * (long double)forwardMove + (long double)pml.right[lane] * (long double)rightMove);
#endif
    }

#if EMULATE_X87
    wishvel[2] = x87f_store_f32(x87f_add(x87f_add(x87f_load_f32(upMove), x87f_load_f32(upMove)), x87f_load_f32(wishvel[2])));
#else
    wishvel[2] = (float)(((long double)upMove + (long double)upMove) + (long double)wishvel[2]);
#endif

    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = wishvel[2];
    float wishSpeed = VectorNormalize(wishdir);
#if EMULATE_X87
    wishSpeed = x87f_store_f32(x87f_mul(x87f_load_f32(wishSpeed), x87f_load_f32(commandScale)));
#else
    wishSpeed = (float)((long double)wishSpeed * (long double)commandScale);
#endif

    PM_Accelerate(wishdir, wishSpeed, pm_accelerate);

    for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        pm->ps->psOrigin[lane] = x87f_store_f32(
            x87f_add(x87f_mul(x87f_load_f32(pm->ps->velocity[lane]), x87f_load_f32(pml.frametime)), x87f_load_f32(pm->ps->psOrigin[lane])));
#else
        pm->ps->psOrigin[lane] =
            (float)((long double)pm->ps->velocity[lane] * (long double)pml.frametime + (long double)pm->ps->psOrigin[lane]);
#endif
    }
}
#endif
