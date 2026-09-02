#include "bg_pmove.h"

#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "qcommon/entity_event_types.h"
#include "math/q_math.h"

#include <stddef.h>
#include <stdint.h>

enum {
    ADS_POINT_CONTENTS_MASK = 0x00404000u,
    ADS_POINT_CONTENTS_CROUCH = 0x00400000u,
    VEHICLE_POSITION_ADS_SNAP = 1
};

/*
 * Complete ADS-request and ADS-fraction state cluster.  The authoritative
 * Windows cgame/game bodies are instruction-identical apart from globals,
 * relocations, and callee addresses:
 *
 *   PM_UpdateAimDownSightFlag  0x30011b60 / 0x20011aa0
 *   PM_ClearAimDownSightFlag   0x30011f40 / 0x20011e80
 *   PM_UpdateAimDownSightLerp  0x30011f50 / 0x20011e90
 *
 * The Linux game bodies at RVAs 0x00032c0b, 0x0003346f, and 0x0003349e
 * preserve the same decisions and stored binary32 results.  Its unoptimized
 * bodies expose calls which MSVC inlined, but do not establish a different
 * source contract.  EMULATE_X87 is therefore an execution-mode choice, not a
 * Windows/Linux behavior split.
 */

void PM_UpdateAimDownSightFlag(void)
{
    playerState_t *const ps = pm->ps;
    const weaponInfo_t *weaponInfo;

    if ((ps->playerStateFlags & PMF_FOLLOW) != 0) {
        return;
    }

    if (ps->pmType < PM_TYPE_DEAD && (pm->command.buttons & PM_BUTTON_ADS) != 0 && (ps->entityStateFlags & EF_IN_VEHICLE) != 0) {
        ps->playerStateFlags |= PMF_ADS;
        goto publish_condition;
    }

    weaponInfo = pml.weaponInfo;
    if (ps->pmType >= PM_TYPE_DEAD || (pm->command.buttons & PM_BUTTON_ADS) == 0 || weaponInfo->adsEnabled == 0 ||
        ps->weaponState == WEAPON_STATE_RAISING || ps->weaponState == WEAPON_STATE_DROPPING ||
        ps->weaponState == WEAPON_STATE_MELEE_WINDUP || ps->weaponState == WEAPON_STATE_MELEE_RELAX ||
        (ps->weaponState == WEAPON_STATE_BREAKING_DOWN && ps->currentWeapon != (int32_t)(uint8_t)pm->command.weapon) ||
        (pml.groundLiftFlag == 0 && ps->pmType != PM_TYPE_LINKED)) {
        ps->playerStateFlags &= ~PMF_ADS;
        goto publish_condition;
    }

    weaponInfo = BG_GetInfoForWeapon(ps->currentWeapon);
    if (weaponInfo->weaponClass != WEAPCLASS_LMG) {
        if ((ps->playerStateFlags & PMF_PRONE) != 0) {
            /* Both Windows modules read oldCommand.buttons but the current
             * command's movement bytes here. */
            if ((pm->oldCommand.buttons & PM_BUTTON_ADS) != 0 && (pm->command.forwardmove != 0 || pm->command.rightmove != 0)) {
                goto publish_condition;
            }
            ps->playerStateFlags |= PMF_ADS | PMF_PRONE_MOVEMENT_OVERRIDE;
        } else {
            ps->playerStateFlags |= PMF_ADS;
        }
        goto publish_condition;
    }

    if ((ps->playerStateFlags & PMF_ADS) != 0) {
        goto publish_condition;
    }

    {
        vec3_t yawAngles = {0.0f, ps->viewAngles[1], 0.0f};
        vec3_t forward;
        vec3_t probe;
        int32_t contents;

        AngleVectors(yawAngles, forward, NULL, NULL);
#if EMULATE_X87
        for (int32_t lane = 0; lane < 3; ++lane) {
            probe[lane] =
                x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(forward[lane]), x87f_load_f32(15.0f)), x87f_load_f32(ps->psOrigin[lane])));
        }
#else
        probe[0] = forward[0] * 15.0f + ps->psOrigin[0];
        probe[1] = forward[1] * 15.0f + ps->psOrigin[1];
        probe[2] = forward[2] * 15.0f + ps->psOrigin[2];
