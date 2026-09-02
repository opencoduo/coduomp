#include "bg_pmove.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "qcommon/client_info_types.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select exactly one PM_Footsteps behavior mode"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select exactly one PM_Footsteps behavior mode"
#endif

// Source: uo_cgame_mp_x86.dll 0x3000bba0..0x3000c103
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000bba0_3000c103.mcode
//
// PM_Footsteps — update the current pmove player's horizontal speed, locomotion
// animation, bob cycle, and footstep event. The mechanical Window_Paint label is
// rejected: the body exclusively reads pm/player-state movement
// fields and calls BG_ExecuteCommand, BG_UpdateConditionValue,
// PM_ShouldMakeFootsteps, and PM_FootstepEvent. The same-module PPC name bank carries
// PM_Footsteps in this exact movement cluster.
//
// ABI: no arguments; all state is reached through pm. Plain RET.
//
// The Windows game body at 0x2000b960 is instruction-identical to the cgame
// body above apart from relocations. Linux retains the same state transitions
// at RVA 0x0006912e, but its unoptimized build stores the intermediate speed,
// lerp, and bob-rate values as binary32 where Windows keeps several values live
// in x87 registers. The whole-function behavior split below preserves that
// genuine computational realization without mixing platform gates through the
// individual expressions.

enum {
    PM_FOOTSTEPS_ANIM_CATEGORY = 3,
    PM_FOOTSTEPS_ANIM_IDLE = 1,
    PM_FOOTSTEPS_ANIM_IDLE_CROUCH = 2,
    PM_FOOTSTEPS_ANIM_IDLE_PRONE = 3,
    PM_FOOTSTEPS_ANIM_STAND_A = 4,
    PM_FOOTSTEPS_ANIM_STAND_B = 5,
    PM_FOOTSTEPS_ANIM_CROUCH_A = 6,
    PM_FOOTSTEPS_ANIM_CROUCH_B = 7,
    PM_FOOTSTEPS_ANIM_PRONE_A = 8,
    PM_FOOTSTEPS_ANIM_PRONE_B = 9,
    PM_FOOTSTEPS_ANIM_STAND_C = 10,
    PM_FOOTSTEPS_ANIM_STAND_D = 11,
    PM_FOOTSTEPS_ANIM_CROUCH_C = 12,
    PM_FOOTSTEPS_ANIM_CROUCH_D = 13,
    PM_FOOTSTEPS_ANIM_AIRBORNE_UP = 16,
    PM_FOOTSTEPS_ANIM_AIRBORNE_DOWN = 17,
    PM_FOOTSTEPS_CONDITION_DIRECTION = ANIM_COND_STRAFING,
    PM_FOOTSTEPS_AIRBORNE_DELAY = 300,
    PM_FOOTSTEPS_STANCE_FLAG_MASK = 3
};

#define PM_FOOTSTEPS_BOB_MASK ((uint32_t)0x000000ffu)
#define PM_FOOTSTEPS_SPECIAL_DISABLE_MASK ((uint32_t)0x00006000u)

/* Exact source flag names are unresolved. Their roles are proven here: bit 0x10
 * enables the delayed airborne footstep update and bit 0x40 selects the alternate
 * locomotion-command family. */
#define PM_FOOTSTEPS_FLAG_AIRBORNE_UPDATE ((uint32_t)0x00000010u)
#define PM_FOOTSTEPS_FLAG_ALT_COMMAND ((uint32_t)0x00000040u)

/* The +0x84 mask suppresses ordinary locomotion commands for special/vehicle
 * states. It is the same machine-proven mask used by weapon-position bob code. */
