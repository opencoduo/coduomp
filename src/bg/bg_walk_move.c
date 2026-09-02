#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#define PM_WALK_WALLJUMP_HEIGHT_DELTA 18.0f
#define PM_WALK_WALLJUMP_NEAR_DAMP 0.65f
#define PM_WALK_WALLJUMP_AIR_DAMP 0.5f
#define PM_WALK_OVERBOUNCE 1.001f
#define PM_WALK_STAND_ACCELERATE 9.0f
#define PM_WALK_CROUCH_ACCELERATE 12.0f
#define PM_WALK_PRONE_ACCELERATE 19.0f
#define PM_WALK_SLICK_ACCELERATE 1.0f
#define PM_WALK_LAND_ACCELERATE_SCALE 0.25f

enum {
    PM_WALK_WALLJUMP_NEAR_TIME = 1800,
    PM_WALK_WALLJUMP_AIR_TIME = 1200
};

/*
 * Complete grounded movement step. The two authoritative Windows bodies are
 * instruction-identical apart from relocated globals and callees:
 *
 *   uo_cgame_mp_x86.dll  0x300091e0..0x30009652
 *   uo_game_mp_x86.dll   0x20008f90..0x20009402
 *
 * Linux game retains the same state machine at RVA 0x00024b7c. Its compiler
 * calls PM_ClipVelocity where Windows inlines it. The only behavior-relevant
 * realization differences are the wish-vector operand order, the spill of the
 * normalized wish length, the binary64 glibc-sqrt boundary, and the final dot
 * product order. Those expressions alone select a platform realization.
 */
void PM_WalkMove(void)
{
    playerState_t *ps = pm->ps;

    if ((ps->playerStateFlags & PMF_WALLJUMP) != 0) {
        if (ps->pmTime > PM_WALK_WALLJUMP_NEAR_TIME) {
            ps->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP;
            ps->jumpOriginZ = 0.0f;
            for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
                ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(ps->velocity[lane]), x87f_load_f32(PM_WALK_WALLJUMP_NEAR_DAMP)));
#else
                ps->velocity[lane] = (float)((long double)ps->velocity[lane] * (long double)PM_WALK_WALLJUMP_NEAR_DAMP);
#endif
            }
        } else if (ps->pmTime == 0) {
#if EMULATE_X87
            const qboolean nearGround =
                x87f_lt_signaling(x87f_load_f32(ps->psOrigin[2]),
                                  x87f_add(x87f_load_f32(ps->jumpOriginZ), x87f_load_f32(PM_WALK_WALLJUMP_HEIGHT_DELTA)))
                    ? qtrue
                    : qfalse;
#else
            const qboolean nearGround =
                (long double)ps->psOrigin[2] < (long double)ps->jumpOriginZ + (long double)PM_WALK_WALLJUMP_HEIGHT_DELTA ? qtrue : qfalse;
#endif
            const float damping = nearGround != qfalse ? PM_WALK_WALLJUMP_NEAR_DAMP : PM_WALK_WALLJUMP_AIR_DAMP;
            ps->pmTime = nearGround != qfalse ? PM_WALK_WALLJUMP_NEAR_TIME : PM_WALK_WALLJUMP_AIR_TIME;
            for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
                ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(ps->velocity[lane]), x87f_load_f32(damping)));
#else
                ps->velocity[lane] = (float)((long double)ps->velocity[lane] * (long double)damping);
#endif
            }
        }
    }

    if (PM_CheckJump() != qfalse) {
        PM_AirMove();
        pm->ps->lastJumpCommandTime = pm->command.commandTime;
        return;
    }

    PM_Friction();

    const float forwardMove = (float)pm->command.forwardmove;
    const float rightMove = (float)pm->command.rightmove;
    usercmd_t command = pm->command;
    const float commandScale = PM_CmdScale_Walk(&command);

#if defined(WINDOWS_BEHAVIOR)
    /* MSVC inlines these two calls and folds Y/X/Z, whereas the retained
     * standalone Windows PM_ClipVelocity body folds Y/Z/X. Preserve the
     * caller's actual instruction graph rather than substituting the callee's
     * near-equivalent graph. It also reuses one normal.z*0 result across both
     * projections. */
    vec3_t *const bases[2] = {&pml.forward, &pml.right};
