#include "bg_pmove.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define PM_LADDER_VIEW_PITCH_OFFSET 0.25f
#define PM_LADDER_VIEW_PITCH_SCALE 2.5f
#define PM_LADDER_STRAFE_SCALE 0.2f
#define PM_LADDER_MIN_LATERAL_STEP 1.0f
#define PM_LADDER_AIR_ATTACH_UPWARD (-500.0f)
#define PM_LADDER_AIR_ATTACH_OTHER (-250.0f)
#define PM_LADDER_YAW_OFFSET 180.0f

enum {
    PM_LADDER_MOVEMENT_DIR_LIMIT = 75
};

const float pm_ladderPushOff = 128.0f;
const int32_t pm_ladderJumpTime = 300;
const float pm_ladderScale = 0.5f;
const float pm_ladderfriction = 16.0f;

/*
 * Complete ladder-contact and ladder-movement subsystem. The Windows module
 * pairs are instruction-identical apart from relocations:
 *
 *   uo_cgame_mp_x86.dll  PM_CheckLadderMove 0x3000d920..0x3000dc62
 *   uo_game_mp_x86.dll   PM_CheckLadderMove 0x2000d6d0..0x2000da12
 *   uo_cgame_mp_x86.dll  PM_LadderMove      0x3000dc70..0x3000e04f
 *   uo_game_mp_x86.dll   PM_LadderMove      0x2000da20..0x2000ddff
 *
 * Linux game retains the same state machines at RVAs 0x0002c9a6 and
 * 0x0002cebc. Its unoptimized PM_CheckLadderMove calls helpers that Windows
 * inlines. PM_LadderMove has proved compiler spill/association differences at
 * the forward, strafe, and gravity expressions; only those expressions select
 * a behavior realization below.
 */

void PM_CheckLadderMove(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;

    if (ps->pmTime != 0 &&
        (ps->playerStateFlags & PMF_LADDER_TIMER_BLOCK_MASK) != 0) {
        return;
    }

    const float traceDistance = pml.walking != 0 ? 8.0f : 30.0f;
    const qboolean wasOnLadder =
        (ps->playerStateFlags & PMF_LADDER) != 0 ? qtrue : qfalse;
    const qboolean ladderWithoutGround =
        wasOnLadder != qfalse && ps->groundEntityNum == ENTITYNUM_NONE
            ? qtrue : qfalse;
    vec3_t probeDirection;

    if (ladderWithoutGround != qfalse) {
        for (int32_t lane = 0; lane < 3; ++lane) {
            uint32_t bits;

            memcpy(&bits, &ps->ladderNormal[lane], sizeof(bits));
            bits ^= UINT32_C(0x80000000);
            memcpy(&probeDirection[lane], &bits, sizeof(bits));
        }
    } else {
        probeDirection[0] = pml.forward[0];
        probeDirection[1] = pml.forward[1];
        probeDirection[2] = 0.0f;
        (void)VectorNormalize(probeDirection);
    }

    move = pm;
    ps = move->ps;
    ps->playerStateFlags &= ~(uint32_t)PMF_LADDER;

    if (ps->pmType >= PM_TYPE_DEAD) {
        ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = 0;
        pml.groundLiftFlag = 0;
        pml.walking = 0;
        return;
    }

    if (PM_GetEffectiveStance(ps) == EFFECTIVE_STANCE_PRONE) {
        return;
    }

    const int32_t sinceLastJump = coduo_int32_from_bits(
        (uint32_t)move->command.commandTime -
        (uint32_t)ps->lastJumpCommandTime);
    if (sinceLastJump < pm_ladderJumpTime) {
        return;
    }

    vec3_t probeMins;
    vec3_t probeMaxs;
    probeMins[0] = move->mins[0] + 6.0f;
    probeMins[1] = move->mins[1] + 6.0f;
    probeMins[2] = 8.0f;
    probeMaxs[0] = move->maxs[0] - 6.0f;
    probeMaxs[1] = move->maxs[1] - 6.0f;
    probeMaxs[2] = move->maxs[2];
    if (probeMaxs[2] < 8.0f) {
        probeMaxs[2] = 8.0f;
    }

    vec3_t end;
#if EMULATE_X87
    for (int32_t lane = 0; lane < 3; ++lane) {
        end[lane] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(probeDirection[lane]),
                     x87f_load_f32(traceDistance)),
            x87f_load_f32(move->ps->psOrigin[lane])));
    }
#else
    for (int32_t lane = 0; lane < 3; ++lane) {
        end[lane] = (float)(
            (long double)probeDirection[lane] *
                (long double)traceDistance +
            (long double)move->ps->psOrigin[lane]);
    }