#define PM_FOOTSTEPS_DISABLE_MASK EF_RESTRICTED_MASK
#define PM_FOOTSTEPS_AIRBORNE_SPEED_UNIT 95.25f
#define PM_FOOTSTEPS_AIRBORNE_RUN_BOB_SCALE 0.45f
#define PM_FOOTSTEPS_AIRBORNE_WALK_BOB_SCALE 0.35f
#define PM_FOOTSTEPS_MOVEMENT_THRESHOLD 10.0f
#define PM_FOOTSTEPS_BOB_RESET_THRESHOLD 1.0f
#define PM_FOOTSTEPS_NO_INPUT_SPEED_LIMIT 120.0f
#define PM_FOOTSTEPS_PRONE_WALK_BOB_SCALE 0.24f
#define PM_FOOTSTEPS_PRONE_RUN_BOB_SCALE 0.25f
#define PM_FOOTSTEPS_CROUCH_WALK_BOB_SCALE 0.315f
#define PM_FOOTSTEPS_CROUCH_RUN_BOB_SCALE 0.34f
#define PM_FOOTSTEPS_STAND_ALT_WALK_BOB_SCALE 0.325f
#define PM_FOOTSTEPS_STAND_ALT_RUN_BOB_SCALE 0.36f
#define PM_FOOTSTEPS_STAND_WALK_BOB_SCALE 0.305f
#define PM_FOOTSTEPS_STAND_RUN_BOB_SCALE 0.335f

#if defined(LINUX_BEHAVIOR)
#if EMULATE_X87
#define PM_FOOTSTEPS_STEP_INTERVAL(num, den, scale) \
    x87f_store_f32(x87f_mul(x87f_div(x87f_load_f32(num), x87f_load_f32(den)), x87f_load_f32(scale)))
#define PM_FOOTSTEPS_BOB_CYCLE(oldTime, msec, step) \
    (x87f_store_i32_trunc(x87f_add(x87f_load_i32(oldTime), x87f_mul(x87f_load_i32(msec), x87f_load_f32(step)))) & 255)
#else
#define PM_FOOTSTEPS_STEP_INTERVAL(num, den, scale) (((num) / (den)) * (scale))
#define PM_FOOTSTEPS_BOB_CYCLE(oldTime, msec, step) \
    (coduo_fp_to_i32_extended((long double)(oldTime) + (long double)(msec) * (long double)(step)) & 255)
#endif
#endif