#if EMULATE_X87
    const x87f zeroNormalProduct = x87f_mul(x87f_load_f32(pml.groundTrace.normal[2]), x87f_load_f32(0.0f));
#else
    const long double zeroNormalProduct = (long double)pml.groundTrace.normal[2] * 0.0L;
#endif
    for (int32_t basisIndex = 0; basisIndex < 2; ++basisIndex) {
        float *const basis = *bases[basisIndex];
#if EMULATE_X87
        x87f backoff = x87f_add(x87f_add(x87f_mul(x87f_load_f32(pml.groundTrace.normal[1]), x87f_load_f32(basis[1])),
                                         x87f_mul(x87f_load_f32(pml.groundTrace.normal[0]), x87f_load_f32(basis[0]))),
                                zeroNormalProduct);
        const float overbounce = x87f_lt_signaling(backoff, x87f_load_f32(0.0f)) ? PM_WALK_OVERBOUNCE : 0.99900097f;
        backoff = x87f_mul(backoff, x87f_load_f32(overbounce));
        basis[0] = x87f_store_f32(x87f_sub(x87f_load_f32(basis[0]), x87f_mul(x87f_load_f32(pml.groundTrace.normal[0]), backoff)));
        basis[1] = x87f_store_f32(x87f_sub(x87f_load_f32(basis[1]), x87f_mul(x87f_load_f32(pml.groundTrace.normal[1]), backoff)));
        basis[2] = x87f_store_f32(x87f_neg(x87f_mul(x87f_load_f32(pml.groundTrace.normal[2]), backoff)));
#else
        long double backoff = ((long double)pml.groundTrace.normal[1] * (long double)basis[1] +
                               (long double)pml.groundTrace.normal[0] * (long double)basis[0]) +
                              zeroNormalProduct;
        const float overbounce = backoff < 0.0L ? PM_WALK_OVERBOUNCE : 0.99900097f;
        backoff *= (long double)overbounce;
        basis[0] = (float)((long double)basis[0] - (long double)pml.groundTrace.normal[0] * backoff);
        basis[1] = (float)((long double)basis[1] - (long double)pml.groundTrace.normal[1] * backoff);
        basis[2] = (float)(-((long double)pml.groundTrace.normal[2] * backoff));
#endif
    }
#else
    pml.forward[2] = 0.0f;
    pml.right[2] = 0.0f;
    PM_ClipVelocity(pml.forward, pml.groundTrace.normal, pml.forward, PM_WALK_OVERBOUNCE);
    PM_ClipVelocity(pml.right, pml.groundTrace.normal, pml.right, PM_WALK_OVERBOUNCE);
#endif
    (void)VectorNormalize(pml.forward);
    (void)VectorNormalize(pml.right);

    vec3_t wishVelocity;
    for (int32_t lane = 0; lane < 3; ++lane) {
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
        wishVelocity[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.right[lane]), x87f_load_f32(rightMove)),
                                                     x87f_mul(x87f_load_f32(pml.forward[lane]), x87f_load_f32(forwardMove))));
#else
        wishVelocity[lane] =
            (float)((long double)pml.right[lane] * (long double)rightMove + (long double)pml.forward[lane] * (long double)forwardMove);
#endif
#else
#if EMULATE_X87
        wishVelocity[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.forward[lane]), x87f_load_f32(forwardMove)),
                                                     x87f_mul(x87f_load_f32(pml.right[lane]), x87f_load_f32(rightMove))));
#else
        wishVelocity[lane] =
            (float)((long double)pml.forward[lane] * (long double)forwardMove + (long double)pml.right[lane] * (long double)rightMove);
#endif
#endif
    }

    vec3_t wishDirection = {wishVelocity[0], wishVelocity[1], wishVelocity[2]};

#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
    const float wishSpeed = x87f_store_f32(x87f_mul(x87f_load_f32((float)VectorNormalize(wishDirection)), x87f_load_f32(commandScale)));
#else
    const float wishSpeed = (float)(VectorNormalize(wishDirection) * (long double)commandScale);
#endif
#else
    const float normalizedWishLength = (float)VectorNormalize(wishDirection);
