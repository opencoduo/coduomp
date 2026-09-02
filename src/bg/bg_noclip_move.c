// Sources: uo_cgame_mp_x86.dll 0x30009700..0x300098b0,
//          uo_game_mp_x86.dll  0x200094b0..0x20009660,
//          game.mp.uo.i386.so  0x0002538b..0x000256ec
//
// PM_NoclipMove — the pmove no-clip movement step: a free-fly move that ignores
// world collision entirely (the origin is integrated directly, with no trace or
// step-slide). It forces the standing view height, applies inline friction, builds
// a full-3D wish-velocity from the command forward/right/up movement bytes rotated
// through the pmove basis vectors, accelerates the player-state velocity toward it,
// and advances the origin by velocity*frametime.
//
// Name adjudication: the .mcode header's "PM_VerifyPronePosition" is a SIZE-ONLY
// guess (win 0x1b1 == some game_mp function) and is REJECTED per the naming rules
// (pmove-prone verification is a trace-heavy server routine; this body never calls
// a trace and never clips). The identity is proven from behaviour and the call
// graph, all from the machine code:
//   * Dispatched by PmoveSingle (0x3000e050) at 0x3000e464 as one pmType-specific
//     movement handler, the sibling of the ground mover at 0x300098c0.
//   * It is the near-twin of the already-reconstructed PM_FlyMove
//     (0x30008f20, functions/FUN_30008f20_30009051.c): the same friction ->
//     PM_CmdScale -> wishvel(pml.forward/pml.right + upmove) -> VectorNormalize ->
//     PM_Accelerate acceleration path. It differs from the spectator move in exactly
//     the ways that distinguish Quake3/CoD PM_NoclipMove from PM_FlyMove:
//       - it forces ps->viewheight to the standing height (bg_viewheight_standing.integer)
//         at entry (no crouch/prone stance in noclip);
//       - the command scale is applied to wishspeed AFTER VectorNormalize
//         (wishspeed *= scale), not to the wishvel before it;
//       - the acceleration constant is 9.0f (0x41100000), not the spectator 8.0f;
//       - there are no vertical free-fly button impulses;
//       - the move ENDS by integrating the origin directly, origin += velocity *
//         pml.frametime (VectorMA), with NO PM_StepSlideMove / trace / collision.
//     That "friction, wishvel with a direct vertical, accelerate, then advance the
//     origin with no collision" shape is the canonical PM_NoclipMove.
//
// Globals / constants (all proven from the bytes):
//   * pm (0x30539850): the pmove context; ps = pm->ps.
//   * bg_viewheight_standing.integer (cgame 0x30539aac; game 0x203581cc):
//     standing view-height int, written to
//     ps->viewHeightTarget (+0xf4) and ps->standViewHeight
//     (+0x57c) at entry.
//   * pml.forward (0x30539580), pml.right (0x3053958c): pmove-locals basis vectors.
//   * pml.frametime (0x305395a4): pmove substep time (seconds).
//   * 0x300715ec = 100.0f  (friction stop-speed floor for the control speed).
//   * 0x3007bf7c = 8.25f   (friction coefficient).
//   * 0x3007bce0 = 1.0f    (min-speed cutoff: below it the velocity is zeroed).
//   * 0x3007bcec = 0.0f    (friction newspeed clamp; the 9.0f accel arg = 0x41100000).
//
// ABI: void(void). The lone PUSH ESI/PUSH EDI are register preservation; EDI caches
// pm, ESI the wishdir stack vec3. Callee (PM_Accelerate) stack args are
// cleaned by this frame (ADD ESP,8); expressed here as plain C.

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
/* These exported Linux BG constants are shared by several movement modes and
 * therefore remain owned by the game movement unit rather than this one
 * extracted function. */
extern const float pm_stopspeed;
extern const float pm_friction;
extern const float pm_accelerate;
#endif