#if defined(WINDOWS_BEHAVIOR)
void PM_Footsteps(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    uint32_t flags;
    effectiveStance_t stance;
    int32_t oldBobCycle;
    int32_t animResult;
    int32_t animChanged;
    int32_t animCommand;
    int32_t direction;
    float horizontalSpeed;
    long double horizontalSpeedX87;
    /* airborneBobRate is kept in st0 across the BG_ExecuteCommand call
     * (0x3000bd00) and consumed by the bobCycle FMUL (0x3000bd16) without ever
     * being stored -- long double so a `float` local does not round it. */
    long double airborneBobRate;
    long double movementAmountX87;
    float movementAmount;
    long double lerp;   /* PM_GetViewHeightLerp returns raw st(0); the stance
                         * blend below (FSUB/FMUL) runs on the unrounded value */
    long double stanceScale;
    /* 0x3000beb4..0x3000bfd8: the quotient and stance-specific multiplier
     * remain live on the x87 stack across both animation calls. */
    long double bobRate;

    if (ps->pmType >= PM_TYPE_DEAD) {
        return;
    }

    /* 0x3000bbbc..0x3000bbce: both m32 velocity components are squared and
     * summed in x87 extended precision; only the final FSQRT is stored m32. */
    horizontalSpeedX87 =
        (long double)ps->velocity[0] * (long double)ps->velocity[0] + (long double)ps->velocity[1] * (long double)ps->velocity[1];
    horizontalSpeed = (float)__builtin_sqrtl(horizontalSpeedX87);
    move->horizontalSpeed = horizontalSpeed;

    if ((ps->entityStateFlags & PM_FOOTSTEPS_DISABLE_MASK) != 0) {
        if ((ps->entityStateFlags & PM_FOOTSTEPS_SPECIAL_DISABLE_MASK) != 0) {
            if (ps->viewHeightTarget == ps->proneViewHeight) {
                if (BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE_PRONE, qtrue) >= 0) {
                    return;
                }
                move = pm;
                ps = move->ps;
            } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
                if (BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE_CROUCH, qtrue) >= 0) {
                    return;
                }
                move = pm;
                ps = move->ps;
            }
        }
        (void)BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE, qtrue);
        return;
    }

    if (ps->viewHeightTarget == ps->crouchViewHeight) {
        stance = EFFECTIVE_STANCE_CROUCH;
    } else if (ps->viewHeightTarget == ps->proneViewHeight) {
        stance = EFFECTIVE_STANCE_PRONE;
    } else {
        stance = EFFECTIVE_STANCE_STAND;
    }

    flags = ps->playerStateFlags;
    if (ps->groundEntityNum == ENTITYNUM_NONE && ps->pmType != PM_TYPE_LINKED) {
        if ((flags & PM_FOOTSTEPS_FLAG_AIRBORNE_UPDATE) == 0 ||
            coduo_int32_from_bits((uint32_t)move->command.commandTime - (uint32_t)ps->lastJumpCommandTime) < PM_FOOTSTEPS_AIRBORNE_DELAY) {
            return;
        }

        /* 0x3000bca4..0x3000bd40: airborne bob uses vertical velocity, chooses
         * an up/down locomotion command, advances bobCycle, and forces the
         * footstep-event phase check. */
        if ((flags & PMF_WALKING) == 0 && ps->leanFraction == 0.0f) {
            airborneBobRate =
                ps->velocity[2] / (ps->runSpeedScale * PM_FOOTSTEPS_AIRBORNE_SPEED_UNIT) * PM_FOOTSTEPS_AIRBORNE_RUN_BOB_SCALE;
        } else {
            airborneBobRate =
                ps->velocity[2] / (ps->walkSpeedScale * PM_FOOTSTEPS_AIRBORNE_SPEED_UNIT) * PM_FOOTSTEPS_AIRBORNE_WALK_BOB_SCALE;
        }
        (void)BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY,
                                !(ps->velocity[2] >= 0.0f) ? PM_FOOTSTEPS_ANIM_AIRBORNE_DOWN : PM_FOOTSTEPS_ANIM_AIRBORNE_UP, qtrue);
        move = pm;
        ps = move->ps;
        oldBobCycle = ps->bobCycle;
        /* pml.msec and oldBobCycle enter via bare FILD/FIADD (0x3000bd05/0x3000bd1f)
         * -- no (float) casts. */
        ps->bobCycle = coduo_fp_to_i32_extended(pml.msec * airborneBobRate + oldBobCycle) & PM_FOOTSTEPS_BOB_MASK;
        PM_FootstepEvent(oldBobCycle, ps->bobCycle, qtrue);

        move = pm;
        ps = move->ps;
        horizontalSpeed = move->horizontalSpeed;

        /* 0x3000bd4b..0x3000bd59: if the current low two movement flags already
         * encode this stance, the airborne update is complete. Otherwise the
         * function deliberately falls through to the ordinary locomotion path. */
        if ((uint32_t)stance == (ps->playerStateFlags & (uint32_t)PM_FOOTSTEPS_STANCE_FLAG_MASK)) {
            return;
        }
        flags = ps->playerStateFlags;
    }

    ps = move->ps;
    horizontalSpeed = move->horizontalSpeed;
    flags = ps->playerStateFlags;

    /* 0x3000bd61..0x3000bd94 enters the idle path only for ordered speed < 10;
     * unordered input and exactly 10 both continue through locomotion. */
    if (horizontalSpeed < PM_FOOTSTEPS_MOVEMENT_THRESHOLD || ps->pmType == PM_TYPE_LINKED) {
        if (horizontalSpeed < PM_FOOTSTEPS_BOB_RESET_THRESHOLD) {
            ps->bobCycle = 0;
        }

        animResult = -1;
        if (ps->viewHeightTarget == ps->proneViewHeight) {
            animResult = BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE_PRONE, qtrue);
            move = pm;
            ps = move->ps;
        } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
            animResult = BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE_CROUCH, qtrue);
            move = pm;
            ps = move->ps;
        }
        if (animResult < 0) {
            (void)BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE, qtrue);
        }
        return;
    }

    /* 0x3000bd9d..0x3000be3d: derive the input-direction movement amount and
     * publish the direction as animation condition 10 when an input axis exists. */
    movementAmountX87 = (long double)ps->speed;
    direction = 0;
    if (move->command.forwardmove != 0 && move->command.rightmove != 0) {
        movementAmountX87 *= (((long double)ps->strafeSpeedScale - 1.0L) * 0.75L + 1.0L + 1.0L) * 0.5L;
        if (move->command.forwardmove < 0) {
            movementAmountX87 *= ((long double)ps->backSpeedScale + 1.0L) * 0.5L;
        }
    } else if (move->command.forwardmove != 0) {
        if (move->command.forwardmove < 0) {
            movementAmountX87 *= (long double)ps->backSpeedScale;
        }
    } else if (move->command.rightmove != 0) {
        movementAmountX87 *= ((long double)ps->strafeSpeedScale - 1.0L) * 0.75L + 1.0L;
        direction = (move->command.rightmove > 0) ? 2 : 1;
    }

    if (move->command.forwardmove != 0 || move->command.rightmove != 0) {
        BG_UpdateConditionValue(ps->psClientNum, PM_FOOTSTEPS_CONDITION_DIRECTION, direction, qtrue);
    }

    /* The pmove pointer is retained across BG_UpdateConditionValue, but the
     * machine code reloads its player-state pointer before reading the scales. */
    ps = move->ps;
    if ((flags & PMF_WALKING) != 0) {
        movementAmountX87 *= (long double)ps->walkSpeedScale;
    } else if ((flags & PMF_SPRINTING) != 0) {
        movementAmountX87 *= (long double)ps->sprintSpeedScale;
    } else {
        movementAmountX87 *= (long double)ps->runSpeedScale;
    }
    /* 0x3000be67 is the sole m32 spill of the movement-amount chain. */
    movementAmount = (float)movementAmountX87;
    /* The two directed viewheight lerps use opposite endpoint order. A zero
     * result from both falls back to the current stance's fixed scale. */
    lerp = PM_GetViewHeightLerp(ps->proneViewHeight, ps->crouchViewHeight);
    if (lerp != 0.0f) {
        stanceScale = (1.0L - lerp) * (long double)ps->crouchSpeedScale + lerp * (long double)ps->proneSpeedScale;
    } else {
        lerp = PM_GetViewHeightLerp(ps->crouchViewHeight, ps->proneViewHeight);
        if (lerp != 0.0f) {
            stanceScale = (1.0L - lerp) * (long double)ps->proneSpeedScale + lerp * (long double)ps->crouchSpeedScale;
        } else if (stance == EFFECTIVE_STANCE_PRONE) {
            stanceScale = (long double)ps->proneSpeedScale;
        } else if (stance == EFFECTIVE_STANCE_CROUCH) {
            stanceScale = (long double)ps->crouchSpeedScale;
        } else {
            stanceScale = 1.0L;
        }
    }

    bobRate = (long double)horizontalSpeed / ((long double)movementAmount * stanceScale);
    if (stance == EFFECTIVE_STANCE_PRONE) {
        if ((flags & PMF_WALKING) != 0) {
            bobRate *= PM_FOOTSTEPS_PRONE_WALK_BOB_SCALE;
        } else {
            bobRate *= PM_FOOTSTEPS_PRONE_RUN_BOB_SCALE;
        }
        animCommand = (flags & PM_FOOTSTEPS_FLAG_ALT_COMMAND) ? PM_FOOTSTEPS_ANIM_PRONE_B : PM_FOOTSTEPS_ANIM_PRONE_A;
    } else if (stance == EFFECTIVE_STANCE_CROUCH) {
        if ((flags & PMF_WALKING) != 0) {
            bobRate *= PM_FOOTSTEPS_CROUCH_WALK_BOB_SCALE;
        } else {
            bobRate *= PM_FOOTSTEPS_CROUCH_RUN_BOB_SCALE;
        }
        if ((flags & PM_FOOTSTEPS_FLAG_ALT_COMMAND) != 0) {
            animCommand = (flags & PMF_WALKING) ? PM_FOOTSTEPS_ANIM_CROUCH_B : PM_FOOTSTEPS_ANIM_CROUCH_D;
        } else {
            animCommand = (flags & PMF_WALKING) ? PM_FOOTSTEPS_ANIM_CROUCH_A : PM_FOOTSTEPS_ANIM_CROUCH_C;
        }
    } else {
        if ((flags & PM_FOOTSTEPS_FLAG_ALT_COMMAND) != 0) {
            if ((flags & PMF_WALKING) != 0) {
                bobRate *= PM_FOOTSTEPS_STAND_ALT_WALK_BOB_SCALE;
                animCommand = PM_FOOTSTEPS_ANIM_STAND_B;
            } else {
                bobRate *= PM_FOOTSTEPS_STAND_ALT_RUN_BOB_SCALE;
                animCommand = PM_FOOTSTEPS_ANIM_STAND_D;
            }
        } else if ((flags & PMF_WALKING) != 0) {
            bobRate *= PM_FOOTSTEPS_STAND_WALK_BOB_SCALE;
            animCommand = PM_FOOTSTEPS_ANIM_STAND_A;
        } else {
            bobRate *= PM_FOOTSTEPS_STAND_RUN_BOB_SCALE;
            animCommand = PM_FOOTSTEPS_ANIM_STAND_C;
        }
    }

    animResult = BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, animCommand, qtrue);
    animChanged = PM_ShouldMakeFootsteps();

    move = pm;
    ps = move->ps;
    oldBobCycle = ps->bobCycle;
    /* pml.msec and oldBobCycle enter via bare FILD/FIADD (0x3000bfd8/0x3000bff1)
     * -- no (float) casts. (bobRate itself is a register-carried cluster; see the
     * file note.) */
    ps->bobCycle = coduo_fp_to_i32_extended(pml.msec * bobRate + oldBobCycle) & PM_FOOTSTEPS_BOB_MASK;

    if (move->command.forwardmove == 0 && move->command.rightmove == 0) {
        if (horizontalSpeed > PM_FOOTSTEPS_NO_INPUT_SPEED_LIMIT) {
            return;
        }
        if (ps->viewHeightTarget == ps->proneViewHeight) {
            animResult = BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE_PRONE, qtrue);
            move = pm;
            ps = move->ps;
        } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
            animResult = BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE_CROUCH, qtrue);
            move = pm;
            ps = move->ps;
        }
    }
    if (animResult < 0) {
        (void)BG_ExecuteCommand(ps, PM_FOOTSTEPS_ANIM_CATEGORY, PM_FOOTSTEPS_ANIM_IDLE, qtrue);
        move = pm;
        ps = move->ps;
    }

    PM_FootstepEvent(oldBobCycle, ps->bobCycle, animChanged);
}
#else
/* ------------------------------------------------------------------ */
/*  0x297e4  PM_Footsteps                                        */
/*  Update footstep timing and animations.                             */
/* ------------------------------------------------------------------ */
/* VERIFIED_MACHINE_CODE(0x297e4): pmType guard, horizontal speed store, entityStateFlags animation branch, airborne ladder timing/events, idle animation paths, strafe/back speed conditioning, stance/viewheight speed scaling, animation IDs, truncating bob-cycle conversions, idle fallback, and PM_FootstepEvent gates checked. */
void PM_Footsteps(void)
{
    float speed2d;
    int anim;
    int effectiveStance;
    uint32_t stanceBits;
    uint32_t sprintBit;
    float moveSpeed;
    float stepInterval;
    int oldTime;
    int shouldMake;

    anim = -1;

    if (pm->ps->pmType >= PM_TYPE_DEAD) {
        return;
    }

#if EMULATE_X87
    speed2d =
        (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(x87f_mul(x87f_load_f32(pm->ps->velocity[0]), x87f_load_f32(pm->ps->velocity[0])),
                                                      x87f_mul(x87f_load_f32(pm->ps->velocity[1]), x87f_load_f32(pm->ps->velocity[1])))));