#if EMULATE_X87
    const float wishSpeed = x87f_store_f32(x87f_mul(x87f_load_f32(normalizedWishLength), x87f_load_f32(commandScale)));
#else
    const float wishSpeed = (float)((long double)normalizedWishLength * (long double)commandScale);
#endif
#endif

    float acceleration;
    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) != 0 || (pm->ps->playerStateFlags & PMF_NO_GROUNDFRICTION) != 0) {
        acceleration = PM_WALK_SLICK_ACCELERATE;
    } else {
        switch (PM_GetEffectiveStance(pm->ps)) {
        case EFFECTIVE_STANCE_PRONE:
            acceleration = PM_WALK_PRONE_ACCELERATE;
            break;
        case EFFECTIVE_STANCE_CROUCH:
            acceleration = PM_WALK_CROUCH_ACCELERATE;
            break;
        default:
            acceleration = PM_WALK_STAND_ACCELERATE;
            break;
        }
    }

    if ((pm->ps->playerStateFlags & PMF_LAND_STUN) != 0) {
#if EMULATE_X87
        acceleration = x87f_store_f32(x87f_mul(x87f_load_f32(acceleration), x87f_load_f32(PM_WALK_LAND_ACCELERATE_SCALE)));
#else
        acceleration = (float)((long double)acceleration * (long double)PM_WALK_LAND_ACCELERATE_SCALE);
#endif
    }

    PM_Accelerate(wishDirection, wishSpeed, acceleration);

    ps = pm->ps;
    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) != 0 || (ps->playerStateFlags & PMF_NO_GROUNDFRICTION) != 0) {
#if EMULATE_X87
        ps->velocity[2] =
            x87f_store_f32(x87f_sub(x87f_load_f32(ps->velocity[2]), x87f_mul(x87f_load_i32(ps->gravity), x87f_load_f32(pml.frametime))));
#else
        ps->velocity[2] = (float)((long double)ps->velocity[2] - (long double)ps->gravity * (long double)pml.frametime);
#endif
    }

    vec3_t originalVelocity = {ps->velocity[0], ps->velocity[1], ps->velocity[2]};
    float speed;
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
    speed = x87f_store_f32(x87f_sqrt(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                                       x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1]))),
                                              x87f_mul(x87f_load_f32(ps->velocity[2]), x87f_load_f32(ps->velocity[2])))));
#else
    speed = (float)coduo_x87_sqrtl(
        ((long double)ps->velocity[0] * (long double)ps->velocity[0] + (long double)ps->velocity[1] * (long double)ps->velocity[1]) +
        (long double)ps->velocity[2] * (long double)ps->velocity[2]);
#endif
#else
#if EMULATE_X87
    const double squaredSpeed = x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                                                 x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1]))),
                                                        x87f_mul(x87f_load_f32(ps->velocity[2]), x87f_load_f32(ps->velocity[2]))));
#else
    const long double squaredSpeed =
        ((long double)ps->velocity[0] * (long double)ps->velocity[0] + (long double)ps->velocity[1] * (long double)ps->velocity[1]) +
        (long double)ps->velocity[2] * (long double)ps->velocity[2];
#endif
    speed = (float)CoduoLibm_SqrtGlibc((double)squaredSpeed);
#endif

#if defined(WINDOWS_BEHAVIOR)
    /* This call is also inlined by MSVC and folds Z/Y/X. */
#if EMULATE_X87
    x87f velocityBackoff = x87f_add(x87f_add(x87f_mul(x87f_load_f32(pml.groundTrace.normal[2]), x87f_load_f32(ps->velocity[2])),
                                             x87f_mul(x87f_load_f32(pml.groundTrace.normal[1]), x87f_load_f32(ps->velocity[1]))),
                                    x87f_mul(x87f_load_f32(pml.groundTrace.normal[0]), x87f_load_f32(ps->velocity[0])));
    const float velocityOverbounce = x87f_lt_signaling(velocityBackoff, x87f_load_f32(0.0f)) ? PM_WALK_OVERBOUNCE : 0.99900097f;
    velocityBackoff = x87f_mul(velocityBackoff, x87f_load_f32(velocityOverbounce));
    ps->velocity[0] =
        x87f_store_f32(x87f_sub(x87f_load_f32(ps->velocity[0]), x87f_mul(x87f_load_f32(pml.groundTrace.normal[0]), velocityBackoff)));
    ps->velocity[1] =
        x87f_store_f32(x87f_sub(x87f_load_f32(ps->velocity[1]), x87f_mul(x87f_load_f32(pml.groundTrace.normal[1]), velocityBackoff)));
    ps->velocity[2] =
        x87f_store_f32(x87f_sub(x87f_load_f32(ps->velocity[2]), x87f_mul(x87f_load_f32(pml.groundTrace.normal[2]), velocityBackoff)));
