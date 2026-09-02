// Source: uo_cgame_mp_x86.dll 0x3000c8e0..0x3000d466
//         uo_game_mp_x86.dll  0x2000c6a0..0x2000d217
//         game.mp.uo.i386.so  RVA 0x0002b15c..0x0002c158
//
// PM_UpdateViewAngles -- apply command angles to a player state, clamp controlled
// vehicle and ladder views, enforce prone yaw/pitch traversal, and update lean.
// The same-module PPC symbol and the field/call graph prove the name; the mcode's
// size-only server vehicle-dismount label is rejected.
// The two Windows bodies have the same 809-instruction operation graph after
// relocation normalization.  Their sole semantic instruction difference is
// the module-owned debug-line command immediate (cgame 202, game 72); the local
// bg_pmove_services.h adapters retain that boundary without forking this body.
// Supporting Mac cgame/game traceback symbols independently name equal-size
// 0xaa8-byte PM_UpdateViewAngles bodies at code offsets 0x3a20 and 0x4300.

#include "bg_pmove.h"

#include "bg_pmove_services.h"
#include "bg_weapon.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#define PM_ANGLE2SHORT_SCALE 182.04445f
#define PM_SHORT2ANGLE_SCALE 0.0054931641f
#define PM_DEGREES_TO_RADIANS 0.017453292f

enum {
    PM_PITCH_CLAMP = 16000,
    PM_VEHICLE_PITCH_CLAMP = 12000,
    PM_DEATH_YAW_UNSET = 999
};

/* Repeated inline ANGLE2SHORT/SHORT2ANGLE and wrapping delta-angle sequences. */
#if EMULATE_X87
#define PM_ANGLE_TO_SHORT(angle_) \
    ((uint16_t)((uint32_t)x87f_store_i32_trunc(x87f_mul(                 \
        x87f_load_f32((angle_)),                                        \
        x87f_load_f32(PM_ANGLE2SHORT_SCALE))) & UINT32_C(0x0000ffff)))
#else
#define PM_ANGLE_TO_SHORT(angle_) \
    ((uint16_t)((uint32_t)coduo_fp_to_i32_extended(                         \
        (long double)(angle_) * (long double)PM_ANGLE2SHORT_SCALE) & 0xffffu))
#endif
#define PM_SHORT_TO_ANGLE(angle_) \
    ((float)(uint16_t)(angle_) * PM_SHORT2ANGLE_SCALE)
#define PM_ADD_DELTA_ANGLE(ps_, axis_, angle_) do {                         \
    (ps_)->deltaAngles[(axis_)] =                                           \
        coduo_int32_from_bits((uint32_t)(ps_)->deltaAngles[(axis_)] +            \
                         (uint32_t)PM_ANGLE_TO_SHORT(angle_));               \
} while (0)