#endif

    trace_t trace;
    ps = move->ps;
    PM_trace(&trace, ps->psOrigin, probeMins, probeMaxs, end,
             ps->psClientNum, move->traceMask);

    move = pm;
    if (trace.fraction < 1.0f &&
        (trace.surfaceFlags & SURF_LADDER) != 0 &&
        (pml.walking == 0 || move->command.forwardmove > 0)) {
        move->ps->ladderNormal[0] = trace.normal[0];
        move->ps->ladderNormal[1] = trace.normal[1];
        move->ps->ladderNormal[2] = trace.normal[2];

        if (wasOnLadder != qfalse) {
            move->ps->playerStateFlags |= PMF_LADDER;
            return;
        }

        ps = move->ps;
        for (int32_t lane = 0; lane < 3; ++lane) {
            uint32_t bits;

            memcpy(&bits, &ps->ladderNormal[lane], sizeof(bits));
            bits ^= UINT32_C(0x80000000);
            memcpy(&probeDirection[lane], &bits, sizeof(bits));
        }

#if EMULATE_X87
        for (int32_t lane = 0; lane < 3; ++lane) {
            end[lane] = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(probeDirection[lane]),
                         x87f_load_f32(traceDistance)),
                x87f_load_f32(ps->psOrigin[lane])));
        }
#else
        for (int32_t lane = 0; lane < 3; ++lane) {
            end[lane] = (float)(
                (long double)probeDirection[lane] *
                    (long double)traceDistance +
                (long double)ps->psOrigin[lane]);
        }
#endif

        PM_trace(&trace, ps->psOrigin, probeMins, probeMaxs, end,
                 ps->psClientNum, move->traceMask);
        if (trace.fraction < 1.0f &&
            (trace.surfaceFlags & SURF_LADDER) != 0) {
            pm->ps->playerStateFlags |= PMF_LADDER;
            return;
        }
    }

    if (ladderWithoutGround != qfalse) {
        (void)BG_AnimScriptEvent(pm->ps, ANIM_EVENT_JUMP, qfalse, qtrue);
    }
}

void PM_LadderMove(void)
{
    if (PM_CheckJump() != qfalse) {
        PM_AirMove();
        pm->ps->lastJumpCommandTime = pm->command.commandTime;
        return;
    }

    float climbScale;
#if EMULATE_X87
    climbScale = x87f_store_f32(x87f_mul(
        x87f_add(x87f_load_f32(pml.forward[2]),
                 x87f_load_f32(PM_LADDER_VIEW_PITCH_OFFSET)),
        x87f_load_f32(PM_LADDER_VIEW_PITCH_SCALE)));
#else
    climbScale = (float)(
        ((long double)pml.forward[2] +
         (long double)PM_LADDER_VIEW_PITCH_OFFSET) *
        (long double)PM_LADDER_VIEW_PITCH_SCALE);
#endif
    if (climbScale > 1.0f) {
        climbScale = 1.0f;
    } else if (climbScale < -1.0f) {
        climbScale = -1.0f;
    }

    pml.forward[2] = 0.0f;
    (void)VectorNormalize(pml.forward);
    pml.right[2] = 0.0f;

    vec3_t horizontalRight;
    (void)VectorNormalize2(pml.right, horizontalRight);
    ProjectPointOnPlane(pml.right, horizontalRight, pm->ps->ladderNormal);

    const float commandScale = PM_CmdScale(&pm->command);
    vec3_t wishVelocity = {0.0f, 0.0f, 0.0f};

    if (pm->command.forwardmove != 0) {
#if defined(WINDOWS_BEHAVIOR)
        const float forwardMove = (float)pm->command.forwardmove;
#if EMULATE_X87
        wishVelocity[2] = x87f_store_f32(x87f_mul(
            x87f_mul(x87f_mul(x87f_load_f32(forwardMove),
                              x87f_load_f32(commandScale)),
                     x87f_load_f32(climbScale)),
            x87f_load_f32(pm_ladderScale)));
#else
        wishVelocity[2] = (float)(
            (long double)forwardMove * (long double)commandScale *
            (long double)climbScale * (long double)pm_ladderScale);
#endif
#else
#if EMULATE_X87
        wishVelocity[2] = x87f_store_f32(x87f_mul(
            x87f_mul(x87f_mul(x87f_load_f32(pm_ladderScale),
                              x87f_load_f32(climbScale)),
                     x87f_load_f32(commandScale)),
            x87f_load_i32(pm->command.forwardmove)));
#else
        wishVelocity[2] = (float)(
            (long double)pm_ladderScale * (long double)climbScale *
            (long double)commandScale *
            (long double)pm->command.forwardmove);
#endif
#endif
    }

    if (pm->command.rightmove != 0) {
#if defined(WINDOWS_BEHAVIOR)
        const float rightMove = (float)pm->command.rightmove;
#if EMULATE_X87
        const float sideSpeed = x87f_store_f32(x87f_mul(
            x87f_mul(x87f_load_f32(rightMove),
                     x87f_load_f32(commandScale)),
            x87f_load_f32(PM_LADDER_STRAFE_SCALE)));
#else
        const float sideSpeed = (float)(
            (long double)rightMove * (long double)commandScale *
            (long double)PM_LADDER_STRAFE_SCALE);
#endif
        for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
            wishVelocity[lane] = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(sideSpeed),
                         x87f_load_f32(pml.right[lane])),
                x87f_load_f32(wishVelocity[lane])));
