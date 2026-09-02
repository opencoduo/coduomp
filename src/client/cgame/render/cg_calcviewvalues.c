// Source: uo_cgame_mp_x86.dll 0x30041a30..0x30041e1c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30041a30_30041e1c.mcode

#include "../client_recovered.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_CalcViewValues builds the current frame's renderer refdef and returns true
 * when either the view-origin collision trace clipped the camera or the final
 * view-projection pass reported its special mark-render result.
 *
 * The .mcode's PM_UFOMove label is rejected: PM_UFOMove is already proven at
 * 0x300098c0, while this body exclusively owns cg_refdef setup and dispatches the
 * locked, intermission, turret/vehicle, first-person, and third-person view
 * helpers. CG_CalcViewValues is the same-module PPC name whose behavior and call
 * graph match this dispatcher; no size correspondence is used.
 */

#define CG_BOB_CYCLE_DIVISOR        255.0f
#define CG_BOB_PHASE_TWO_PI           6.2831855f
#define CG_VIEW_BOB_VERTICAL_DELAY_MS 500
#define CG_VIEW_COLLISION_DOWN         8.0f
#define CG_VIEW_COLLISION_UP         200.0f

enum {
    /* playerStateFlags bit selecting the post-jump vertical-speed bob input. */
    CG_VIEW_BOB_VERTICAL_SPEED_FLAG = 0x10,
    CG_VIEW_COLLISION_MASK = 0x20,
    CG_VIEW_COLLISION_HANDLE = 32,
    CG_VIEW_TRACE_EXCLUDE_NONE = -1
};

static const vec3_t cg_viewCollisionMins = { -8.0f, -8.0f, -8.0f };
static const vec3_t cg_viewCollisionMaxs = {  8.0f,  8.0f,  8.0f };