#else
    long double velocityBackoff = ((long double)pml.groundTrace.normal[2] * (long double)ps->velocity[2] +
                                   (long double)pml.groundTrace.normal[1] * (long double)ps->velocity[1]) +
                                  (long double)pml.groundTrace.normal[0] * (long double)ps->velocity[0];
    const float velocityOverbounce = velocityBackoff < 0.0L ? PM_WALK_OVERBOUNCE : 0.99900097f;
    velocityBackoff *= (long double)velocityOverbounce;
    ps->velocity[0] = (float)((long double)ps->velocity[0] - (long double)pml.groundTrace.normal[0] * velocityBackoff);
    ps->velocity[1] = (float)((long double)ps->velocity[1] - (long double)pml.groundTrace.normal[1] * velocityBackoff);
    ps->velocity[2] = (float)((long double)ps->velocity[2] - (long double)pml.groundTrace.normal[2] * velocityBackoff);
#endif
#else
    PM_ClipVelocity(ps->velocity, pml.groundTrace.normal, ps->velocity, PM_WALK_OVERBOUNCE);
#endif

#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
    const x87f projection = x87f_add(x87f_add(x87f_mul(x87f_load_f32(originalVelocity[2]), x87f_load_f32(ps->velocity[2])),
                                              x87f_mul(x87f_load_f32(originalVelocity[1]), x87f_load_f32(ps->velocity[1]))),
                                     x87f_mul(x87f_load_f32(originalVelocity[0]), x87f_load_f32(ps->velocity[0])));
    const qboolean preserveSpeed = x87f_lt_signaling(x87f_load_f32(0.0f), projection) ? qtrue : qfalse;
#else
    const long double projection = ((long double)originalVelocity[2] * (long double)ps->velocity[2] +
                                    (long double)originalVelocity[1] * (long double)ps->velocity[1]) +
                                   (long double)originalVelocity[0] * (long double)ps->velocity[0];
    const qboolean preserveSpeed = projection > 0.0L ? qtrue : qfalse;
#endif
#else
#if EMULATE_X87
    const x87f projection = x87f_add(x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(originalVelocity[0])),
                                              x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(originalVelocity[1]))),
                                     x87f_mul(x87f_load_f32(ps->velocity[2]), x87f_load_f32(originalVelocity[2])));
    const qboolean preserveSpeed = x87f_lt_signaling(x87f_load_f32(0.0f), projection) ? qtrue : qfalse;
#else
    const long double projection = ((long double)ps->velocity[0] * (long double)originalVelocity[0] +
                                    (long double)ps->velocity[1] * (long double)originalVelocity[1]) +
                                   (long double)ps->velocity[2] * (long double)originalVelocity[2];
    const qboolean preserveSpeed = projection > 0.0L ? qtrue : qfalse;
#endif
#endif

    if (preserveSpeed != qfalse) {
        (void)VectorNormalize(ps->velocity);
        for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
            ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(speed), x87f_load_f32(ps->velocity[lane])));
#else
            ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(ps->velocity[lane]), x87f_load_f32(speed)));
#endif
#else
#if defined(WINDOWS_BEHAVIOR)
            ps->velocity[lane] = (float)((long double)speed * (long double)ps->velocity[lane]);
#else
            ps->velocity[lane] = (float)((long double)ps->velocity[lane] * (long double)speed);
#endif
#endif
        }
    }

    if (ps->velocity[0] != 0.0f || ps->velocity[1] != 0.0f) {
        PM_StepSlideMove(0);
    }
    PM_SetMovementDir();
}