#else
            wishVelocity[lane] = (float)(
                (long double)sideSpeed * (long double)pml.right[lane] +
                (long double)wishVelocity[lane]);
#endif
        }
#else
        for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
            wishVelocity[lane] = x87f_store_f32(x87f_add(
                x87f_load_f32(wishVelocity[lane]),
                x87f_mul(
                    x87f_mul(
                        x87f_mul(x87f_load_f32(commandScale),
                                 x87f_load_f32(PM_LADDER_STRAFE_SCALE)),
                        x87f_load_i32(pm->command.rightmove)),
                    x87f_load_f32(pml.right[lane]))));
#else
            wishVelocity[lane] = (float)(
                (long double)wishVelocity[lane] +
                (long double)commandScale *
                    (long double)PM_LADDER_STRAFE_SCALE *
                    (long double)pm->command.rightmove *
                    (long double)pml.right[lane]);
#endif
        }
#endif
    }

    vec3_t wishDirection;
    const float wishSpeed = VectorNormalize2(wishVelocity, wishDirection);
    PM_Accelerate(wishDirection, wishSpeed, 9.0f);

    if (pm->command.forwardmove == 0) {
        playerState_t *ps = pm->ps;
#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
        const x87f gravityStep = x87f_mul(
            x87f_load_f32((float)ps->gravity),
            x87f_load_f32(pml.frametime));
#else
        const x87f gravityStep = x87f_mul(
            x87f_load_i32(ps->gravity), x87f_load_f32(pml.frametime));
#endif
#else
#if defined(WINDOWS_BEHAVIOR)
        const long double gravityStep =
            (long double)(float)ps->gravity * (long double)pml.frametime;
#else
        const long double gravityStep =
            (long double)ps->gravity * (long double)pml.frametime;
#endif
#endif
        if (ps->velocity[2] > 0.0f) {
#if EMULATE_X87
            ps->velocity[2] = x87f_store_f32(x87f_sub(
                x87f_load_f32(ps->velocity[2]), gravityStep));
#else
            ps->velocity[2] = (float)(
                (long double)ps->velocity[2] - gravityStep);
#endif
            if (ps->velocity[2] < 0.0f) {
                ps->velocity[2] = 0.0f;
            }
        } else {
#if EMULATE_X87
            ps->velocity[2] = x87f_store_f32(x87f_add(
                x87f_load_f32(ps->velocity[2]), gravityStep));
#else
            ps->velocity[2] = (float)(
                (long double)ps->velocity[2] + gravityStep);
#endif
            if (ps->velocity[2] > 0.0f) {
                ps->velocity[2] = 0.0f;
            }
        }
    }

    if (pm->command.rightmove == 0) {
        vec2_t ladderRight = {pml.right[0], pml.right[1]};
        (void)VectorNormalize2D(ladderRight);
#if EMULATE_X87
        float sideSpeed = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(ladderRight[0]),
                     x87f_load_f32(pm->ps->velocity[0])),
            x87f_mul(x87f_load_f32(ladderRight[1]),
                     x87f_load_f32(pm->ps->velocity[1]))));
#else
        float sideSpeed = (float)(
            (long double)ladderRight[0] *
                (long double)pm->ps->velocity[0] +
            (long double)ladderRight[1] *
                (long double)pm->ps->velocity[1]);