#if defined(WINDOWS_BEHAVIOR)
void PM_NoclipMove(void)
{
    int32_t targetViewHeight = bg_viewheight_standing.integer;
    pmove_t *move = pm;
    playerState_t *ps = move->ps;

    /* 0x30009700..0x30009724: force the standing view height.
     * bg_viewheight_standing.integer is stored into ps->viewHeightTarget
     * (+0xf4) and ps->standViewHeight (+0x57c). Noclip has no crouch/prone
     * stance. */
    ps->viewHeightTarget = targetViewHeight;
    int32_t standingViewHeight = bg_viewheight_standing.integer;
    playerState_t *standingPs = move->ps;
    standingPs->standViewHeight = standingViewHeight;

    /* 0x30009726..0x300097bf: inline friction (classic Quake3 PM_Friction, folded
     * into this mover rather than the shared PM_Friction 0x30008470).
     *   speed = |velocity| (full 3-D). */
    /* speed is never stored to a float slot: the FSQRT result (0x30009741) is kept
     * in st0 through the FCOM 1.0f, the control-select, the FSUBR ST0,ST1 and the
     * FDIVRP (0x300097a3, speed is the divisor). The carrier therefore retains
     * the live Windows PC=53 x87 value through the friction calculation. */
    playerState_t *frictionPs = move->ps; /* 0x30009726 independent reload. */
#if EMULATE_X87
    x87f speed = x87f_sqrt(x87f_add(x87f_add(x87f_mul(x87f_load_f32(frictionPs->velocity[0]), x87f_load_f32(frictionPs->velocity[0])),
                                             x87f_mul(x87f_load_f32(frictionPs->velocity[1]), x87f_load_f32(frictionPs->velocity[1]))),
                                    x87f_mul(x87f_load_f32(frictionPs->velocity[2]), x87f_load_f32(frictionPs->velocity[2]))));
#else
    long double speedSquared = (long double)frictionPs->velocity[0] * frictionPs->velocity[0];
    speedSquared += (long double)frictionPs->velocity[1] * frictionPs->velocity[1];
    speedSquared += (long double)frictionPs->velocity[2] * frictionPs->velocity[2];
    long double speed = coduo_x87_sqrtl(speedSquared);
#endif

#if EMULATE_X87
    if (x87f_lt(speed, x87f_load_f32(1.0f))) {
#else
    if (speed < 1.0f) {
#endif
        /* 0x30009756..0x30009767: below the cutoff, kill the velocity entirely. */
        frictionPs->velocity[0] = 0.0f;
        playerState_t *velocityYPs = move->ps;
        velocityYPs->velocity[1] = 0.0f;
        playerState_t *velocityZPs = move->ps;
        velocityZPs->velocity[2] = 0.0f;
    } else {
        /* 0x30009769..0x300097bf: control = max(speed, 100.0f) (stop-speed floor);
         * drop = control * pml.frametime * 8.25f; newspeed = max(speed - drop, 0);
         * velocity *= newspeed / speed. The x87 selects `speed` when speed >= 100.0f,
         * else the 100.0f constant. */
        /* control/newspeed/frictionScale stay in the x87 stack (control via FLD ST0,
         * the friction product * frametime * 8.25f, the FSUBR, and the FDIVRP) with
         * no float store until each velocity component is written -- long double. */
#if EMULATE_X87
        x87f control = x87f_lt(speed, x87f_load_f32(100.0f)) ? x87f_load_f32(100.0f) : speed;
        x87f newspeed = x87f_sub(speed, x87f_mul(x87f_mul(control, x87f_load_f32(pml.frametime)), x87f_load_f32(8.25f)));
        if (x87f_lt(newspeed, x87f_load_f32(0.0f))) {
            newspeed = x87f_load_f32(0.0f);
        }
        const x87f frictionScale = x87f_div(newspeed, speed);
        frictionPs->velocity[0] = x87f_store_f32(x87f_mul(frictionScale, x87f_load_f32(frictionPs->velocity[0])));
        playerState_t *velocityYPs = move->ps;
        velocityYPs->velocity[1] = x87f_store_f32(x87f_mul(frictionScale, x87f_load_f32(velocityYPs->velocity[1])));
        playerState_t *velocityZPs = move->ps;
        velocityZPs->velocity[2] = x87f_store_f32(x87f_mul(frictionScale, x87f_load_f32(velocityZPs->velocity[2])));
#else
        long double control = (speed < 100.0L) ? 100.0L : speed;
        long double newspeed = speed - control * (long double)pml.frametime * 8.25L;
        if (newspeed < 0.0f) {
            newspeed = 0.0f;
        }
        long double frictionScale = newspeed / speed;
        frictionPs->velocity[0] = (float)(frictionScale * frictionPs->velocity[0]);
        playerState_t *velocityYPs = move->ps;
        velocityYPs->velocity[1] = (float)(frictionScale * velocityYPs->velocity[1]);
        playerState_t *velocityZPs = move->ps;
        velocityZPs->velocity[2] = (float)(frictionScale * velocityZPs->velocity[2]);
#endif
    }

    /* 0x300097bf..0x300097c7: scale = PM_CmdScale(&move->command). The current usercmd
     * is embedded at move+0x04 (LEA ECX,[EDI+4]); reached through pmove_t's
     * flattened fields as &move->command, matching the sibling
     * movers. */
    usercmd_t *cmd = &move->command;
    float scale = PM_CmdScale(cmd);

    /* 0x300097cb..0x30009839: sign-extend the three command movement bytes and
     * convert to float (forwardmove = forwardmove @ +0x18, rightmove = s8[1] @
     * +0x19, upmove = s8[2] @ +0x1a) and build the wish-velocity in the pmove basis:
     *   wishvel[i] = pml.forward[i]*forwardmove + pml.right[i]*rightmove,
     *   wishvel[2] additionally += upmove (direct vertical, FILD'd from the int). */
    float fmove = (float)cmd->forwardmove; /* forwardmove */
    float smove = (float)cmd->rightmove; /* rightmove   */
    int32_t umove = cmd->upmove;      /* upmove (widened to int by FILD) */

    vec3_t wishdir;
#if EMULATE_X87
    const x87f wishX = x87f_add(x87f_mul(x87f_load_f32(pml.right[0]), x87f_load_f32(smove)),
                                x87f_mul(x87f_load_f32(pml.forward[0]), x87f_load_f32(fmove)));
    const x87f wishY = x87f_add(x87f_mul(x87f_load_f32(pml.right[1]), x87f_load_f32(smove)),
                                x87f_mul(x87f_load_f32(pml.forward[1]), x87f_load_f32(fmove)));
    const x87f wishZBasis = x87f_add(x87f_mul(x87f_load_f32(pml.right[2]), x87f_load_f32(smove)),
                                     x87f_mul(x87f_load_f32(pml.forward[2]), x87f_load_f32(fmove)));
    const x87f wishZ = x87f_add(x87f_load_i32(umove), wishZBasis);
    wishdir[2] = x87f_store_f32(wishZ); /* 0x3000983f stores z first. */
    wishdir[0] = x87f_store_f32(wishX);
    wishdir[1] = x87f_store_f32(wishY);
#else
    long double wishX = (long double)pml.right[0] * smove;
    wishX += (long double)pml.forward[0] * fmove;
    long double wishY = (long double)pml.right[1] * smove;
    wishY += (long double)pml.forward[1] * fmove;
    long double wishZBasis = (long double)pml.right[2] * smove;
    wishZBasis += (long double)pml.forward[2] * fmove;
    long double wishZ = (long double)umove + wishZBasis;
    wishdir[2] = (float)wishZ; /* 0x3000983f stores z first. */
    wishdir[0] = (float)wishX;
    wishdir[1] = (float)wishY;
#endif

    /* 0x30009857..0x3000985c: wishspeed = |wishdir| (VectorNormalize normalizes in
     * place and returns the pre-normalization length), then the command scale is
     * applied to the SPEED (wishspeed *= scale) -- the noclip ordering, distinct from
     * the spectator move which scales the wishvel before normalizing. */
    const float normalizedWishspeed = VectorNormalize(wishdir);
#if EMULATE_X87
    float wishspeed = x87f_store_f32(x87f_mul(x87f_load_f32(normalizedWishspeed), x87f_load_f32(scale)));
#else
    float wishspeed = (float)((long double)normalizedWishspeed * (long double)scale);
#endif

    /* 0x30009860..0x30009870: accelerate the player velocity toward wishdir with the
     * noclip acceleration of 9.0f (the pushed 0x41100000). */
    PM_Accelerate(wishdir, wishspeed, 9.0f);

    /* 0x30009875..0x300098b0: advance the origin with NO collision:
     * origin[i] += velocity[i] * pml.frametime (VectorMA). This direct integration,
     * with no PM_StepSlideMove/trace, is what makes this the no-clip move. */
#if EMULATE_X87
    for (int32_t lane = 0; lane < 3; ++lane) {
        playerState_t *originPs = move->ps;
        originPs->psOrigin[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(originPs->velocity[lane])),
                                                           x87f_load_f32(originPs->psOrigin[lane])));
    }