#else
    speed2d = (float)CoduoLibm_Sqrt((double)(pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1]));
#endif
    pm->horizontalSpeed = speed2d;

    if ((pm->ps->entityStateFlags & EF_RESTRICTED_MASK) != 0) {
        /* Vehicle / special state */
        if ((pm->ps->entityStateFlags & 0x6000) != 0) {
            if (pm->ps->viewHeightTarget == pm->ps->proneViewHeight) {
                anim = BG_ExecuteCommand(pm->ps, 3, 3, 1);
            } else if (pm->ps->viewHeightTarget == pm->ps->crouchViewHeight) {
                anim = BG_ExecuteCommand(pm->ps, 3, 2, 1);
            }
        }
        if (anim < 0) {
            BG_ExecuteCommand(pm->ps, 3, 1, 1);
        }
        return;
    }

    effectiveStance = PM_GetEffectiveStance(pm->ps);

    /* Prone crawling footstep */
    if (pm->ps->groundEntityNum == ENTITYNUM_NONE && pm->ps->pmType != PM_TYPE_LINKED) {
        if ((pm->ps->playerStateFlags & PMF_LADDER) != 0) {
            if (coduo_int32_from_bits((uint32_t)pm->command.commandTime - (uint32_t)pm->ps->lastJumpCommandTime) < pm_ladderJumpTime) {
                return;
            }
            {
#if EMULATE_X87
                float ladderDenom =
                    x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(pm_ladderScale), x87f_load_f32(1.5f)), x87f_load_f32(127.0f)));
