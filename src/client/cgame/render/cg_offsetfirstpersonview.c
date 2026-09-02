// Source: uo_cgame_mp_x86.dll 0x3003fb60..0x3003ffb7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003fb60_3003ffb7.mcode

#include "../client_recovered.h"
#include "../globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_OffsetFirstPersonView — add the first-person view-height, damage-kick,
 * movement-bob, lean, and impact-event offsets to the refdef origin and angles.
 *
 * The .mcode header's PM_Weapon_FinishWeaponChange label is a size-only guess and
 * is rejected. The sole call site is the first-person arm of CG_CalcViewValues,
 * the body reads/writes cg_refdef and cg_refdefViewAngles throughout, and the
 * same-module PPC name bank identifies this view helper as
 * CG_OffsetFirstPersonView.
 */

#define CG_DEG2RAD 0.01745329238474369f

enum {
    CG_FIRST_PERSON_TRACE_HANDLE = 17,
    CG_IMPACT_KICK_RISE_MS = 150,
    CG_IMPACT_KICK_FALL_MS = 300,
    CG_IMPACT_KICK_TOTAL_MS = 450
};

static const vec3_t cg_firstPersonTraceMins = {-8.0f, -8.0f, -8.0f};
static const vec3_t cg_firstPersonTraceMaxs = {8.0f, 8.0f, 8.0f};