#if defined(WINDOWS_BEHAVIOR)
void PM_UpdateViewAngles(playerState_t *ps, const usercmd_t *cmd,
                         pm_trace_fn_t traceFunc)
{
    float previousYaw;
    float commandYaw;
    float yawDelta;
    float yawLimit;
    float correction = 0.0f;
    qboolean proneBlocked = qfalse;

    if (ps->pmType == PM_TYPE_INTERMISSION) {
        return;
    }

    if (ps->pmType >= PM_TYPE_DEAD) {
        if (ps->stats[STAT_DEAD_YAW] == PM_DEATH_YAW_UNSET) {
            int16_t packed = (int16_t)((uint16_t)ps->deltaAngles[1] +
                                       (uint16_t)cmd->angles[1]);
#if EMULATE_X87
            const float packedFloat = x87f_store_f32(x87f_load_i32(packed));
            ps->stats[STAT_DEAD_YAW] = x87f_store_i32_trunc(x87f_mul(
                x87f_load_f32(packedFloat),
                x87f_load_f32(PM_SHORT2ANGLE_SCALE)));
#else
            ps->stats[STAT_DEAD_YAW] = coduo_fp_to_i32_extended(
                (long double)(float)packed *
                (long double)PM_SHORT2ANGLE_SCALE);
#endif
        }
        PM_UpdateLean(ps, cmd, traceFunc);
        return;
    }

    previousYaw = ps->viewAngles[1];
    for (int axis = 0; axis < 3; ++axis) {
        int16_t packed = (int16_t)((uint16_t)ps->deltaAngles[axis] +
                                   (uint16_t)cmd->angles[axis]);

        if (axis == 0) {
            int32_t limit = (ps->entityStateFlags & EF_IN_VEHICLE) != 0
                                ? PM_VEHICLE_PITCH_CLAMP : PM_PITCH_CLAMP;
            if ((int32_t)packed > limit) {
                ps->deltaAngles[0] = coduo_int32_from_bits(
                    (uint32_t)limit - (uint32_t)cmd->angles[0]);
                packed = (int16_t)limit;
            } else if ((int32_t)packed < -limit) {
                ps->deltaAngles[0] = coduo_int32_from_bits(
                    0u - (uint32_t)limit - (uint32_t)cmd->angles[0]);
                packed = (int16_t)-limit;
            }
        }
        ps->viewAngles[axis] = (float)packed * PM_SHORT2ANGLE_SCALE;
    }
    commandYaw = ps->viewAngles[1];

    if ((ps->entityStateFlags & EF_IN_VEHICLE) != 0) {
        pmove_t *move = pm;
        if (move != NULL &&
            (move->viewClampMaxDeltas[0] != 0.0f ||
             move->viewClampMaxDeltas[1] != 0.0f ||
             move->viewClampMaxDeltas[2] != 0.0f)) {
            for (int axis = 0; axis < 3; ++axis) {
                float limit = move->viewClampMaxDeltas[axis];
                float delta;
                /* 0x3000ca40..0x3000ca57: FABS; FCOMP double 1.0; JNP skip —
                 * the axis is skipped whenever |limit| < 1.0, not only at 0. */
                if (fabsf(limit) < 1.0f) {
                    continue;
                }
                delta = AngleNormalize180(
                    move->viewClampTargetAngles[axis] - ps->viewAngles[axis]);
                if (fabsf(delta) > fabsf(limit)) {
                    float excess;
                    if (delta > limit) {
                        excess = delta - limit;
                        ps->viewAngles[axis] = AngleNormalize360Accurate(
                            move->viewClampTargetAngles[axis] - limit);
                    } else {
                        excess = delta + limit;
                        ps->viewAngles[axis] = AngleNormalize360Accurate(
                            move->viewClampTargetAngles[axis] + limit);
                    }
                    PM_ADD_DELTA_ANGLE(ps, axis, excess);
                }
            }
        }
    } else if ((ps->playerStateFlags & PMF_LADDER) != 0 &&
               ps->groundEntityNum == ENTITYNUM_NONE &&
               bg_ladder_yawcap.integer != 0) {
        float target = vectoyaw(ps->ladderNormal) + 180.0f;
        float limit = (float)bg_ladder_yawcap.integer;
        float delta = AngleNormalize180(target - ps->viewAngles[1]);
        if (fabsf(delta) > limit) {
            float excess;
            if (delta > limit) {
                excess = delta - limit;
                ps->viewAngles[1] = AngleNormalize360Accurate(target - limit);
            } else {
                excess = delta + limit;
                ps->viewAngles[1] = AngleNormalize360Accurate(target + limit);
            }
            PM_ADD_DELTA_ANGLE(ps, 1, excess);
        }
    }

    if ((ps->entityStateFlags & EF_RESTRICTED_MASK) != 0 ||
        ((ps->playerStateFlags & PMF_PRONE) == 0 &&
         ((ps->playerStateFlags & PMF_ADS) == 0 ||
          BG_PM_WEAPON_INFO(ps->currentWeapon)->weaponClass != WEAPCLASS_LMG))) {
        goto update_lean;
    }

    if (bg_debugProneCheck.integer != 0) {
        /* 0x3000cc53..0x3000cd9f: debug line and prone-yaw arc. */
        vec3_t start = { ps->psOrigin[0], ps->psOrigin[1],
                         ps->psOrigin[2] + (float)ps->proneViewHeight };
        vec3_t end;
        float pitch = ps->viewAngles[0] * PM_DEGREES_TO_RADIANS;
        float yaw = ps->viewAngles[1] * PM_DEGREES_TO_RADIANS;
        float sinPitch;
        float cosPitch;
        float sinYaw;
        float cosYaw;

        /* 0x3000cca0/0x3000ccd6 each execute one FSINCOS and store its two
         * binary32 outputs.  The native adapter emits that instruction on x86
         * and preserves the same paired-output interface on other hosts. */
        coduo_x87_sincosf(yaw, &sinYaw, &cosYaw);
        coduo_x87_sincosf(pitch, &sinPitch, &cosPitch);
        end[0] = (float)((long double)start[0] +
                         (long double)cosPitch * (long double)cosYaw * 18.0L);
        end[1] = (float)((long double)start[1] +
                         (long double)cosPitch * (long double)sinYaw * 18.0L);
        end[2] = (float)((long double)start[2] -
                         (long double)sinPitch * 18.0L);
        bg_compat_pmove_debug_line(start, end);
        bg_compat_pmove_debug_arc(
            start,
            ps->proneDirection - bg_prone_yawcap.value,
            ps->proneDirection + bg_prone_yawcap.value);
    }

    yawDelta = AngleNormalize180(ps->proneDirection - ps->viewAngles[1]);
    yawLimit = (float)bg_prone_yawcap.integer;
    if (BG_PM_WEAPON_INFO(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG) {
        yawLimit = (float)bg_lmg_yawcap.integer;
    }

    if ((ps->playerStateFlags & PMF_PRONE) != 0 &&
        !(BG_PM_WEAPON_INFO(ps->currentWeapon)->weaponClass == WEAPCLASS_LMG &&
          (ps->playerStateFlags & PMF_ADS) != 0)) {
        float moveThreshold = yawLimit - 5.0f;
        qboolean needsTraverse = fabsf(yawDelta) > moveThreshold;

        if (!needsTraverse &&
            (cmd->forwardmove != 0 || cmd->rightmove != 0) &&
            yawDelta != 0.0f) {
            needsTraverse = qtrue;
        }

        if (needsTraverse) {
            float maxStep = pml.frametime * 55.0f;
            float candidate;

            /* 0x3000ce76..0x3000ce87: FCOMPP; TEST AH,0x41; JNZ — equality goes
             * to the +/-maxStep branch, so this test is strict. */
            if (fabsf(yawDelta) < maxStep) {
                candidate = ps->viewAngles[1];
            } else {
                candidate = ps->proneDirection + (yawDelta > 0.0f ? -maxStep : maxStep);
            }

            if (BG_CheckProneTurned(ps, candidate, traceFunc) == 0) {
                qboolean keepSearching = qtrue;
                qboolean candidateValid = qfalse;

                for (;;) {
                    uint16_t remainingShort = (uint16_t)(
                        (uint32_t)coduo_fp_to_i32_extended(
                            ((long double)ps->proneDirection -
                             (long double)candidate) *
                            (long double)PM_ANGLE2SHORT_SCALE) &
                        0xffffu);
                    float remaining = PM_SHORT_TO_ANGLE(remainingShort);
                    float step;

                    if (remaining > 180.0f) {
                        remaining -= 360.0f;
                    }

                    if (fabsf(remaining) > 1.0f) {
                        step = remaining > 0.0f ? 1.0f : -1.0f;
                    } else {
                        step = remaining;
                        keepSearching = qfalse;
                        proneBlocked = qtrue;
                    }

                    candidate = AngleNormalize360Accurate(candidate + step);
                    if (BG_CheckProneTurned(ps, candidate, traceFunc) != 0) {
                        candidateValid = qtrue;
                        break;
                    }
                    if (!keepSearching) {
                        break;
                    }
                }

                if (!candidateValid) {
                    goto candidate_search_done;
                }
            }

            /* 0x3000cfb6..0x3000d052: the candidate must pass both prone
             * validity checks, in this exact order. */
            qboolean candidateFits =
                BG_CheckProneValid(
                    ps->psClientNum, ps->psOrigin, ps->playerMaxs[0],
                    30.0f, ps->viewAngles[1], NULL, NULL, NULL,
                    /* 0x3000cfed PUSH 0x1: skipInitialTrace */
                    qtrue, ps->groundEntityNum != ENTITYNUM_NONE, NULL,
                    traceFunc, NULL, qfalse, 45.0f, qfalse, NULL) != 0;
            if (candidateFits) {
                candidateFits =
                    BG_CheckProne(
                        ps->psClientNum, ps->psOrigin, ps->playerMaxs[0],
                        30.0f, candidate, NULL, NULL, NULL,
                        /* 0x3000d037 PUSH 0x1: skipInitialTrace */
                        qtrue,
                        ps->groundEntityNum != ENTITYNUM_NONE, NULL,
                        traceFunc, NULL, qfalse, 45.0f, NULL) != 0;
            }

            if (candidateFits) {
                ps->proneDirection = candidate;
            } else {
                proneBlocked = qtrue;
            }
candidate_search_done:
            ;
        }
    }

    correction = AngleNormalize180(ps->proneDirection - ps->viewAngles[1]);

    if (correction != 0.0f &&
        (ps->playerStateFlags & PMF_PRONE) != 0 &&
        BG_PM_WEAPON_INFO(ps->currentWeapon)->weaponClass != WEAPCLASS_LMG) {
        float candidateYaw = ps->proneDirection;
        qboolean maySearch = qtrue;

        for (;;) {
            qboolean bodyFits = BG_CheckProneValid(
                ps->psClientNum, ps->psOrigin, ps->playerMaxs[0], 30.0f,
                candidateYaw, NULL, NULL, NULL,
                /* 0x3000d10b PUSH 0x1: skipInitialTrace */
                qtrue,
                ps->groundEntityNum != ENTITYNUM_NONE, NULL, traceFunc, NULL,
                qfalse, 45.0f, qfalse, NULL) != 0;

            if (bodyFits && BG_CheckProneTurned(ps, candidateYaw, traceFunc) != 0) {
                ps->proneDirection = candidateYaw;
                break;
            }
            if (!maySearch) {
                break;
            }

            /* 0x3000d14d..0x3000d15e: the stop test clears maySearch for
             * |correction| <= 1 and for an unordered correction. */
            if (!(fabsf(correction) > 1.0f)) {
                maySearch = qfalse;
            } else {
                correction = correction > 0.0f ? 1.0f : -1.0f;
            }
            proneBlocked = qtrue;
            PM_ADD_DELTA_ANGLE(ps, 1, correction);
            ps->viewAngles[1] = AngleNormalize360Accurate(ps->viewAngles[1] + correction);
            /* 0x3000d1c8..0x3000d1e7: the remaining delta is recomputed into the
             * SAME slot the next iteration's stop test and the candidate advance
             * read — correction is refreshed every failing iteration. */
            correction = AngleNormalize180(
                ps->proneDirection - ps->viewAngles[1]);
            if (!bodyFits) {
                /* 0x3000d1f3..0x3000d20c: advances by the recomputed delta. */
                candidateYaw = AngleNormalize360Accurate(candidateYaw + correction);
            }
        }
    }

    /* 0x3000d221 (proneDirection store) falls straight into the clamp at
     * 0x3000d227 FLD [ESP+0x10] — the delta is NOT recomputed against the
     * possibly re-seated proneDirection; the last value of `correction`
     * (0x3000d081 or 0x3000d1e7) is what gets clamped. */
    if (correction > yawLimit) {
        float excess = correction - yawLimit;
        PM_ADD_DELTA_ANGLE(ps, 1, excess);
        ps->viewAngles[1] = AngleNormalize360Accurate(ps->proneDirection - yawLimit);
    } else if (correction < -yawLimit) {
        float excess = correction + yawLimit;
        PM_ADD_DELTA_ANGLE(ps, 1, excess);
        ps->viewAngles[1] = AngleNormalize360Accurate(ps->proneDirection + yawLimit);
    }

    if (proneBlocked) {
        float previousCorrection;
        ps->playerStateFlags |= PMF_PRONE_BLOCKED;
        previousCorrection = AngleNormalize180(previousYaw - ps->viewAngles[1]);
        /* 0x3000d2f7..0x3000d308 skips only for > 1 or unordered; equality
         * remains on the softening path. */
        if (fabsf(previousCorrection) <= 1.0f) {
            float commandCorrection = AngleNormalize180(
                commandYaw - ps->viewAngles[1]);
            if ((long double)commandCorrection *
                    (long double)previousCorrection >
                0.0L) {
                float softened = previousCorrection * 0.98000002f;
                ps->viewAngles[1] = AngleNormalize360Accurate(ps->viewAngles[1] + softened);
                /* 0x3000d339..0x3000d37c: the 0.98 product overwrites the
                 * previousCorrection slot; the deltaAngles add at 0x3000d363
                 * reloads the SOFTENED value. */
                PM_ADD_DELTA_ANGLE(ps, 1, softened);
            }
        }
    }

    {
        float pitchDelta = AngleNormalize180(
            ps->proneTorsoPitch - ps->viewAngles[0]);
        if (pitchDelta > 45.0f) {
            float excess = pitchDelta - 45.0f;
            PM_ADD_DELTA_ANGLE(ps, 0, excess);
            ps->viewAngles[0] = AngleNormalize180Accurate(ps->proneTorsoPitch - 45.0f);
        } else if (pitchDelta < -45.0f) {
            float excess = pitchDelta + 45.0f;
            PM_ADD_DELTA_ANGLE(ps, 0, excess);
            ps->viewAngles[0] = AngleNormalize180Accurate(ps->proneTorsoPitch + 45.0f);
        }
    }

update_lean:
    if (ps->pmType != PM_TYPE_UFO && ps->pmType != PM_TYPE_NOCLIP &&
        ps->pmType != PM_TYPE_SPECTATOR) {
        PM_UpdateLean(ps, cmd, traceFunc);
    }
}
#else
/*
 * The Linux game body is retained as a complete function because its
 * unoptimized x87 spills and unordered comparisons are observable at several
 * clamp boundaries.  It also calls the BG_CheckProne wrapper at all three
 * validation sites; the wrapper forwards to BG_CheckProneValid with the same
 * fixed final option that the Windows body sometimes passes directly.
 */
void PM_UpdateViewAngles(playerState_t *client,
                         const usercmd_t *command,
                         pm_trace_fn_t traceFunc)
{
    float oldYaw;
    float commandYaw;

    if (client->pmType == PM_TYPE_INTERMISSION) {
        return;
    }

    if (client->pmType >= PM_TYPE_DEAD) {
        int16_t yawShort = (int16_t)((uint16_t)client->deltaAngles[1] +
                                     (uint16_t)command->angles[1]);
        if (client->stats[STAT_DEAD_YAW] == 999) {
#if EMULATE_X87
            client->stats[STAT_DEAD_YAW] = x87f_store_i32_trunc(x87f_mul(
                x87f_load_i32(yawShort),
                x87f_load_f32(PM_SHORT2ANGLE_SCALE)));
#else
            client->stats[STAT_DEAD_YAW] =
                coduo_fp_to_i32_extended(
                    (long double)yawShort *
                    (long double)PM_SHORT2ANGLE_SCALE);
#endif
        }
        PM_UpdateLean(client, command, traceFunc);
        return;
    }

    oldYaw = client->viewAngles[1];
    for (int axis = 0; axis < 3; axis++) {
        int16_t angleShort = (int16_t)((uint16_t)client->deltaAngles[axis] +
                                       (uint16_t)command->angles[axis]);
        if (axis == 0) {
            int pitchCap = PM_PITCH_CLAMP;
            if ((client->entityStateFlags & EF_IN_VEHICLE) != 0) {
                pitchCap = PM_VEHICLE_PITCH_CLAMP;
            }
            if (pitchCap < angleShort) {
                client->deltaAngles[0] = coduo_int32_from_bits(
                    (uint32_t)pitchCap - (uint32_t)command->angles[0]);
                angleShort = (int16_t)pitchCap;
            } else if ((int)angleShort < -pitchCap) {
                client->deltaAngles[0] = coduo_int32_from_bits(
                    0u - (uint32_t)command->angles[0] - (uint32_t)pitchCap);
                angleShort = (int16_t)-pitchCap;
            }
        }
        client->viewAngles[axis] =
            (float)angleShort * PM_SHORT2ANGLE_SCALE;
    }
    commandYaw = client->viewAngles[1];

    if ((client->entityStateFlags & EF_IN_VEHICLE) == 0) {
        if ((client->playerStateFlags & PMF_LADDER) != 0 &&
            client->groundEntityNum == ENTITYNUM_NONE &&
            bg_ladder_yawcap.integer != 0) {
            const float *ladderNormal = client->ladderNormal;
            float ladderYaw = vectoyaw(ladderNormal) + 180.0f;
            float delta = AngleDelta(ladderYaw, client->viewAngles[1]);

            /* 0x2b5a0-0x2b694: the yaw-cap cvar integer enters every use
             * exact (fild), never rounded to float; the negated bound is an
             * integer negation. */
            if ((long double)bg_ladder_yawcap.integer < delta ||
                delta < (long double)coduo_int32_from_bits(
                            0u - (uint32_t)bg_ladder_yawcap.integer)) {
#if EMULATE_X87
                if ((long double)bg_ladder_yawcap.integer < delta) {
                    delta = x87f_store_f32(x87f_sub(
                        x87f_load_f32(delta),
                        x87f_load_i32(bg_ladder_yawcap.integer)));
                } else {
                    delta = x87f_store_f32(x87f_add(
                        x87f_load_f32(delta),
                        x87f_load_i32(bg_ladder_yawcap.integer)));
                }
                PM_ADD_DELTA_ANGLE(client, 1, delta);
                if (delta > 0.0f) {
                    client->viewAngles[1] = AngleNormalize360Accurate(
                        x87f_store_f32(x87f_sub(
                            x87f_load_f32(ladderYaw),
                            x87f_load_i32(bg_ladder_yawcap.integer))));
                } else {
                    client->viewAngles[1] = AngleNormalize360Accurate(
                        x87f_store_f32(x87f_add(
                            x87f_load_f32(ladderYaw),
                            x87f_load_i32(bg_ladder_yawcap.integer))));
                }
#else
                if ((long double)bg_ladder_yawcap.integer < delta) {
                    delta -= (long double)bg_ladder_yawcap.integer;
                } else {
                    delta += (long double)bg_ladder_yawcap.integer;
                }
                PM_ADD_DELTA_ANGLE(client, 1, delta);
                if (delta > 0.0f) {
                    client->viewAngles[1] =
                        AngleNormalize360Accurate(
                            ladderYaw - (long double)bg_ladder_yawcap.integer);
                } else {
                    client->viewAngles[1] =
                        AngleNormalize360Accurate(
                            ladderYaw + (long double)bg_ladder_yawcap.integer);
                }
#endif
            }
        }
    } else if (pm != NULL &&
               (pm->viewClampMaxDeltas[0] != 0.0f ||
                isnan(pm->viewClampMaxDeltas[0]) ||
                pm->viewClampMaxDeltas[1] != 0.0f ||
                isnan(pm->viewClampMaxDeltas[1]) ||
                pm->viewClampMaxDeltas[2] != 0.0f ||
                isnan(pm->viewClampMaxDeltas[2]))) {
        for (int axis = 0; axis < 3; axis++) {
            float maxDelta = pm->viewClampMaxDeltas[axis];
            if (fabsf(maxDelta) >= 1.0f) {
                float delta = AngleDelta(pm->viewClampTargetAngles[axis],
                                         client->viewAngles[axis]);
                if (fabsf(maxDelta) < fabsf(delta)) {
                    if (maxDelta < delta) {
                        delta -= maxDelta;
                    } else {
                        delta += maxDelta;
                    }
                    PM_ADD_DELTA_ANGLE(client, axis, delta);
                    if (delta > 0.0f) {
                        client->viewAngles[axis] =
                            AngleNormalize360Accurate(
                                pm->viewClampTargetAngles[axis] - maxDelta);
                    } else {
                        client->viewAngles[axis] =
                            AngleNormalize360Accurate(
                                pm->viewClampTargetAngles[axis] + maxDelta);
                    }
                }
            }
        }
    }

    if ((client->entityStateFlags & EF_RESTRICTED_MASK) == 0 &&
        ((client->playerStateFlags & PMF_PRONE) != 0 ||
         ((client->playerStateFlags & PMF_ADS) != 0 &&
          ((const weaponInfo_t *)BG_GetInfoForWeapon(client->currentWeapon))->weaponClass ==
          WEAPCLASS_LMG))) {
        int blocked = 0;
        float yawDelta;
        float yawCap;
        const weaponInfo_t *weaponInfo;

        if (bg_debugProneCheck.integer != 0) {
            vec3_t forward;
            vec3_t start;
            vec3_t end;

            start[0] = client->psOrigin[0];
            start[1] = client->psOrigin[1];
            start[2] = client->psOrigin[2];
            /* 0x2b72b: in-place add, the viewheight integer entering exact
             * via fild. */
#if EMULATE_X87
            start[2] = x87f_store_f32(x87f_add(
                x87f_load_f32(start[2]),
                x87f_load_i32(client->proneViewHeight)));
#else
            start[2] += (long double)client->proneViewHeight;
#endif
            AngleVectors(client->viewAngles, forward, NULL, NULL);
            /* 0x2b763: machine computes the scaled forward term first. */
#if EMULATE_X87
            for (int i = 0; i < 3; i++) {
                end[i] = x87f_store_f32(x87f_add(
                    x87f_mul(x87f_load_f32(forward[i]), x87f_load_f32(18.0f)),
                    x87f_load_f32(start[i])));
            }
#else
            end[0] = forward[0] * 18.0f + start[0];
            end[1] = forward[1] * 18.0f + start[1];
            end[2] = forward[2] * 18.0f + start[2];
#endif
            bg_compat_pmove_debug_line(start, end);
            bg_compat_pmove_debug_arc(
                start,
                client->proneDirection - bg_prone_yawcap.value,
                client->proneDirection + bg_prone_yawcap.value);
        }

        yawDelta = AngleDelta(client->proneDirection, client->viewAngles[1]);
        yawCap = (float)bg_prone_yawcap.integer;
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(client->currentWeapon);
        if (weaponInfo->weaponClass == WEAPCLASS_LMG) {
            yawCap = (float)bg_lmg_yawcap.integer;
        }

        if ((client->playerStateFlags & PMF_PRONE) != 0 &&
            (weaponInfo->weaponClass != WEAPCLASS_LMG ||
             (client->playerStateFlags & PMF_ADS) == 0) &&
            (yawDelta > yawCap - 5.0f ||
             yawDelta < -(yawCap - 5.0f) ||
             ((command->forwardmove != 0 || command->rightmove != 0) &&
              (yawDelta != 0.0f || isnan(yawDelta))))) {
            float targetYaw;
            int keepTrying = 1;
            int proneOK;

            /* frametime * 55.0f full width (0x68018 stock: fmulp; fcomip),
             * non-power-of-two -> emulate the compare. */
#if EMULATE_X87
            if (x87f_lt(x87f_abs(x87f_load_f32(yawDelta)),
                        x87f_mul(x87f_load_f32(pml.frametime),
                                 x87f_load_f32(55.0f)))) {
#else
            if (fabsf(yawDelta) < pml.frametime * 55.0f) {
#endif
                targetYaw = client->viewAngles[1];
            } else if (yawDelta > 0.0f) {
#if EMULATE_X87
                targetYaw = x87f_store_f32(x87f_sub(
                    x87f_load_f32(client->proneDirection),
                    x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(55.0f))));
#else
                targetYaw = client->proneDirection - pml.frametime * 55.0f;
#endif
            } else {
#if EMULATE_X87
                targetYaw = x87f_store_f32(x87f_add(
                    x87f_load_f32(client->proneDirection),
                    x87f_mul(x87f_load_f32(pml.frametime), x87f_load_f32(55.0f))));
#else
                targetYaw = client->proneDirection + pml.frametime * 55.0f;
#endif
            }

            while (BG_CheckProneTurned(client, targetYaw, traceFunc) == 0) {
                if (!keepTrying) {
                    goto prone_yaw_adjust_done;
                }
                yawDelta = AngleDelta(client->proneDirection, targetYaw);
                keepTrying = fabsf(yawDelta) > 1.0f;
                if (!keepTrying) {
                    blocked = 1;
                } else if (yawDelta > 0.0f) {
                    yawDelta = 1.0f;
                } else {
                    yawDelta = -1.0f;
                }
                targetYaw = AngleNormalize360Accurate(targetYaw + yawDelta);
            }

            proneOK = BG_CheckProne(client->psClientNum, client->psOrigin,
                                    client->playerMaxs[0], 30.0f,
                                    client->viewAngles[1], NULL, NULL, NULL, 1,
                                    client->groundEntityNum != ENTITYNUM_NONE,
                                    NULL, traceFunc, NULL, 0, 45.0f, NULL);
            if (proneOK != 0) {
                proneOK = BG_CheckProne(client->psClientNum, client->psOrigin,
                                        client->playerMaxs[0], 30.0f,
                                        targetYaw, NULL, NULL, NULL, 1,
                                        client->groundEntityNum !=
                                        ENTITYNUM_NONE, NULL, traceFunc,
                                        NULL, 0, 45.0f, NULL);
                if (proneOK != 0) {
                    client->proneDirection = targetYaw;
                }
            }
            if (proneOK == 0) {
                blocked = 1;
            }
        }

prone_yaw_adjust_done:
        yawDelta = AngleDelta(client->proneDirection, client->viewAngles[1]);
        if ((yawDelta != 0.0f || isnan(yawDelta)) &&
            (client->playerStateFlags & PMF_PRONE) != 0) {
            weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(client->currentWeapon);
            if (weaponInfo->weaponClass != WEAPCLASS_LMG) {
                float targetYaw = client->proneDirection;
                int keepTrying = 1;
                while (1) {
                    int proneOK = BG_CheckProne(
                        client->psClientNum, client->psOrigin,
                        client->playerMaxs[0], 30.0f, targetYaw,
                        NULL, NULL, NULL, 1,
                        client->groundEntityNum != ENTITYNUM_NONE,
                        NULL, traceFunc, NULL, 0, 45.0f, NULL);
                    int yawOK = 1;

                    if (proneOK != 0) {
                        yawOK = BG_CheckProneTurned(client, targetYaw,
                                                     traceFunc);
                    }
                    if (proneOK != 0 && yawOK != 0) {
                        break;
                    }
                    if (!keepTrying) {
                        goto prone_view_clamp_done;
                    }
                    keepTrying = fabsf(yawDelta) > 1.0f;
                    if (keepTrying) {
                        if (yawDelta > 0.0f) {
                            yawDelta = 1.0f;
                        } else {
                            yawDelta = -1.0f;
                        }
                    }
                    blocked = 1;
                    PM_ADD_DELTA_ANGLE(client, 1, yawDelta);
                    client->viewAngles[1] =
                        AngleNormalize360Accurate(client->viewAngles[1] +
                                                  yawDelta);
                    yawDelta = AngleDelta(client->proneDirection,
                                          client->viewAngles[1]);
                    if (proneOK == 0) {
                        targetYaw =
                            AngleNormalize360Accurate(targetYaw + yawDelta);
                    }
                }
                client->proneDirection = targetYaw;
            }
        }

prone_view_clamp_done:
        if (yawCap < yawDelta || yawDelta < -yawCap) {
            if (yawCap < yawDelta) {
                yawDelta -= yawCap;
            } else {
                yawDelta += yawCap;
            }
            PM_ADD_DELTA_ANGLE(client, 1, yawDelta);
            if (yawDelta > 0.0f) {
                client->viewAngles[1] =
                    AngleNormalize360Accurate(client->proneDirection - yawCap);
            } else {
                client->viewAngles[1] =
                    AngleNormalize360Accurate(client->proneDirection + yawCap);
            }
        }

        if (blocked != 0) {
            float oldYawDelta;

            client->playerStateFlags |= PMF_PRONE_BLOCKED;
            oldYawDelta = AngleDelta(oldYaw, client->viewAngles[1]);
            if (fabsf(oldYawDelta) <= 1.0f) {
                float commandYawDelta = AngleDelta(commandYaw,
                                                   client->viewAngles[1]);
                /* NO shim: sign test — rounding never flips the sign of the
                 * product, so `> 0.0f` is width-independent (rule 7). */
                if (oldYawDelta * commandYawDelta > 0.0f) {
                    oldYawDelta *= 0.98f;
                    client->viewAngles[1] =
                        AngleNormalize360Accurate(client->viewAngles[1] +
                                                  oldYawDelta);
                    PM_ADD_DELTA_ANGLE(client, 1, oldYawDelta);
                }
            }
        }

        yawDelta = AngleDelta(client->proneTorsoPitch, client->viewAngles[0]);
        if (yawDelta > 45.0f || yawDelta < -45.0f) {
            if (yawDelta > 45.0f) {
                yawDelta -= 45.0f;
            } else {
                yawDelta += 45.0f;
            }
            PM_ADD_DELTA_ANGLE(client, 0, yawDelta);
            if (yawDelta > 0.0f) {
                client->viewAngles[0] =
                    AngleNormalize180Accurate(client->proneTorsoPitch - 45.0f);
            } else {
                client->viewAngles[0] =
                    AngleNormalize180Accurate(client->proneTorsoPitch + 45.0f);
            }
        }
    }

    if (client->pmType != PM_TYPE_UFO &&
        client->pmType != PM_TYPE_NOCLIP &&
        client->pmType != PM_TYPE_SPECTATOR) {
        PM_UpdateLean(client, command, traceFunc);
    }
}

#endif