#else
                float ladderDenom = pm_ladderScale * 1.5f * 127.0f;
#endif
                float velZ = pm->ps->velocity[2];
                if ((pm->ps->playerStateFlags & 0x80) != 0 || pm->ps->leanFraction != 0.0f || isnan(pm->ps->leanFraction)) {
#if EMULATE_X87
                    stepInterval = x87f_store_f32(
                        x87f_mul(x87f_div(x87f_load_f32(velZ), x87f_mul(x87f_load_f32(pm->ps->walkSpeedScale), x87f_load_f32(ladderDenom))),
                                 x87f_load_f32(0.35f)));
#else
                    stepInterval = (velZ / (pm->ps->walkSpeedScale * ladderDenom)) * 0.35f;
#endif
                } else {
#if EMULATE_X87
                    stepInterval = x87f_store_f32(
                        x87f_mul(x87f_div(x87f_load_f32(velZ), x87f_mul(x87f_load_f32(pm->ps->runSpeedScale), x87f_load_f32(ladderDenom))),
                                 x87f_load_f32(0.45f)));
#else
                    stepInterval = (velZ / (pm->ps->runSpeedScale * ladderDenom)) * 0.45f;
#endif
                }
            }
            if (pm->ps->velocity[2] >= 0.0f) {
                anim = BG_ExecuteCommand(pm->ps, 3, PM_FOOTSTEPS_ANIM_AIRBORNE_UP, 1);
            } else {
                anim = BG_ExecuteCommand(pm->ps, 3, PM_FOOTSTEPS_ANIM_AIRBORNE_DOWN, 1);
            }
            oldTime = pm->ps->bobCycle;
            pm->ps->bobCycle = PM_FOOTSTEPS_BOB_CYCLE(oldTime, pml.msec, stepInterval);
            PM_FootstepEvent(oldTime, pm->ps->bobCycle, 1);
        }
        if (effectiveStance == (int)(pm->ps->playerStateFlags & 3)) {
            return;
        }
    }

    sprintBit = pm->ps->playerStateFlags & 0x80;
    stanceBits = pm->ps->playerStateFlags;

    /* Idle / very slow movement */
    if (pm->horizontalSpeed < 10.0f || pm->ps->pmType == PM_TYPE_LINKED) {
        if (pm->horizontalSpeed < 1.0f) {
            pm->ps->bobCycle = 0;
        }
        if (pm->ps->viewHeightTarget == pm->ps->proneViewHeight) {
            anim = BG_ExecuteCommand(pm->ps, 3, 3, 1);
        } else if (pm->ps->viewHeightTarget == pm->ps->crouchViewHeight) {
            anim = BG_ExecuteCommand(pm->ps, 3, 2, 1);
        }
        if (anim < 0) {
            BG_ExecuteCommand(pm->ps, 3, 1, 1);
        }
        return;
    }

    /* Normal movement footsteps */
    moveSpeed = (float)pm->ps->speed;

    /* Adjust speed for strafe/backward */
    if (pm->command.forwardmove == 0) {
        if (pm->command.rightmove != 0) {
#if EMULATE_X87
            moveSpeed = x87f_store_f32(
                x87f_mul(x87f_load_f32(moveSpeed),
                         x87f_add(x87f_mul(x87f_sub(x87f_load_f32(pm->ps->strafeSpeedScale), x87f_load_f32(1.0f)), x87f_load_f32(0.75f)),
                                  x87f_load_f32(1.0f))));
#else
            moveSpeed *= (pm->ps->strafeSpeedScale - 1.0f) * 0.75f + 1.0f;
#endif
            if (pm->command.rightmove < 1) {
                BG_UpdateConditionValue(pm->ps->psClientNum, 10, 1, 1);
            } else {
                BG_UpdateConditionValue(pm->ps->psClientNum, 10, 2, 1);
            }
        }
    } else {
        if (pm->command.rightmove == 0) {
            if (pm->command.forwardmove < 0) {
                moveSpeed *= pm->ps->backSpeedScale;
            }
        } else {
#if EMULATE_X87
            moveSpeed = x87f_store_f32(
                x87f_mul(x87f_load_f32(moveSpeed),
                         x87f_mul(x87f_add(x87f_add(x87f_mul(x87f_sub(x87f_load_f32(pm->ps->strafeSpeedScale), x87f_load_f32(1.0f)),
                                                             x87f_load_f32(0.75f)),
                                                    x87f_load_f32(1.0f)),
                                           x87f_load_f32(1.0f)),
                                  x87f_load_f32(0.5f))));
#else
            moveSpeed *= ((pm->ps->strafeSpeedScale - 1.0f) * 0.75f + 1.0f + 1.0f) * 0.5f;
#endif
            if (pm->command.forwardmove < 0) {
#if EMULATE_X87
                moveSpeed = x87f_store_f32(
                    x87f_mul(x87f_load_f32(moveSpeed),
                             x87f_mul(x87f_add(x87f_load_f32(pm->ps->backSpeedScale), x87f_load_f32(1.0f)), x87f_load_f32(0.5f))));
#else
                moveSpeed *= (pm->ps->backSpeedScale + 1.0f) * 0.5f;
#endif
            }
        }
        BG_UpdateConditionValue(pm->ps->psClientNum, 10, 0, 1);
    }

    /* Apply stance speed multiplier */
    if (sprintBit == 0) {
        if ((stanceBits & PMF_SPRINTING) == 0) {
            moveSpeed *= pm->ps->runSpeedScale;
        } else {
            moveSpeed *= pm->ps->sprintSpeedScale;
        }
    } else {
        moveSpeed *= pm->ps->walkSpeedScale;
    }

    /* Apply viewheight lerp speed modifier */
    {
        float lerpFrac = PM_GetViewHeightLerp(pm->ps->crouchViewHeight, pm->ps->proneViewHeight);
        if (lerpFrac != 0.0f || isnan(lerpFrac)) {
#if EMULATE_X87
            moveSpeed = x87f_store_f32(
                x87f_mul(x87f_load_f32(moveSpeed),
                         x87f_add(x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(lerpFrac)), x87f_load_f32(pm->ps->crouchSpeedScale)),
                                  x87f_mul(x87f_load_f32(lerpFrac), x87f_load_f32(pm->ps->proneSpeedScale)))));
