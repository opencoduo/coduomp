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
#include "compat/coduo_x87_eval.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#define PM_ANGLE2SHORT_SCALE 182.04445f
#define PM_SHORT2ANGLE_SCALE 0.0054931641f
#define PM_DEGREES_TO_RADIANS 0.017453292f
#define PM_PRONE_DEBUG_LINE_LENGTH 18.0f

enum {
    PM_PITCH_CLAMP = 16000,
    PM_VEHICLE_PITCH_CLAMP = 12000,
    PM_DEATH_YAW_UNSET = 999
};

/* Repeated inline ANGLE2SHORT/SHORT2ANGLE and wrapping delta-angle sequences. */
#define PM_ANGLE_TO_SHORT(angle_) \
    ((uint16_t)((uint32_t)coduo_fp_to_i32_extended(                         \
        (long double)(angle_) * (long double)PM_ANGLE2SHORT_SCALE) & 0xffffu))
#define PM_SHORT_TO_ANGLE(angle_) \
    ((float)(uint16_t)(angle_) * PM_SHORT2ANGLE_SCALE)
#define PM_ADD_DELTA_ANGLE(ps_, axis_, angle_) do {                         \
    (ps_)->deltaAngles[(axis_)] =                                           \
        coduo_int32_from_bits((uint32_t)(ps_)->deltaAngles[(axis_)] +            \
                         (uint32_t)PM_ANGLE_TO_SHORT(angle_));               \
} while (0)

/* NOT_FROM_ORIGINAL_SOURCE: shared body for the original PM_UpdateViewAngles
 * entry point and the prediction pre-pass that must refresh command view
 * angles without advancing the incremental lean state a second time. */
static void coduomp_pm_update_view_angles_internal(playerState_t *ps,
                                                   const usercmd_t *cmd,
                                                   pm_trace_fn_t traceFunc,
                                                   qboolean updateLean)
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
            ps->stats[STAT_DEAD_YAW] = coduo_fp_to_i32_extended(
                (long double)(float)packed *
                (long double)PM_SHORT2ANGLE_SCALE);
        }
        if (updateLean != qfalse) {
            PM_UpdateLean(ps, cmd, traceFunc);
        }
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
        /* The direction products round to binary32 before the line scale and
         * origin add. Explicit stores preserve that boundary on x87 hosts. */
        volatile float forwardX = (float)((long double)cosPitch * (long double)cosYaw);
        volatile float forwardY = (float)((long double)cosPitch * (long double)sinYaw);
        end[0] = (float)((long double)start[0] +
                         (long double)forwardX * (long double)PM_PRONE_DEBUG_LINE_LENGTH);
        end[1] = (float)((long double)start[1] +
                         (long double)forwardY * (long double)PM_PRONE_DEBUG_LINE_LENGTH);
        end[2] = (float)((long double)start[2] -
                         (long double)sinPitch * (long double)PM_PRONE_DEBUG_LINE_LENGTH);
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
    if (updateLean != qfalse &&
        ps->pmType != PM_TYPE_UFO && ps->pmType != PM_TYPE_NOCLIP &&
        ps->pmType != PM_TYPE_SPECTATOR) {
        PM_UpdateLean(ps, cmd, traceFunc);
    }
}

void PM_UpdateViewAngles(playerState_t *ps, const usercmd_t *cmd,
                         pm_trace_fn_t traceFunc)
{
    coduomp_pm_update_view_angles_internal(ps, cmd, traceFunc, qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: prediction pre-pass wrapper. The original
 * CG_PredictPlayerState_Internal calls PM_UpdateViewAngles for every buffered
 * command before deciding which commands Pmove will simulate. That refreshes
 * the absolute command view angles, but also advances the incremental lean
 * state for commands that are skipped, so with pmove_fixed every frame replays
 * up to the full command history through PM_UpdateLean. This wrapper keeps the
 * view-angle update and leaves lean advancement to the simulated commands. */
void coduomp_pm_update_view_angles_without_lean(playerState_t *ps,
                                                const usercmd_t *cmd,
                                                pm_trace_fn_t traceFunc)
{
    coduomp_pm_update_view_angles_internal(ps, cmd, traceFunc, qfalse);
}