#endif
        /* The original stores the Z sum before performing this addition. */
        probe[2] += 1.0f;

        contents = pm->pointContents(probe, ps->psClientNum, ADS_POINT_CONTENTS_MASK);
        if (contents != 0) {
            ps->proneDirection = ps->viewAngles[1];
            ps->playerStateFlags |= PMF_ADS;
            if ((contents & ADS_POINT_CONTENTS_CROUCH) != 0) {
                ps->playerStateFlags |= PMF_DUCKED;
            } else {
                ps->playerStateFlags &= ~PMF_DUCKED;
            }
            ps->playerStateFlags &= ~PMF_PRONE;
            goto publish_condition;
        }
    }

    if (ps->serverCursorHint == CURSOR_HINT_LMG) {
        ps->proneDirection = ps->viewAngles[1];
        ps->playerStateFlags |= PMF_ADS;
        if (ps->serverCursorHintVal != 0) {
            ps->playerStateFlags |= PMF_DUCKED;
        } else {
            ps->playerStateFlags &= ~PMF_DUCKED;
        }
        ps->playerStateFlags &= ~PMF_PRONE;
        goto publish_condition;
    }

    if ((ps->playerStateFlags & PMF_PRONE) != 0 ||
        (ps->groundEntityNum != ENTITYNUM_NONE &&
         BG_CheckProneValid(ps->psClientNum, ps->psOrigin, pm->maxs[0], 30.0f, ps->viewAngles[1], &ps->torsoHeight, &ps->torsoPitch,
                            &ps->waistPitch, qfalse, qtrue, NULL, pm->trace3, pm->trace2, qfalse, 60.0f, qtrue, pm->entityType) != 0)) {
        if ((ps->playerStateFlags & (PMF_PRONE | PMF_ADS)) == 0) {
            ps->proneDirection = ps->viewAngles[1];
        }
        ps->playerStateFlags |= PMF_PRONE | PMF_ADS;
        goto publish_condition;
    }

    ps->playerStateFlags |= PMF_PRONE_BLOCKED;
    if ((pm->command.wbuttons & PM_WBUTTON_STANCE_LATCH) == 0) {
        PM_AddEvent((ps->playerStateFlags & PMF_DUCKED) != 0 ? EV_STANCE_FORCE_CROUCH : EV_STANCE_FORCE_STAND);
    }

publish_condition:
    BG_UpdateConditionValue(ps->psClientNum, ANIM_COND_WEAPON_POSITION, (ps->playerStateFlags & PMF_ADS) != 0, qtrue);
}

void PM_ClearAimDownSightFlag(void)
{
    pm->ps->playerStateFlags &= ~PMF_ADS;
}

void PM_UpdateAimDownSightLerp(void)
{
    playerState_t *const ps = pm->ps;
    const weaponInfo_t *const weaponInfo = pml.weaponInfo;
    const qboolean inVehicle = (ps->entityStateFlags & EF_IN_VEHICLE) != 0 ? qtrue : qfalse;
    qboolean wantAds = qfalse;

    if ((ps->playerStateFlags & PMF_FOLLOW) != 0) {
        return;
    }
    if (weaponInfo->adsEnabled == 0 && inVehicle == qfalse) {
        ps->adsFraction = 0.0f;
        return;
    }

    if (inVehicle != qfalse && (ps->playerStateFlags & PMF_ADS) != 0) {
        wantAds = qtrue;
    } else {
        qboolean allowAds = qtrue;
        const int32_t weaponState = ps->weaponState;

        if (weaponInfo->segmentedReload == 0) {
            if (weaponState == WEAPON_STATE_RELOADING &&
                coduo_int32_from_bits((uint32_t)ps->weaponTime - (uint32_t)weaponInfo->reloadLoopTime) > 0 &&
                weaponInfo->weaponClass != WEAPCLASS_LMG) {
                allowAds = qfalse;
            }
        } else if (weaponInfo->weaponClass != WEAPCLASS_LMG) {
            if (weaponState == WEAPON_STATE_RELOADING || weaponState == WEAPON_STATE_RELOADING_INTERRUPT ||
                weaponState == WEAPON_STATE_RELOAD_START || weaponState == WEAPON_STATE_RELOAD_START_INTERRUPT ||
                (weaponState == WEAPON_STATE_RELOAD_END &&
                 coduo_int32_from_bits((uint32_t)ps->weaponTime - (uint32_t)weaponInfo->reloadLoopTime) > 0)) {
                allowAds = qfalse;
            }
        }

        if (allowAds != qfalse && weaponInfo->adsRaiseEnabled == 0 && weaponState == WEAPON_STATE_RECHAMBERING) {
            allowAds = qfalse;
        }
        if (allowAds != qfalse && (ps->playerStateFlags & PMF_ADS) != 0) {
            wantAds = qtrue;
        }
    }

    if (weaponInfo->adsFireDelayEnabled != 0 && ps->weaponDelay != 0 && ps->weaponState == WEAPON_STATE_FIRING) {
        wantAds = qtrue;
    }

    if ((wantAds != qfalse && ps->adsFraction == 1.0f) || (wantAds == qfalse && ps->adsFraction == 0.0f)) {
        return;
    }

    if (inVehicle != qfalse && ps->vehiclePosition == VEHICLE_POSITION_ADS_SNAP) {
        ps->adsFraction = wantAds != qfalse ? 1.0f : 0.0f;
    } else if (wantAds != qfalse) {
#if EMULATE_X87
        ps->adsFraction = x87f_store_f32(
            x87f_add(x87f_load_f32(ps->adsFraction), x87f_mul(x87f_load_i32(pml.msec), x87f_load_f32(weaponInfo->adsFireDelayRate))));
#else
        ps->adsFraction = (float)((long double)ps->adsFraction + (long double)pml.msec * (long double)weaponInfo->adsFireDelayRate);
#endif
    } else {
#if EMULATE_X87
        ps->adsFraction = x87f_store_f32(
            x87f_sub(x87f_load_f32(ps->adsFraction), x87f_mul(x87f_load_i32(pml.msec), x87f_load_f32(weaponInfo->adsFireDelayOutRate))));
#else
        ps->adsFraction = (float)((long double)ps->adsFraction - (long double)pml.msec * (long double)weaponInfo->adsFireDelayOutRate);
#endif
    }

    if (ps->adsFraction >= 1.0f) {
        ps->adsFraction = 1.0f;
    } else if (ps->adsFraction <= 0.0f) {
        ps->adsFraction = 0.0f;
    }
}
