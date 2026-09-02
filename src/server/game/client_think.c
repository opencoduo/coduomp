/*
 * Source reconstruction for client think processing.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/libm/coduo_libm.h"

/* Values observed in the original ClientThink_real instruction stream. */
#define CLIENT_THINK_TIME_CLAMP_MAX 200
#define CLIENT_THINK_TIME_CLAMP_MIN (-1000)
#define CLIENT_THINK_CROUCH_MAX_Z 50.0f
#define CLIENT_THINK_PRONE_MAX_Z 30.0f
#define CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED 200.0f
#define CLIENT_THINK_VEHICLE_DAMAGE 10
#define CLIENT_THINK_VEHICLE_DAMAGE_COOLDOWN_MS 500
#define CLIENT_THINK_ACTIVATE_BUTTON PM_BUTTON_ACTIVATE
#define CLIENT_THINK_VIEW_CLAMP_PITCH_DELTA 60.0f
#define CLIENT_THINK_VIEW_CLAMP_YAW_DELTA 70.0f
#define CLIENT_THINK_VEHICLE_KNOCKBACK_FLAG 0x00000200u
#define CLIENT_THINK_BUTTON_USE PM_BUTTON_ACTIVATE
#define CLIENT_THINK_SHELLSHOCK_SWAY_BLEND_MS 3000
#define CLIENT_THINK_SHELLSHOCK_SWAY_BLEND_LAST_MS (CLIENT_THINK_SHELLSHOCK_SWAY_BLEND_MS - 1)

/* ------------------------------------------------------------------ */
/*  0x40c91  ClientThink_real                                         */
/* ------------------------------------------------------------------ */

/*
 * Process client movement and actions for the current frame.
 *
 * This is the core client processing function that handles:
 *  - Session state routing (intermission, spectator, normal)
 *  - Time clamping and pmove_msec adjustment
 *  - Inactivity timer checks
 *  - Vehicle collision detection
 *  - Weapon sway and positioning calculations
 *  - Pmove execution for movement physics
 *  - Client events and trigger touches
 *  - Entity impacts and state updates
 *
 * RECOVERED(UO-GAME-UNK-0165): This function is extremely complex (4225 bytes).
 * The reconstruction identifies major sections and uses named constants.
 *
 * VERIFIED_DECOMPILER(0x40c91, 50c91_ClientThink_real.c, VERIFY-P1-CLIENTTHINK-2026-06-17): DATAFLOW_VERIFIED - command-time writeback, pmove setup order, button latches, weapon/view angle stack reuse, vehicle collision, weapon-context copyback, Pmove, entity-state export, linking, trigger touch, impacts, and activate handling checked against current decompiler output.
 */