#else
            moveSpeed *= (1.0f - lerpFrac) * pm->ps->crouchSpeedScale + lerpFrac * pm->ps->proneSpeedScale;
#endif
        } else {
            lerpFrac = PM_GetViewHeightLerp(pm->ps->proneViewHeight, pm->ps->crouchViewHeight);
            if (lerpFrac != 0.0f || isnan(lerpFrac)) {
#if EMULATE_X87
                moveSpeed = x87f_store_f32(x87f_mul(
                    x87f_load_f32(moveSpeed),
                    x87f_add(x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(lerpFrac)), x87f_load_f32(pm->ps->proneSpeedScale)),
                             x87f_mul(x87f_load_f32(lerpFrac), x87f_load_f32(pm->ps->crouchSpeedScale)))));
#else
                moveSpeed *= (1.0f - lerpFrac) * pm->ps->proneSpeedScale + lerpFrac * pm->ps->crouchSpeedScale;
#endif
            } else if (effectiveStance == 1) {
                moveSpeed *= pm->ps->proneSpeedScale;
            } else if (effectiveStance == 2) {
                moveSpeed *= pm->ps->crouchSpeedScale;
            }
        }
    }

    /* Select animation and compute step interval */
    if (effectiveStance == 1) {
        if (sprintBit == 0) {
            stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.25f);
        } else {
            stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.24f);
        }
        if ((pm->ps->playerStateFlags & 0x40) == 0) {
            anim = BG_ExecuteCommand(pm->ps, 3, 8, 1);
        } else {
            anim = BG_ExecuteCommand(pm->ps, 3, 9, 1);
        }
    } else if (effectiveStance == 2) {
        if (sprintBit == 0) {
            stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.34f);
        } else {
            stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.315f);
        }
        if ((pm->ps->playerStateFlags & 0x40) == 0) {
            if (sprintBit == 0) {
                anim = BG_ExecuteCommand(pm->ps, 3, PM_FOOTSTEPS_ANIM_CROUCH_C, 1);
            } else {
                anim = BG_ExecuteCommand(pm->ps, 3, 6, 1);
            }
        } else if (sprintBit == 0) {
            anim = BG_ExecuteCommand(pm->ps, 3, PM_FOOTSTEPS_ANIM_CROUCH_D, 1);
        } else {
            anim = BG_ExecuteCommand(pm->ps, 3, 7, 1);
        }
    } else if ((pm->ps->playerStateFlags & 0x40) == 0) {
        if (sprintBit == 0) {
            stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.335f);
            anim = BG_ExecuteCommand(pm->ps, 3, 10, 1);
        } else {
            stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.305f);
            anim = BG_ExecuteCommand(pm->ps, 3, 4, 1);
        }
    } else if (sprintBit == 0) {
        stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.36f);
        anim = BG_ExecuteCommand(pm->ps, 3, PM_FOOTSTEPS_ANIM_STAND_D, 1);
    } else {
        stepInterval = PM_FOOTSTEPS_STEP_INTERVAL(pm->horizontalSpeed, moveSpeed, 0.325f);
        anim = BG_ExecuteCommand(pm->ps, 3, 5, 1);
    }

    shouldMake = PM_ShouldMakeFootsteps();
    oldTime = pm->ps->bobCycle;
    pm->ps->bobCycle = PM_FOOTSTEPS_BOB_CYCLE(oldTime, pml.msec, stepInterval);

    /* Idle animation if not moving */
    if (pm->command.forwardmove == 0 && pm->command.rightmove == 0) {
        if (pm->horizontalSpeed <= 120.0f) {
            if (pm->ps->viewHeightTarget == pm->ps->proneViewHeight) {
                anim = BG_ExecuteCommand(pm->ps, 3, 3, 1);
            } else if (pm->ps->viewHeightTarget == pm->ps->crouchViewHeight) {
                anim = BG_ExecuteCommand(pm->ps, 3, 2, 1);
            }
            if (anim < 0) {
                BG_ExecuteCommand(pm->ps, 3, 1, 1);
            }
        }
    } else {
        if (anim < 0) {
            BG_ExecuteCommand(pm->ps, 3, 1, 1);
        }
        PM_FootstepEvent(oldTime, pm->ps->bobCycle, shouldMake);
    }
}
#endif