#endif

        if (sideSpeed != 0.0f || isnan(sideSpeed)) {
            for (int32_t lane = 0; lane < 2; ++lane) {
#if EMULATE_X87
                pm->ps->velocity[lane] = x87f_store_f32(x87f_add(
                    x87f_load_f32(pm->ps->velocity[lane]),
                    x87f_mul(x87f_neg(x87f_load_f32(sideSpeed)),
                             x87f_load_f32(ladderRight[lane]))));
#else
                pm->ps->velocity[lane] = (float)(
                    (long double)pm->ps->velocity[lane] -
                    (long double)sideSpeed *
                        (long double)ladderRight[lane]);
#endif
            }

#if EMULATE_X87
            float sideDrop = x87f_store_f32(x87f_mul(
                x87f_mul(x87f_load_f32(sideSpeed),
                         x87f_load_f32(pml.frametime)),
                x87f_load_f32(pm_ladderfriction)));
#else
            float sideDrop = (float)(
                (long double)sideSpeed * (long double)pml.frametime *
                (long double)pm_ladderfriction);
#endif
            if (fabsf(sideDrop) < fabsf(sideSpeed)) {
                if (fabsf(sideDrop) < PM_LADDER_MIN_LATERAL_STEP) {
                    sideDrop = (float)PM_FloatSign(sideDrop);
                }
#if EMULATE_X87
                sideSpeed = x87f_store_f32(x87f_sub(
                    x87f_load_f32(sideSpeed), x87f_load_f32(sideDrop)));
#else
                sideSpeed = (float)((long double)sideSpeed -
                                    (long double)sideDrop);
#endif
                for (int32_t lane = 0; lane < 2; ++lane) {
#if EMULATE_X87
                    pm->ps->velocity[lane] = x87f_store_f32(x87f_add(
                        x87f_load_f32(pm->ps->velocity[lane]),
                        x87f_mul(x87f_load_f32(ladderRight[lane]),
                                 x87f_load_f32(sideSpeed))));
#else
                    pm->ps->velocity[lane] = (float)(
                        (long double)pm->ps->velocity[lane] +
                        (long double)ladderRight[lane] *
                            (long double)sideSpeed);
#endif
                }
            }
        }
    }

    if (pml.walking == 0) {
#if EMULATE_X87
        const float normalSpeed = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(pm->ps->ladderNormal[0]),
                     x87f_load_f32(pm->ps->velocity[0])),
            x87f_mul(x87f_load_f32(pm->ps->ladderNormal[1]),
                     x87f_load_f32(pm->ps->velocity[1]))));
#else
        const float normalSpeed = (float)(
            (long double)pm->ps->ladderNormal[0] *
                (long double)pm->ps->velocity[0] +
            (long double)pm->ps->ladderNormal[1] *
                (long double)pm->ps->velocity[1]);
#endif
        for (int32_t lane = 0; lane < 2; ++lane) {
#if EMULATE_X87
            pm->ps->velocity[lane] = x87f_store_f32(x87f_add(
                x87f_load_f32(pm->ps->velocity[lane]),
                x87f_mul(x87f_neg(x87f_load_f32(normalSpeed)),
                         x87f_load_f32(pm->ps->ladderNormal[lane]))));
#else
            pm->ps->velocity[lane] = (float)(
                (long double)pm->ps->velocity[lane] -
                (long double)normalSpeed *
                    (long double)pm->ps->ladderNormal[lane]);
#endif
        }

        const float attachForce = wishVelocity[2] > 0.0f
                                      ? PM_LADDER_AIR_ATTACH_UPWARD
                                      : PM_LADDER_AIR_ATTACH_OTHER;
        for (int32_t lane = 0; lane < 2; ++lane) {
#if EMULATE_X87
            pm->ps->velocity[lane] = x87f_store_f32(x87f_add(
                x87f_load_f32(pm->ps->velocity[lane]),
                x87f_mul(x87f_load_f32(pm->ps->ladderNormal[lane]),
                         x87f_load_f32(attachForce))));
#else
            pm->ps->velocity[lane] = (float)(
                (long double)pm->ps->velocity[lane] +
                (long double)pm->ps->ladderNormal[lane] *
                    (long double)attachForce);
#endif
        }
    }

    PM_StepSlideMove(qfalse);

#if EMULATE_X87
    const float ladderYaw = x87f_store_f32(x87f_add(
        x87f_load_f32(vectoyaw(pm->ps->ladderNormal)),
        x87f_load_f32(PM_LADDER_YAW_OFFSET)));
#else
    const float ladderYaw = (float)(
        (long double)vectoyaw(pm->ps->ladderNormal) +
        (long double)PM_LADDER_YAW_OFFSET);
#endif
    const float yawDelta = AngleDelta(ladderYaw, pm->ps->viewAngles[1]);
    int32_t movementDir =
        coduo_fp_to_i32_extended((long double)yawDelta);
    int32_t magnitude = movementDir;
    if (magnitude < 0) {
        magnitude = coduo_int32_from_bits(0u - (uint32_t)magnitude);
    }
    if (magnitude > PM_LADDER_MOVEMENT_DIR_LIMIT) {
        movementDir = movementDir > 0 ? PM_LADDER_MOVEMENT_DIR_LIMIT
                                      : -PM_LADDER_MOVEMENT_DIR_LIMIT;
    }
    pm->ps->movementDir = (int8_t)movementDir;
}