void ClientThink_real(gentity_t *ent, usercmd_t *command)
{
    gclient_t *client = ent->client;
    int time;
    int msec;
    int entityClientNum;
    pmove_t pm;
    int oldEventSequence;
    gentity_t *vehicleUnlinkedForPmove;
    vec3_t viewKickAngles;
    vec3_t muzzleAngles;
    pm_weapon_angle_state_t weaponContext;

    if (client->connectedState != CON_CONNECTED) {
        return;
    }

    /* Get command time and clamp to prevent large jumps */
    time = command->commandTime;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (coduo_int32_from_bits((uint32_t)level.time + (uint32_t)CLIENT_THINK_TIME_CLAMP_MAX) < time) {
        time = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)CLIENT_THINK_TIME_CLAMP_MAX);
    }
    if (time < coduo_int32_from_bits((uint32_t)level.time + (uint32_t)CLIENT_THINK_TIME_CLAMP_MIN)) {
        time = coduo_int32_from_bits((uint32_t)level.time + (uint32_t)CLIENT_THINK_TIME_CLAMP_MIN);
    }
    command->commandTime = time;

    /* Calculate msec delta from last command */
    msec = coduo_int32_from_bits((uint32_t)time - (uint32_t)client->ps.commandTime);
    entityClientNum = (int)(ent - g_entities);

    /* Validate msec and check for client number mismatch */
    if ((0 < msec) || (client->ps.psClientNum != entityClientNum)) {
        /* Clamp msec to maximum */
        if (CLIENT_THINK_TIME_CLAMP_MAX < msec) {
            msec = CLIENT_THINK_TIME_CLAMP_MAX;
        }

        /* Validate and adjust pmove_msec cvar */
        if (pmove_msec.integer < PMOVE_MSEC_MIN) {
            trap_Cvar_Set("pmove_msec", "8");
        } else if (PMOVE_MSEC_MAX < pmove_msec.integer) {
            trap_Cvar_Set("pmove_msec", "33");
        }

        /* Apply pmove_fixed timing if enabled */
        if (pmove_fixed.integer != 0 || client->pmoveFixed != 0) {
            int32_t fixedNumerator = coduo_int32_from_bits((uint32_t)pmove_msec.integer + (uint32_t)time - UINT32_C(1));
            int32_t fixedQuotient = fixedNumerator / pmove_msec.integer;

            time = coduo_int32_from_bits((uint32_t)fixedQuotient * (uint32_t)pmove_msec.integer);
        }
        command->commandTime = time;

        /* Route to appropriate think handler based on session state */
        if (client->sessionState == SESS_STATE_INTERMISSION) {
            ClientIntermissionThink(ent, command);
            return;
        }

        /* Check spectator inactivity */
        if (ClientSpectatorInactivityTimer(client) == 0) {
            return;
        }
        if (client->sessionState == SESS_STATE_SPECTATOR) {
            SpectatorThink(ent, command);
            return;
        }

        /* Check general inactivity */
        if (ClientInactivityTimer(client) == 0) {
            return;
        }

        /* Save old event sequence for ClientEvents and link-time updates. */
        oldEventSequence = client->ps.eventIndex;
        vehicleUnlinkedForPmove = NULL;

        /* Clear and initialize pmove structure */
        memset(&pm, 0, sizeof(pmove_t));

        /* Set up pmove fields */
        pm.ps = &client->ps;
        pm.command = *command;
        pm.command.commandTime = time;
        pm.oldCommand = client->oldPmoveCommand;
        pm.traceMask = (client->ps.pmType < PM_TYPE_DEAD) ? MASK_PLAYERSOLID : MASK_DEADSOLID;
        pm.trace = trap_TraceCapsule;
        pm.trace2 = trap_TraceCapsule;
        pm.trace3 = trap_TraceCapsule;
        pm.pointContents = trap_PointContents;
        pm.entityType = G_EntityType;
        pm.adsInputBlocked = G_IsInMatchTimeout();
        pm.debugMove = g_debugMove.integer;
        if ((client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
            gentity_t *controlledVehicle = &g_entities[ent->passEntityNum];
            const vehicle_state_t *vehicleState = (const vehicle_state_t *)controlledVehicle->vehicle;

            pm.viewClampTargetAngles[0] = vehicleState->viewClampTargetAngles[0];
            pm.viewClampTargetAngles[1] = vehicleState->viewClampTargetAngles[1];
            pm.viewClampTargetAngles[2] = vehicleState->viewClampTargetAngles[2];
            if (client->ps.vehicleType == 1 && client->ps.vehiclePosition == 3) {
                pm.viewClampMaxDeltas[0] = CLIENT_THINK_VIEW_CLAMP_PITCH_DELTA;
                pm.viewClampMaxDeltas[1] = CLIENT_THINK_VIEW_CLAMP_YAW_DELTA;
            }
        }
        pm.pmove_msec_min = client->pmoveFixed | pmove_fixed.integer;
        pm.pmove_msec_max = pmove_msec.integer;

        client->spectatorSnapshotOrigin[0] = client->ps.psOrigin[0];
        client->spectatorSnapshotOrigin[1] = client->ps.psOrigin[1];
        client->spectatorSnapshotOrigin[2] = client->ps.psOrigin[2];

        client->oldButtons = client->currentButtons;
        client->currentButtons = client->command.buttons;
        if ((client->ps.entityStateFlags & EF_IN_VEHICLE) == 0 &&
            ((const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon))->weaponClass == WEAPCLASS_LMG &&
            (client->ps.playerStateFlags & PMF_ADS) != 0) {
            client->currentButtons &= ~CLIENT_THINK_BUTTON_USE;
        }
        client->latchedButtons = ~client->oldButtons & client->currentButtons;

        client->oldWbuttons = client->spectatorWbuttons;
        client->spectatorWbuttons = client->command.wbuttons;
        client->latchedWbuttons = ~client->oldWbuttons & client->spectatorWbuttons;

        /* Calculate view angles and weapon sway */
        const float clientSpeed = BG_GetSpeed(&client->ps, level.time);

        {
            bg_view_angle_state_t viewContext;

            viewContext.ps = &client->ps;
            viewContext.viewKickStartTime = client->damageTime;
            viewContext.time = level.time;
            viewContext.viewKickPitch = client->damagePitch;
            viewContext.viewKickRoll = client->damageRoll;
            viewContext.speed = clientSpeed;

            BG_CalculateViewAngles(&viewContext, viewKickAngles);
        }
        muzzleAngles[0] = client->ps.viewAngles[0] + viewKickAngles[0];
        muzzleAngles[1] = client->ps.viewAngles[1] + viewKickAngles[1];
        muzzleAngles[2] = client->ps.viewAngles[2] + viewKickAngles[2];
        {
            const weaponInfo_t *weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(client->ps.currentWeapon);
            int shellshockTimeRemaining =
                coduo_int32_from_bits((uint32_t)client->ps.motionState.shellshock.time +
                                      (uint32_t)client->ps.motionState.shellshock.duration - (uint32_t)level.time);
            float shellshockSwayScale;

            if (shellshockTimeRemaining > 0) {
                float shellshockFraction = 1.0f;

                if (shellshockTimeRemaining <= CLIENT_THINK_SHELLSHOCK_SWAY_BLEND_LAST_MS) {
                    /* The binary divides by 3000.0f (fdivp at 0x412bc), not
                     * a multiply by the reciprocal, and smooths as
                     * (3 - (f + f)) * f * f (0x412c4..0x412e4). */
                    shellshockFraction = (float)shellshockTimeRemaining / (float)CLIENT_THINK_SHELLSHOCK_SWAY_BLEND_MS;
                    shellshockFraction = (3.0f - (shellshockFraction + shellshockFraction)) * shellshockFraction * shellshockFraction;
                }

                shellshockSwayScale = 1.0f + (weaponInfo->swayShellShockScale - 1.0f) * shellshockFraction;
            } else {
                shellshockSwayScale = 1.0f;
            }

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            (void)shellshockSwayScale;
        }
        BG_CalculateWeaponPosition_Sway(&client->ps, client->weaponPreviousViewAngles, client->weaponSwayOffsets, client->weaponSwayAngles,
                                        1.0f, msec);
        {
            weaponContext.ps = &client->ps;
            weaponContext.speed = clientSpeed;
            weaponContext.frameTime = (float)((long double)msec * (long double)0.001f);
            weaponContext.moveOffset[0] = client->weaponMoveOffset[0];
            weaponContext.moveOffset[1] = client->weaponMoveOffset[1];
            weaponContext.moveOffset[2] = client->weaponMoveOffset[2];
            weaponContext.idleScale = client->weaponIdleScale;
            weaponContext.time = level.time;
            weaponContext.viewKickStartTime = client->damageTime;
            weaponContext.viewKickPitch = client->damagePitch;
            weaponContext.viewKickYaw = client->damageRoll;
            weaponContext.recoilPitch = client->weaponRecoilAngles[0];
            weaponContext.recoilYaw = client->weaponRecoilAngles[1];
            weaponContext.recoilRoll = client->weaponRecoilAngles[2];
            weaponContext.recoilPitchVelocity = client->fireRecoilVelocity[0];
            weaponContext.recoilYawVelocity = client->fireRecoilVelocity[1];
            weaponContext.weaponRecoilState = client->weaponRecoilState;
            weaponContext.baseAngles[0] = client->weaponSwayAngles[0];
            weaponContext.baseAngles[1] = client->weaponSwayAngles[1];

            /* 0x414c6 passes the same stack vector that received the view-kick
             * output; the weapon-angle call overwrites it in place. */
            BG_CalculateWeaponAngles(&weaponContext, viewKickAngles);
        }

        /* Handle ADS (aim-down-sight) angle composition */
        if (BG_IsAimDownSightWeapon(client->ps.currentWeapon) && (client->ps.adsFraction != 0.0f || isnan(client->ps.adsFraction))) {
            axis_t axis1;
            axis_t axis2;
            axis_t result;
            AnglesToAxis(viewKickAngles, axis1);
            AnglesToAxis(muzzleAngles, axis2);
            MatrixMultiply(axis1, axis2, result);
            /* C99 multidimensional-array qualifier bridge; AxisToAngles
             * retains a read-only view of result. */
            AxisToAngles((const vec_t(*)[3])result, muzzleAngles);
        }

        /* Vehicle collision detection only runs while not linked into a vehicle. */
        if ((client->ps.entityStateFlags & EF_IN_VEHICLE) == 0) {
            vec3_t mins, maxs;
            trace_t trace;

            /* Set up capsule bounds for player */
            mins[0] = client->ps.playerMins[0];
            mins[1] = client->ps.playerMins[1];
            mins[2] = client->ps.playerMins[2];
            maxs[0] = client->ps.playerMaxs[0];
            maxs[1] = client->ps.playerMaxs[1];
            maxs[2] = client->ps.playerMaxs[2];

            /* Adjust maxs based on stance */
            if ((client->ps.playerStateFlags & PMF_PRONE) == 0) {
                if ((client->ps.playerStateFlags & PMF_DUCKED) != 0) {
                    maxs[2] = CLIENT_THINK_CROUCH_MAX_Z;
                }
            } else {
                maxs[2] = CLIENT_THINK_PRONE_MAX_Z;
            }

            /* Trace capsule from player position */
            trap_TraceCapsule(&trace, client->ps.psOrigin, mins, maxs, client->ps.psOrigin, client->ps.psClientNum, pm.traceMask);

            if (trace.startsolid != 0 || trace.allsolid != 0) {
                gentity_t *hitEnt = &g_entities[trace.entityNum];
                if (hitEnt->s.eType == ET_VEHICLE) {
                    vec3_t away;
                    vec3_t velocityDir;
                    float playerSpeed;

                    away[0] = client->ps.psOrigin[0] - hitEnt->currentOrigin[0];
                    away[1] = client->ps.psOrigin[1] - hitEnt->currentOrigin[1];
                    away[2] = client->ps.psOrigin[2] - hitEnt->currentOrigin[2];
                    VectorNormalize(away);
                    playerSpeed = VectorNormalize2(client->ps.velocity, velocityDir);
                    away[2] = 0.0f;
                    velocityDir[2] = 0.0f;

                    /* The binary compares the full three-term dot (the z term
                     * is the zeroed products) straight from the x87 register
                     * against 0.8f (0x41723..0x41753); a float local would add
                     * a rounding it does not perform. */
#if EMULATE_X87
                    if (playerSpeed < 1.0f ||
                        x87f_lt(x87f_load_f32(0.8f), x87f_add(x87f_add(x87f_mul(x87f_load_f32(away[0]), x87f_load_f32(velocityDir[0])),
                                                                       x87f_mul(x87f_load_f32(away[1]), x87f_load_f32(velocityDir[1]))),
                                                              x87f_mul(x87f_load_f32(away[2]), x87f_load_f32(velocityDir[2]))))) {
#else
                    if (playerSpeed < 1.0f || away[0] * velocityDir[0] + away[1] * velocityDir[1] + away[2] * velocityDir[2] > 0.8f) {
#endif
                        vehicleUnlinkedForPmove = hitEnt;
                        trap_UnlinkEntity(hitEnt);

                        if ((client->ps.playerStateFlags & PMF_PRONE) == 0 && playerSpeed < 1.0f) {
                            vehicle_state_t *vehicleState = (vehicle_state_t *)hitEnt->vehicle;
#if EMULATE_X87
                            float vehicleSpeed = (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(
                                x87f_add(x87f_mul(x87f_load_f32(vehicleState->velocity[0]), x87f_load_f32(vehicleState->velocity[0])),
                                         x87f_mul(x87f_load_f32(vehicleState->velocity[1]), x87f_load_f32(vehicleState->velocity[1]))),
                                x87f_mul(x87f_load_f32(vehicleState->velocity[2]), x87f_load_f32(vehicleState->velocity[2])))));
#else
                            float vehicleSpeed = (float)CoduoLibm_Sqrt((double)(vehicleState->velocity[0] * vehicleState->velocity[0] +
                                                                                vehicleState->velocity[1] * vehicleState->velocity[1] +
                                                                                vehicleState->velocity[2] * vehicleState->velocity[2]));
#endif

                            if (vehicleSpeed > 1.0f) {
                                float knockbackSpeed;

                                VEH_PlayerCollision(hitEnt, ent);
#if EMULATE_X87
                                knockbackSpeed = (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(
                                    x87f_add(x87f_mul(x87f_load_f32(client->ps.velocity[0]), x87f_load_f32(client->ps.velocity[0])),
                                             x87f_mul(x87f_load_f32(client->ps.velocity[1]), x87f_load_f32(client->ps.velocity[1]))),
                                    x87f_mul(x87f_load_f32(client->ps.velocity[2]), x87f_load_f32(client->ps.velocity[2])))));
#else
                                knockbackSpeed = (float)CoduoLibm_Sqrt((double)(client->ps.velocity[0] * client->ps.velocity[0] +
                                                                                client->ps.velocity[1] * client->ps.velocity[1] +
                                                                                client->ps.velocity[2] * client->ps.velocity[2]));
#endif
                                if (knockbackSpeed < CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED) {
                                    VectorNormalize(client->ps.velocity);
                                    client->ps.velocity[0] *= CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                                    client->ps.velocity[1] *= CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                                    client->ps.velocity[2] *= CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                                }
                            } else {
                                client->ps.velocity[0] = away[0] * CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                                client->ps.velocity[1] = away[1] * CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                                client->ps.velocity[2] = away[2] * CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                            }

                            client->ps.pmTime = (int)CLIENT_THINK_VEHICLE_KNOCKBACK_SPEED;
                            client->ps.playerStateFlags |= CLIENT_THINK_VEHICLE_KNOCKBACK_FLAG;
                        }
                    }

                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    if ((client->ps.playerStateFlags & PMF_PRONE) != 0 &&
                        client->vehicleProneDamageTime <
                            coduo_int32_from_bits((uint32_t)level.time - (uint32_t)CLIENT_THINK_VEHICLE_DAMAGE_COOLDOWN_MS)) {
                        int oldHealth = ent->health;
                        VEH_PlayerDamage(ent, hitEnt, CLIENT_THINK_VEHICLE_DAMAGE);
                        /* If health didn't change, damage self (fallback) */
                        if (ent->health == oldHealth) {
                            VEH_PlayerDamage(ent, ent, CLIENT_THINK_VEHICLE_DAMAGE);
                        }
                    }
                }
            }
        }

        /* Execute movement physics */
        client->weaponMoveOffset[0] = weaponContext.moveOffset[0];
        client->weaponMoveOffset[1] = weaponContext.moveOffset[1];
        client->weaponMoveOffset[2] = weaponContext.moveOffset[2];
        client->weaponIdleScale = weaponContext.idleScale;
        client->weaponRecoilAngles[0] = weaponContext.recoilPitch;
        client->weaponRecoilAngles[1] = weaponContext.recoilYaw;
        client->weaponRecoilAngles[2] = weaponContext.recoilRoll;
        client->fireRecoilVelocity[0] = weaponContext.recoilPitchVelocity;
        client->fireRecoilVelocity[1] = weaponContext.recoilYawVelocity;
        client->weaponRecoilState = weaponContext.weaponRecoilState;
        client->spectatorSnapshotAngle0 = muzzleAngles[0];
        client->spectatorSnapshotAngle1 = muzzleAngles[1];
        Pmove(&pm);
        if (vehicleUnlinkedForPmove != NULL) {
            trap_LinkEntity(vehicleUnlinkedForPmove);
        }

        ent->s.turretOverheatState = (ent->client->ps.playerStateFlags & PMF_DUCKED) != 0;

        if (ent->client->ps.eventIndex != oldEventSequence) {
            ent->lastThinkTime = level.time;
            ent->eventTime2 = level.time;
        }

        /* Convert player state to entity state */
        if (g_smoothClients.integer == 0) {
            BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qtrue);
        } else {
            BG_PlayerStateToEntityStateExtrapolate(&ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue);
        }

        /* VERIFIED_DECOMPILER(0x40c91, 50c91_ClientThink_real.c, VERIFY-P1-CLIENTTHINK-2026-06-17): DATAFLOW_VERIFIED - after BG_PlayerStateToEntityState*, copy currentOrigin (+0x13c) from entity +0x18, which is pos.trBase[0] because gentity_t::pos starts at +0x0c and trajectory_t::trBase starts at +0x0c. */
        ent->currentOrigin[0] = ent->s.pos.trBase[0];
        ent->currentOrigin[1] = ent->s.pos.trBase[1];
        ent->currentOrigin[2] = ent->s.pos.trBase[2];
        memcpy(ent->mins, pm.mins, sizeof(ent->mins));
        memcpy(ent->maxs, pm.maxs, sizeof(ent->maxs));
        ent->clientSpawnStateByte = pm.watertype;
        ent->clientSpawnResetByte = pm.waterlevel;

        /* Process client events */
        ClientEvents(ent, (uint32_t)oldEventSequence);

        /* Link entity to world */
        trap_LinkEntity(ent);

        /* Touch triggers unless noclip is active. */
        if (ent->client->noclip == 0) {
            G_TouchTriggers(ent);
        }

        /* Copy final origin and angles to entity */
        ent->currentOrigin[0] = ent->client->ps.psOrigin[0];
        ent->currentOrigin[1] = ent->client->ps.psOrigin[1];
        ent->currentOrigin[2] = ent->client->ps.psOrigin[2];
        ent->currentAngles[0] = 0.0f;
        ent->currentAngles[1] = 0.0f;
        ent->currentAngles[2] = 0.0f;
        ent->currentAngles[1] = ent->client->ps.viewAngles[1]; /* Yaw from view angles */

        /* Process entity impacts */
        ClientImpacts(ent, &pm);

        if (ent->client->ps.eventIndex != oldEventSequence) {
            ent->lastThinkTime = level.time;
        }

        /* Handle activate button */
        if ((client->latchedButtons & CLIENT_THINK_ACTIVATE_BUTTON) != 0) {
            Cmd_Activate_f(ent);
        }
    }
}
