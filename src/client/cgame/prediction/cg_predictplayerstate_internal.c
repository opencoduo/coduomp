// Source: uo_cgame_mp_x86.dll 0x30035800..0x30036069
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035800_30036069.mcode
//
// CG_PredictPlayerState_Internal replays the buffered usercmd window through
// Pmove, applies mover and prediction-error correction, and transitions the
// resulting local player state. The mcode's PmoveSingle size guess is rejected;
// the same-module PPC bank identifies this adjacent 0x860-byte routine exactly.

#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    CMD_BACKUP = 128,
    PMOVE_TRACE_MASK_SPECTATOR_CLEAR = 0x02010000,
    PMOVE_VEHICLE_TYPE = 12
};

#define PS_FLAG_DUCKED ((uint32_t)0x00000002u)
#define PS_FLAG_PREDICTION_PUSH ((uint32_t)0x00000200u)

#define ANGLE2SHORT_SCALE 182.04445f   /* .rdata 0x3007bd60 == 65536/360 (0x43360b61) */

void CG_PredictPlayerState_Internal(void)
{
    if (cg_demoPlayback != 0 ||
        (cg_snap->ps.playerStateFlags & PSF_FOLLOWING) != 0) {
        CG_InterpolatePlayerState(qfalse);
        return;
    }
    if (cg_nopredict_vmCvar.integer != 0 || g_synchronousClients_vmCvar.integer != 0) {
        CG_InterpolatePlayerState(qtrue);
        return;
    }

    usercmd_t *cmd = &cg_pmove.command;
    usercmd_t *oldCmd = &cg_pmove.oldCommand;

    cg_pmove.ps = (playerState_t *)&cg_predictedPlayerState;
    cg_pmove.trace = CG_TraceCapsule;
    cg_pmove.trace2 = CG_TraceCapsule;
    cg_pmove.trace3 = CG_TraceCapsule;
    cg_pmove.pointContents = CG_PointContents;
    cg_pmove.entityType = GetEntityTypeIfModelLoaded;
    cg_pmove.adsInputBlocked = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_IS_IN_MATCH_TIMEOUT));

    cg_pmove.traceMask = cg_predictedPlayerState.pmType < PM_TYPE_DEAD
                             ? MASK_PLAYERSOLID
                             : MASK_DEADSOLID;
    if (cg_snap->ps.pmType == PM_TYPE_SPECTATOR) {
        cg_pmove.traceMask &= ~PMOVE_TRACE_MASK_SPECTATOR_CLEAR;
    }

    const int32_t currentCmdNumber =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_CURRENT_CMD_NUMBER));
    const int32_t oldestCmdNumber = coduo_int32_from_bits(
        (uint32_t)currentCmdNumber - (uint32_t)(CMD_BACKUP - 1));
    usercmd_t oldestCmd;
    if (cgame_syscall(CG_GET_USER_CMD, oldestCmdNumber, &oldestCmd) == 0) {
        if (cg_showmiss_vmCvar.integer != 0) {
            Com_PrintMessage("exceeded PACKET_BACKUP on commands\n");
        }
        return;
    }

    /* 0x30035927..0x3003593c copies the old state after materializing the
     * latest-command trap arguments but before calling the trap. */
    playerState_t oldPlayerState = cg_predictedPlayerState;
    usercmd_t latestCmd;
    (void)cgame_syscall(CG_GET_USER_CMD, currentCmdNumber, &latestCmd);

    memcpy(&cg_predictedPlayerState, &cg_nextSnap->ps,
           sizeof(cg_predictedPlayerState));
    cg_latestSnapshotTime = cg_nextSnap->serverTime;

    cg_pmove.viewClampTargetAngles[2] = 0.0f;
    cg_pmove.viewClampTargetAngles[1] = 0.0f;
    cg_pmove.viewClampTargetAngles[0] = 0.0f;
    cg_pmove.viewClampMaxDeltas[2] = 0.0f;
    cg_pmove.viewClampMaxDeltas[1] = 0.0f;
    cg_pmove.viewClampMaxDeltas[0] = 0.0f;

    if ((cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) != 0) {
        int32_t followedEntityNum =
            cg_predictedPlayerState.viewLockedEntityNum;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)followedEntityNum >= (uint32_t)MAX_GENTITIES) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_PredictPlayerState_Internal: invalid "
                      "view-lock entity %i",
                      followedEntityNum);
            return;
        }
        centity_t *followed = &cg_entities[followedEntityNum];
        if (followed->currentValid != 0) {
            cg_pmove.viewClampTargetAngles[0] = followed->lerpAngles[0];
            cg_pmove.viewClampTargetAngles[1] = followed->lerpAngles[1];
            cg_pmove.viewClampTargetAngles[2] = followed->lerpAngles[2];
            if (cg_predictedPlayerState.vehicleType == 1 &&
                cg_predictedPlayerState.vehiclePosition == 3) {
                cg_pmove.viewClampMaxDeltas[1] = 70.0f;
                cg_pmove.viewClampMaxDeltas[0] = 60.0f;
            }
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    weaponInfo_t *selectedWeapon =
        bg_weaponInfos[cg_predictedPlayerState.currentWeapon];
    int32_t requestedPmoveMsec = pmove_msec.integer;
    cg_currentWeaponInfo = selectedWeapon;

    if (requestedPmoveMsec < PMOVE_MSEC_MIN) {
        cgame_syscall(CG_CVAR_SET_VALUE, (intptr_t)&pmove_msec,
                      "8");
    } else if (requestedPmoveMsec > PMOVE_MSEC_MAX) {
        cgame_syscall(CG_CVAR_SET_VALUE, (intptr_t)&pmove_msec,
                      "33");
    }

    cg_pmove.pmove_msec_min = pmove_fixed_vmCvar.integer;
    cg_pmove.pmove_msec_max = pmove_msec.integer;

    /* 0x30035821: the saved collision-entity slot ([ESP+0x34]) is zeroed ONCE at
     * function entry, not per iteration; after every Pmove the machine re-clears
     * the (possibly stale, already-zero) entity field through it without
     * resetting the slot. Hoisted out of the loop to match. */
    centity_t *predictionCollisionEntity = NULL;
    qboolean predictionRan = qfalse;
    int32_t cmdNumber = oldestCmdNumber;
    while (cmdNumber <= currentCmdNumber) {
        (void)cgame_syscall(CG_GET_USER_CMD, cmdNumber, cmd);

        /* 0x30035ab0: reads the pmove_fixed value CACHED into cg_pmove (+0xf8)
         * at 0x30035a7e, not the cvar itself. */
        if (cg_pmove.pmove_msec_min != 0) {
            PM_UpdateViewAngles(&cg_predictedPlayerState, cmd,
                                CG_TraceCapsule);
        }

        if (cmd->commandTime <= cg_predictedPlayerState.commandTime ||
            cmd->commandTime > latestCmd.commandTime) {
            goto next_command;
        }
        int32_t previousCmdNumber = coduo_int32_from_bits(
            (uint32_t)cmdNumber - 1u);
        if (cgame_syscall(CG_GET_USER_CMD, previousCmdNumber, oldCmd) == 0) {
            goto next_command;
        }

        if (cg_predictedPlayerState.commandTime == oldPlayerState.commandTime) {
            vec3_t adjustedOrigin;
            vec3_t moverAngleDelta;
            CG_AdjustPositionForMover(cg_predictedPlayerState.psOrigin,
                                      cg_predictedPlayerState.groundEntityNum,
                                      cg_latestSnapshotTime, cg_physicsTime,
                                      adjustedOrigin, moverAngleDelta);

            /* 0x30035b41-0x30035b6d: apply the mover's yaw delta to the
             * predicted view: FLD moverAngleDelta[1] ([ESP+0x70] while the
             * three call args are still pushed = the angle-delta block's
             * second float); FMUL [0x3007bd60]; ftol (0x3006be3c); AND 0xffff;
             * ADD into [0x30483214] = cg_predictedPlayerState.deltaAngles[1]. */
            int32_t yawDeltaShort =
                coduo_fp_to_i32_extended(
                    (long double)moverAngleDelta[1] *
                    (long double)ANGLE2SHORT_SCALE) &
                0xffff;
            cg_predictedPlayerState.deltaAngles[1] = coduo_int32_from_bits(
                (uint32_t)cg_predictedPlayerState.deltaAngles[1] +
                (uint32_t)yawDeltaShort);

            /* 0x30035b6f..0x30035bb5: the original developer diagnostic compares
             * all three adjusted-origin components against the previous predicted
             * origin and prints on any unequal (including unordered) comparison. */
            if (cg_showmiss_vmCvar.integer != 0 &&
                (oldPlayerState.psOrigin[0] != adjustedOrigin[0] ||
                 oldPlayerState.psOrigin[1] != adjustedOrigin[1] ||
                 oldPlayerState.psOrigin[2] != adjustedOrigin[2])) {
                Com_PrintMessage("prediction error\n");
            }

            vec3_t miss;
            miss[0] = (float)(
                (long double)oldPlayerState.psOrigin[0] -
                (long double)adjustedOrigin[0]);
            miss[1] = (float)(
                (long double)oldPlayerState.psOrigin[1] -
                (long double)adjustedOrigin[1]);
            /* Asymmetric per-component spill: miss[0]/miss[1] are FSTP'd and squared
             * as float*float (0x30035bc3/0x30035bd2), but miss[2] is FST-kept
             * (0x30035be1 FST [ESP+0x20]) so its square is the unrounded st0 times
             * the stored float copy. The sqrt is likewise FST-kept (0x30035bff) and
             * compared UNROUNDED against 0.1f (0x30035c03) -- both Class 8. */
            const long double miss2raw =
                (long double)oldPlayerState.psOrigin[2] -
                (long double)adjustedOrigin[2];
            miss[2] = (float)miss2raw;
            const long double missDistanceRaw =
                sqrtl((miss2raw * (long double)miss[2] +
                       (long double)miss[1] * (long double)miss[1]) +
                      (long double)miss[0] * (long double)miss[0]);
            const float missDistance = (float)missDistanceRaw;
            if (missDistanceRaw > 0.1f) {
                if (cg_showmiss_vmCvar.integer != 0) {
                    Com_PrintMessage("Prediction miss: %f\n",
                                     (double)missDistance);
                }

                if (cg_errordecay_vmCvar.integer != 0) {
                    const int32_t elapsed =
                        coduo_int32_from_bits(
                            cg_time - (uint32_t)cg_predictedErrorTime);
                    /* elapsed enters via bare FILD (0x30035c52) -- no (float) cast;
                     * cg.time deltas exceed 2^24 and an explicit cast would round
                     * (Class 4). */
                    long double decayRaw =
                        ((long double)cg_errordecay_vmCvar.value -
                         (long double)elapsed) /
                        (long double)cg_errordecay_vmCvar.value;
                    float decay = (float)decayRaw;
                    if (decayRaw < 0.0L) {
                        decay = 0.0f;
                    }
                    if (decay > 0.0f && cg_showmiss_vmCvar.integer != 0) {
                        Com_PrintMessage("Double prediction decay: %f\n",
                                         (double)decay);
                    }
                    cg_predictedError[0] = (float)(
                        (long double)cg_predictedError[0] *
                        (long double)decay);
                    cg_predictedError[1] = (float)(
                        (long double)cg_predictedError[1] *
                        (long double)decay);
                    /* The Z product at 0x30035cce remains in ST0 across the
                     * common X/Y tail and is not rounded before miss[2] is
                     * added. */
                    long double scaledZRaw =
                        (long double)cg_predictedError[2] *
                        (long double)decay;

                    float correctedX = (float)(
                        (long double)cg_predictedError[0] +
                        (long double)miss[0]);
                    int32_t errorTime = cg_physicsTime;
                    cg_predictedErrorTime = errorTime;
                    cg_predictedError[0] = correctedX;
                    cg_predictedError[1] = (float)(
                        (long double)cg_predictedError[1] +
                        (long double)miss[1]);
                    cg_predictedError[2] = (float)(
                        scaledZRaw + (long double)miss[2]);
                } else {
                    long double scaledZRaw = 0.0f;
                    cg_predictedError[1] = 0.0f;
                    cg_predictedError[0] = 0.0f;

                    float correctedX = (float)(
                        (long double)cg_predictedError[0] +
                        (long double)miss[0]);
                    int32_t errorTime = cg_physicsTime;
                    cg_predictedErrorTime = errorTime;
                    cg_predictedError[0] = correctedX;
                    cg_predictedError[1] = (float)(
                        (long double)cg_predictedError[1] +
                        (long double)miss[1]);
                    cg_predictedError[2] = (float)(
                        scaledZRaw + (long double)miss[2]);
                }
            }
        }

        /* 0x30035d2a: gate reads the cached pmove_fixed copy in cg_pmove (+0xf8);
         * the divisor re-reads pmove_msec.integer (0x30035d38 MOV ECX,[0x304504ac]). */
        if (cg_pmove.pmove_msec_min != 0) {
            int32_t pmoveMsec = pmove_msec.integer;
            int32_t roundedNumerator = coduo_int32_from_bits(
                (uint32_t)cmd->commandTime + (uint32_t)pmoveMsec - 1u);
            int32_t quotient = roundedNumerator / pmoveMsec;
            cmd->commandTime = coduo_int32_from_bits(
                (uint32_t)quotient * (uint32_t)pmoveMsec);
        }

        if (cg_norender_vmCvar.integer != 0) {
            cmd->angles[0] = oldCmd->angles[0];
            cmd->angles[1] = oldCmd->angles[1];
            cmd->angles[2] = oldCmd->angles[2];
            cmd->buttons = 0;
            cmd->wbuttons &= PM_WBUTTON_STANCE_MASK;
            cmd->forwardmove = 0;
            cmd->rightmove = 0;
            cmd->upmove = 0;
            int32_t commandAdvance = coduo_int32_from_bits(
                (uint32_t)cmd->commandTime -
                (uint32_t)cg_predictedPlayerState.commandTime);
            if (commandAdvance > 1) {
                cmd->commandTime = coduo_int32_from_bits(
                    (uint32_t)cg_predictedPlayerState.commandTime + 1u);
            }
        }

        /* 0x30035dba-0x30035e1a: the overlap-trace bounds are STACK temporaries
         * ([ESP+0x54..0x5c] mins, [ESP+0x44..0x4c] maxs), copied from
         * ps.playerMins/playerMaxs with the prone/duck maxs[2] override applied
         * on the stack slot. The machine never writes cg_pmove.mins (+0xd8) or
         * cg_pmove.maxs (+0xe4); Pmove sees whatever it last set there. */
        vec3_t traceMins, traceMaxs;
        traceMins[0] = cg_predictedPlayerState.playerMins[0];
        traceMins[1] = cg_predictedPlayerState.playerMins[1];
        traceMins[2] = cg_predictedPlayerState.playerMins[2];
        traceMaxs[0] = cg_predictedPlayerState.playerMaxs[0];
        traceMaxs[1] = cg_predictedPlayerState.playerMaxs[1];
        traceMaxs[2] = cg_predictedPlayerState.playerMaxs[2];
        if ((cg_predictedPlayerState.playerStateFlags & PMF_PRONE) != 0) {
            traceMaxs[2] = 30.0f;   /* 0x30035e03: 0x41f00000 */
        } else if ((cg_predictedPlayerState.playerStateFlags &
                    PS_FLAG_DUCKED) != 0) {
            traceMaxs[2] = 50.0f;   /* 0x30035e12: 0x42480000 */
        }

        trace_t overlap;
        CG_TraceCapsule(&overlap,
                           cg_predictedPlayerState.psOrigin,
                           traceMins, traceMaxs,
                           cg_predictedPlayerState.psOrigin,
                           cg_predictedPlayerState.psClientNum,
                           cg_pmove.traceMask);

        if (overlap.allsolid != 0 || overlap.startsolid != 0) {
            centity_t *hitEntity = cgame_compat_unchecked_cgentity(
                (int32_t)overlap.entityNum);
            if (hitEntity->currentValid != 0 &&
                hitEntity->currentState.eType == PMOVE_VEHICLE_TYPE) {
                vec3_t away = {
                    cg_predictedPlayerState.psOrigin[0] -
                        hitEntity->lerpOrigin[0],
                    cg_predictedPlayerState.psOrigin[1] -
                        hitEntity->lerpOrigin[1],
                    cg_predictedPlayerState.psOrigin[2] -
                        hitEntity->lerpOrigin[2]
                };
                (void)VectorNormalize(away);

                vec3_t velocityDirection;
                const float velocitySpeed =
                    VectorNormalize2(cg_predictedPlayerState.velocity,
                                     velocityDirection);
                /* horizontalDot is kept in st0 and compared UNROUNDED against
                 * 0.80000001f (FCOMP at 0x30035eef); a float local would round it. */
                const long double horizontalDot =
                    (long double)velocityDirection[0] *
                        (long double)away[0] +
                    (long double)velocityDirection[1] *
                        (long double)away[1];
                if (velocitySpeed < 1.0f || horizontalDot > 0.80000001f) {
                    hitEntity->predictionCollisionActive = qtrue;
                    predictionCollisionEntity = hitEntity;

                    if ((cg_predictedPlayerState.playerStateFlags &
                         PMF_PRONE) == 0 && velocitySpeed < 1.0f) {
                        cg_predictedPlayerState.velocity[0] = away[0] * 200.0f;
                        cg_predictedPlayerState.velocity[1] = away[1] * 200.0f;
                        cg_predictedPlayerState.velocity[2] = 0.0f;
                        cg_predictedPlayerState.pmTime = 200;
                        cg_predictedPlayerState.playerStateFlags |=
                            PS_FLAG_PREDICTION_PUSH;
                    }
                }
            }
        }

        Pmove(&cg_pmove);
        if (predictionCollisionEntity != NULL) {
            predictionCollisionEntity->predictionCollisionActive = qfalse;
        }
        CG_TouchTriggerPrediction();
        predictionRan = qtrue;

next_command:
        cmdNumber = coduo_int32_from_bits((uint32_t)cmdNumber + 1u);
    }

    /* Same import-validated currentWeapon cache refresh as the pre-loop store.
     * The binary reads the diagnostic cvar before publishing this pointer. */
    selectedWeapon = bg_weaponInfos[cg_predictedPlayerState.currentWeapon];
    int32_t verbosePrediction = cg_showmiss_vmCvar.integer;
    cg_currentWeaponInfo = selectedWeapon;
    if (verbosePrediction > 1) {
        Com_PrintMessage("[%i : %i] ", cmd->commandTime,
                         coduo_int32_from_bits(cg_time));
    }
    if (predictionRan == qfalse) {
        if (cg_showmiss_vmCvar.integer != 0) {
            Com_PrintMessage("no prediction run\n");
        }
        return;
    }

    vec3_t moverAngleDelta;
    /* 0x30035ff6: the FINAL mover adjustment interpolates to cg_time
     * (MOV EDX,[0x304831b0]) — the render timestamp — unlike the in-loop call
     * at 0x30035b1c, which passes cg_physicsTime ([0x304831b4]). */
    CG_AdjustPositionForMover(cg_predictedPlayerState.psOrigin,
                              cg_predictedPlayerState.groundEntityNum,
                              cg_latestSnapshotTime, coduo_int32_from_bits(cg_time),
                              cg_predictedPlayerState.psOrigin, moverAngleDelta);
    CG_TransitionPlayerState(&cg_predictedPlayerState, &oldPlayerState);
}