#else
    long double frameTimeX = pml.frametime; /* 0x30009875 load precedes ps reload. */
    playerState_t *originXPs = move->ps;
    originXPs->psOrigin[0] = (float)(frameTimeX * originXPs->velocity[0] + originXPs->psOrigin[0]);
    playerState_t *originYPs = move->ps;
    long double frameTimeY = pml.frametime;
    originYPs->psOrigin[1] = (float)(frameTimeY * originYPs->velocity[1] + originYPs->psOrigin[1]);
    playerState_t *originZPs = move->ps;
    long double frameTimeZ = pml.frametime;
    originZPs->psOrigin[2] = (float)(frameTimeZ * originZPs->velocity[2] + originZPs->psOrigin[2]);
#endif
}
#else
void PM_NoclipMove(void)
{
    vec3_t wishvel;
    vec3_t wishdir;
    float speed;
    float commandScale;
    float friction;
    float control;
    float newSpeed;
    float wishSpeed;

    pm->ps->viewHeightTarget = bg_viewheight_standing.integer;
    pm->ps->standViewHeight = bg_viewheight_standing.integer;

    /* Linux forms the X/Y/Z sum at PC=64, narrows it to the binary64 glibc
     * sqrt argument, then stores the returned root as binary32. */
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

    if (speed < 1.0f) {
        pm->ps->velocity[0] = 0.0f;
        pm->ps->velocity[1] = 0.0f;
        pm->ps->velocity[2] = 0.0f;
    } else {
        /* The multiply by 1.5, selected control value, accumulated drop,
         * subtraction, and division are each stored as binary32 in the Linux
         * body before the next stage consumes them. */
#if EMULATE_X87
        friction = x87f_store_f32(x87f_mul(x87f_load_f32(pm_friction), x87f_load_f32(1.5f)));
#else
        friction = (float)((long double)pm_friction * 1.5L);
#endif
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
#if EMULATE_X87
        const float drop = x87f_store_f32(x87f_add(
            x87f_mul(x87f_mul(x87f_load_f32(control), x87f_load_f32(friction)), x87f_load_f32(pml.frametime)), x87f_load_f32(0.0f)));
        newSpeed = x87f_store_f32(x87f_sub(x87f_load_f32(speed), x87f_load_f32(drop)));
#else
        const float drop = (float)((long double)control * (long double)friction * (long double)pml.frametime + 0.0L);
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

    commandScale = PM_CmdScale(&pm->command);

    const float forwardMove = (float)pm->command.forwardmove;
    const float rightMove = (float)pm->command.rightmove;
    for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        wishvel[lane] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(pml.forward[lane]), x87f_load_f32(forwardMove)),
                                                x87f_mul(x87f_load_f32(pml.right[lane]), x87f_load_f32(rightMove))));
#else
        wishvel[lane] =
            (float)((long double)pml.forward[lane] * (long double)forwardMove + (long double)pml.right[lane] * (long double)rightMove);
#endif
    }
#if EMULATE_X87
    wishvel[2] = x87f_store_f32(x87f_add(x87f_load_f32(wishvel[2]), x87f_load_i32(pm->command.upmove)));
#else
    wishvel[2] = (float)((long double)wishvel[2] + (long double)pm->command.upmove);
#endif

    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = wishvel[2];
    wishSpeed = VectorNormalize(wishdir);
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