qboolean CG_CalcViewValues(void)
{
    /* 0x30041a35..0x30041a41: exactly 20 dwords (sizeof refdef_t) cleared. */
    memset(&cg_refdef, 0, sizeof(cg_refdef));

    /* Locked/cube-face rendering owns a complete special refdef and never runs
     * the normal FOV/projection path. */
    if (cg_lockedViewFace != 0) {
        CG_BuildLockedViewRefdef();
        return qfalse;
    }

    CG_CalcVrect();

    if (g_cgScreenReadyState != 0) {
        Com_ErrorMessage(cg_cinematicCameraUnavailableMessage);
    }

    if (cg_predictedPlayerState.pmType == PM_TYPE_INTERMISSION) {
        vec3_t right;

        memcpy(cg_refdef.vieworg, cg_predictedPlayerState.psOrigin,
               sizeof(cg_refdef.vieworg));
        memcpy(cg_refdefViewAngles, cg_predictedPlayerState.viewAngles,
               sizeof(cg_refdefViewAngles));

        AngleVectors(cg_refdefViewAngles, cg_refdef.viewaxis[0], right,
                     cg_refdef.viewaxis[2]);
        cg_refdef.viewaxis[1][0] = -right[0];
        cg_refdef.viewaxis[1][1] = -right[1];
        cg_refdef.viewaxis[1][2] = -right[2];

        return CG_CalcViewProjection();
    }

    /* 0x30041b1d..0x30041ba6: cache the exact bob phase and the speed input used
     * by first-person/weapon bob. The bit-0x10 path suppresses it for 500 ms,
     * then uses vertical speed; the ordinary path uses horizontal magnitude. */
    {
        float bobByte = (float)(cg_predictedPlayerState.bobCycle & 255);
        cg_bobCyclePhase = (float)(
            ((long double)bobByte / (long double)CG_BOB_CYCLE_DIVISOR) *
            (long double)CG_BOB_PHASE_TWO_PI +
            (long double)CG_BOB_PHASE_TWO_PI);

        if ((cg_predictedPlayerState.playerStateFlags &
             CG_VIEW_BOB_VERTICAL_SPEED_FLAG) != 0) {
            int32_t elapsed = coduo_int32_from_bits(
                cg_time - (uint32_t)cg_predictedPlayerState.lastJumpCommandTime);
            cg_weaponMoveSpeed =
                (elapsed < CG_VIEW_BOB_VERTICAL_DELAY_MS)
                    ? 0.0f
                    : cg_predictedPlayerState.velocity[2];
        } else {
            float vx = cg_predictedPlayerState.velocity[0];
            float vy = cg_predictedPlayerState.velocity[1];
            /* 0x30041b91: _CIsqrt consumes the sum-of-squares as a raw st(0)
             * (80-bit, never rounded to float). sqrtl keeps the argument 80-bit;
             * the store to cg_weaponMoveSpeed (0x30041b96 FSTP DWORD) is the only
             * rounding to float. sqrtf would round the argument first. */
            cg_weaponMoveSpeed = (float)coduo_x87_sqrtl(
                (long double)vy * (long double)vy +
                (long double)vx * (long double)vx);
        }
    }

    /* Vehicle/turret angle state is updated before the predicted angles are
     * copied into the refdef; the callee may update playerState.viewAngles. */
    CG_CalcVehicleViewValues();
    memcpy(cg_refdef.vieworg, cg_predictedPlayerState.psOrigin,
           sizeof(cg_refdef.vieworg));
    memcpy(cg_refdefViewAngles, cg_predictedPlayerState.viewAngles,
           sizeof(cg_refdefViewAngles));

    /* 0x30041bb0..0x30041c9e: decay the prediction-origin error only for a
     * strictly interior fraction. The timestamp is cleared outside (0,1). */
    if (cg_errordecay_vmCvar.value > 0.0f) {
        int32_t elapsed = coduo_int32_from_bits(
            cg_time - (uint32_t)cg_predictedErrorTime);
        float elapsedFloat = (float)elapsed;
        float decay = (float)(
            ((long double)cg_errordecay_vmCvar.value -
             (long double)elapsedFloat) /
            (long double)cg_errordecay_vmCvar.value);

        if (decay > 0.0f && decay < 1.0f) {
            cg_refdef.vieworg[0] += cg_predictedError[0] * decay;
            cg_refdef.vieworg[1] += cg_predictedError[1] * decay;
            cg_refdef.vieworg[2] += cg_predictedError[2] * decay;
        } else {
            cg_predictedErrorTime = 0;
        }
    }

    CG_CalcTurretViewValues();

    /* 0x30041ca9..0x30041cdc: choose the final positional view offset. */
    if (cg_thirdPerson != 0) {
        CG_OffsetThirdPersonView();
    } else if ((cg_predictedPlayerState.entityStateFlags &
                EF_IN_VEHICLE) != 0 &&
               cg_predictedPlayerState.viewLockedEntityNum != ENTITYNUM_NONE) {
        CG_CalcVehicleViewPos();
    } else {
        CG_OffsetFirstPersonView();
    }

    qboolean cameraClipped = qfalse;

    /* Noclip bypasses the vertical view-origin collision adjustment. Otherwise
     * the marks/content probe at vieworg-8 gates an 8-unit box test 200 units
     * above the view. A hit (fraction < 1) replaces vieworg with trace.endpos. */
    if (cg_predictedPlayerState.pmType != PM_TYPE_NOCLIP) {
        vec3_t lowerPoint;
        lowerPoint[0] = cg_refdef.vieworg[0];
        lowerPoint[1] = cg_refdef.vieworg[1];
        lowerPoint[2] = cg_refdef.vieworg[2] - CG_VIEW_COLLISION_DOWN;

        if (CG_PointContents(lowerPoint,
                                        CG_VIEW_TRACE_EXCLUDE_NONE,
                                        CG_VIEW_COLLISION_MASK) != 0) {
            vec3_t upperPoint;
            trace_t trace;

            upperPoint[0] = cg_refdef.vieworg[0];
            upperPoint[1] = cg_refdef.vieworg[1];
            upperPoint[2] = cg_refdef.vieworg[2] + CG_VIEW_COLLISION_UP;

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            CG_Trace(CG_VIEW_COLLISION_HANDLE, upperPoint,
                     cg_viewCollisionMaxs,
                     &trace,
                     upperPoint,
                     cg_viewCollisionMins,
                     CG_VIEW_TRACE_EXCLUDE_NONE);

            if (trace.fraction < 1.0f) {
                memcpy(cg_refdef.vieworg, trace.endpos,
                       sizeof(cg_refdef.vieworg));
                cameraClipped = qtrue;
            }
        }
    }

    /* Build viewaxis from the final angles. AngleVectors returns `right`; the
     * renderer axis convention stores its negation as row 1. */
    {
        vec3_t right;
        AngleVectors(cg_refdefViewAngles, cg_refdef.viewaxis[0], right,
                     cg_refdef.viewaxis[2]);
        cg_refdef.viewaxis[1][0] = -right[0];
        cg_refdef.viewaxis[1][1] = -right[1];
        cg_refdef.viewaxis[1][2] = -right[2];
    }

    if (CG_CalcViewProjection() != qfalse || cameraClipped != qfalse) {
        return qtrue;
    }
    return qfalse;
}