void CG_OffsetFirstPersonView(void)
{
    bg_view_angle_state_t state;
    vec3_t offset = {0.0f, 0.0f, 0.0f};

    /* 0x3003fb60..0x3003fb74: the incoming snapshot's embedded pmType is the
     * early gate. The branch happens after the register saves in machine code,
     * but has no source-level side effect. */
    if (cg_nextSnap->ps.pmType == PM_TYPE_INTERMISSION) {
        return;
    }

    /* 0x3003fb7a..0x3003fbe4: the damage-kick time values are expressed relative
     * to the player-state tail time base. The first delta is zero when there has
     * not yet been a directional damage event. */
    state.ps = &cg_predictedPlayerState;
    state.viewKickStartTime = (cg_damageDirLatestServerTime != 0)
                                  ? (int32_t)((uint32_t)cg_damageDirLatestServerTime - (uint32_t)cg_predictedPlayerState.deltaTime)
                                  : 0;
    state.time = (int32_t)((uint32_t)cg_time - (uint32_t)cg_predictedPlayerState.deltaTime);
    state.viewKickPitch = cg_damageFlashScale;
    state.viewKickRoll = cg_damageFlashX;
    state.speed = cg_weaponMoveSpeed;

    BG_CalculateViewAngles(&state, offset);

    cg_refdefViewAngles[0] += offset[0];
    cg_refdefViewAngles[1] += offset[1];
    cg_refdefViewAngles[2] += offset[2];

    /* 0x3003fbf3..0x3003fc44: the in-vehicle bit substitutes a fixed 28-unit
     * view height. Any vehicle-bob flag then suppresses the remaining ordinary
     * first-person effects. */
    if ((cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) != 0) {
        cg_refdef.vieworg[2] += 28.0f;
    } else {
        cg_refdef.vieworg[2] += cg_predictedPlayerState.viewHeightCurrent;
    }

    if ((cg_predictedPlayerState.entityStateFlags & EF_RESTRICTED_MASK) != 0) {
        return;
    }

    /* 0x3003fc4a..0x3003fd44: while aiming a spread-class weapon, move a point
     * 19 units forward at full ADS and trace an 8-unit box from the eye to it.
     * The wrapper's +0x2e result byte gates whether its returned end position is
     * accepted. */
    if (bg_weaponInfos[cg_snap->ps.currentWeapon]->weaponClass == WEAPCLASS_LMG && cg_predictedPlayerState.adsFraction != 0.0f) {
        vec3_t angles = {0.0f, cg_predictedPlayerState.proneDirection, 0.0f};
        vec3_t forward;
        vec3_t traceEnd;
        trace_t trace;
        float distance;

        AngleVectors(angles, forward, NULL, NULL);
        distance = cg_predictedPlayerState.adsFraction * 19.0f;
        traceEnd[0] = cg_refdef.vieworg[0] + distance * forward[0];
        traceEnd[1] = cg_refdef.vieworg[1] + distance * forward[1];
        traceEnd[2] = cg_refdef.vieworg[2] + distance * forward[2];

        CG_Trace(CG_FIRST_PERSON_TRACE_HANDLE, traceEnd, cg_firstPersonTraceMaxs, &trace, cg_refdef.vieworg, cg_firstPersonTraceMins,
                 cg_snap->ps.psClientNum);

        if (trace.allsolid == 0) {
            memcpy(cg_refdef.vieworg, trace.endpos, sizeof(cg_refdef.vieworg));
        }
    }

    /* 0x3003fd4a..0x3003fec8: add vertical bob directly, then aim the horizontal
     * bob along the exact Euler-derived direction emitted by the three FSINCOS
     * pairs. */
    /* 0x3003fd69: the raw ST0 return is rounded to a float slot BEFORE the
     * vieworg[2] add (FSTP [ESP+0x18]; FADD; FSTP). */
    float verticalBob =
        (float)BG_GetVerticalBobFactor(&cg_predictedPlayerState, cg_bobCyclePhase, cg_weaponMoveSpeed, cg_bobMax_vmCvar.value);
    cg_refdef.vieworg[2] += verticalBob;

    {
        float horizontalBob =
            BG_GetHorizontalBobFactor(&cg_predictedPlayerState, cg_bobCyclePhase, cg_weaponMoveSpeed, cg_bobMax_vmCvar.value);
        float yaw = cg_refdefViewAngles[1] * CG_DEG2RAD;
        float pitch = cg_refdefViewAngles[0] * CG_DEG2RAD;
        float roll = cg_refdefViewAngles[2] * CG_DEG2RAD;
        float sinYaw;
        float cosYaw;
        float sinPitch;
        float cosPitch;
        float sinRoll;
        float cosRoll;
        float sinRollSinPitch;
        vec3_t bobDirection;

        coduo_x87_sincosf(yaw, &sinYaw, &cosYaw);
        coduo_x87_sincosf(pitch, &sinPitch, &cosPitch);
        coduo_x87_sincosf(roll, &sinRoll, &cosRoll);
        sinRollSinPitch = sinRoll * sinPitch;

        bobDirection[0] = cosRoll * sinYaw - sinRollSinPitch * cosYaw;
        bobDirection[1] = -cosRoll * cosYaw - sinRollSinPitch * sinYaw;
        bobDirection[2] = -sinRoll * cosPitch;

        cg_refdef.vieworg[0] += horizontalBob * bobDirection[0];
        cg_refdef.vieworg[1] += horizontalBob * bobDirection[1];
        cg_refdef.vieworg[2] += horizontalBob * bobDirection[2];
    }

    /* 0x3003fe37..0x3003ff53: the local impact kick rises for 150 ms and falls
     * for 300 ms. A backwards cg_time re-baselines the stored stamp to now-450,
     * while the current evaluation deliberately keeps the original negative
     * elapsed value. */
    {
        int32_t elapsed = coduo_int32_from_bits(cg_time - (uint32_t)cg_impactViewKickTime);
        float elapsedFloat = (float)elapsed;
        float phase;

        if (elapsedFloat < 0.0f) {
            cg_impactViewKickTime = coduo_int32_from_bits(cg_time - (uint32_t)CG_IMPACT_KICK_TOTAL_MS);
        }

        if (elapsedFloat < (float)CG_IMPACT_KICK_RISE_MS) {
            phase = elapsedFloat / (float)CG_IMPACT_KICK_RISE_MS;
            cg_refdef.vieworg[2] += cg_impactViewKick * phase;
        } else if (elapsedFloat < (float)CG_IMPACT_KICK_TOTAL_MS) {
            /* 0x3003ff25/0x3003ff2b: (elapsedFloat - 150.0f) is rounded to a float
             * slot and reloaded at 0x3003ff2f before the divide (Class 1). */
            float fallElapsed = elapsedFloat - (float)CG_IMPACT_KICK_RISE_MS;
            phase = 1.0f - fallElapsed / (float)CG_IMPACT_KICK_FALL_MS;
            cg_refdef.vieworg[2] += cg_impactViewKick * phase;
        }
    }

    CG_SettleViewOriginZ();

    AddLeanToPosition(cg_refdef.vieworg, cg_refdefViewAngles[1], cg_predictedPlayerState.leanFraction, 16.0f, 20.0f);

    /* 0x3003ff7f..0x3003ffad: never allow the final eye Z below origin.z+8. */
    {
        float minimumViewZ = cg_predictedPlayerState.psOrigin[2] + 8.0f;
        if (cg_refdef.vieworg[2] < minimumViewZ) {
            cg_refdef.vieworg[2] = minimumViewZ;
        }
    }
}
