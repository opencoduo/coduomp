// Source: uo_cgame_mp_x86.dll 0x3003f610..0x3003f994
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f610_3003f994.mcode
//
// CG_OffsetThirdPersonView — position the ordinary chase camera behind the
// predicted player, clip it against world geometry, and aim it back at a focus
// point 512 units in front of the player.  The role is proved by the view-origin
// and view-angle dataflow and by its call from the third-person branch of the
// refdef builder at 0x30041cb1.  The size-only Scr_Vehicle_DamageScale guess is
// rejected; this function reads no vehicle damage state.
//
#include "../client_recovered.h"
#include "../globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stdint.h>

enum {
    CG_THIRD_PERSON_TRACE_MODEL = 49,
    CG_THIRD_PERSON_VERTICAL_TRACE_MODEL = 32,
    CG_THIRD_PERSON_TRACE_CONTENTS_0x20 = 0x20
};

static const float CG_THIRD_PERSON_MAX_FOCUS_PITCH = 45.0f;
static const float CG_THIRD_PERSON_FOCUS_DISTANCE = 512.0f;
static const float CG_THIRD_PERSON_CAMERA_RAISE = 8.0f;
static const float CG_THIRD_PERSON_COLLISION_LIFT = 32.0f;
static const float CG_THIRD_PERSON_VERTICAL_TRACE_HEIGHT = 1000.0f;
static const float CG_THIRD_PERSON_MIN_HORIZONTAL_DISTANCE = 1.0f;
static const float CG_DEGREES_TO_RADIANS = 0.017453292f;
static const float CG_NEGATIVE_RADIANS_TO_DEGREES = -57.295776f;

void CG_OffsetThirdPersonView(void)
{
    static const vec3_t traceMins = {-4.0f, -4.0f, -4.0f};
    static const vec3_t traceMaxs = {4.0f, 4.0f, 4.0f};
    vec3_t focusAngles;
    vec3_t focusForward;
    vec3_t focusPoint;
    vec3_t camera;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    trace_t trace;
    float horizontalDistance;

    /* Start at eye height.  Preserve the incoming pitch for the focus point;
     * death view changes only the focus yaw before the chase offset is built. */
    cg_refdef.vieworg[2] += cg_predictedPlayerState.viewHeightCurrent;
    focusAngles[0] = cg_refdefViewAngles[0];
    focusAngles[1] = cg_refdefViewAngles[1];
    focusAngles[2] = 0.0f;
    if (cg_predictedPlayerState.pmType >= PM_TYPE_DEAD) {
        focusAngles[1] = (float)cg_predictedPlayerState.stats[STAT_DEAD_YAW];
        cg_refdefViewAngles[1] = focusAngles[1];
    }
    if (focusAngles[0] > CG_THIRD_PERSON_MAX_FOCUS_PITCH) {
        focusAngles[0] = CG_THIRD_PERSON_MAX_FOCUS_PITCH;
    }

    /* 0x3003f678..0x3003f76e inlines the two sin/cos pairs instead of calling
     * AngleVectors.  Its Z convention is -sin(pitch). */
    {
        float yaw = focusAngles[1] * CG_DEGREES_TO_RADIANS;
        float pitch = focusAngles[0] * CG_DEGREES_TO_RADIANS;
        float sinYaw;
        float cosYaw;
        float sinPitch;
        float cosPitch;

        /* The original computes yaw first, pitch second, with one FSINCOS for
         * each stored float argument. */
        coduo_x87_sincosf(yaw, &sinYaw, &cosYaw);
        coduo_x87_sincosf(pitch, &sinPitch, &cosPitch);
        focusForward[0] = cosPitch * cosYaw;
        focusForward[1] = cosPitch * sinYaw;
        focusForward[2] = -sinPitch;
    }
    focusPoint[0] = cg_refdef.vieworg[0] + focusForward[0] * CG_THIRD_PERSON_FOCUS_DISTANCE;
    focusPoint[1] = cg_refdef.vieworg[1] + focusForward[1] * CG_THIRD_PERSON_FOCUS_DISTANCE;
    focusPoint[2] = cg_refdef.vieworg[2] + focusForward[2] * CG_THIRD_PERSON_FOCUS_DISTANCE;

    camera[0] = cg_refdef.vieworg[0];
    camera[1] = cg_refdef.vieworg[1];
    camera[2] = cg_refdef.vieworg[2] + CG_THIRD_PERSON_CAMERA_RAISE;

    cg_refdefViewAngles[0] *= 0.5f;
    cg_refdefViewAngles[1] -= cg_thirdPersonAngle_vmCvar.value;
    AngleVectors(cg_refdefViewAngles, forward, right, up);

    camera[0] -= cg_thirdPersonRange_vmCvar.value * forward[0];
    camera[1] -= cg_thirdPersonRange_vmCvar.value * forward[1];
    camera[2] -= cg_thirdPersonRange_vmCvar.value * forward[2];

    CG_Trace(CG_THIRD_PERSON_TRACE_MODEL, camera, traceMaxs, &trace, cg_refdef.vieworg, traceMins, cg_predictedPlayerState.psClientNum);

    if (trace.fraction != 1.0f) {
        camera[0] = trace.endpos[0];
        camera[1] = trace.endpos[1];
        camera[2] = trace.endpos[2] + (1.0f - trace.fraction) * CG_THIRD_PERSON_COLLISION_LIFT;

        CG_Trace(CG_THIRD_PERSON_TRACE_MODEL, camera, traceMaxs, &trace, cg_refdef.vieworg, traceMins, cg_predictedPlayerState.psClientNum);
        camera[0] = trace.endpos[0];
        camera[1] = trace.endpos[1];
        camera[2] = trace.endpos[2];

        if (trace.allsolid != 0 && (trace.contents & CG_THIRD_PERSON_TRACE_CONTENTS_0x20) != 0) {
            vec3_t lifted;
            lifted[0] = camera[0];
            lifted[1] = camera[1];
            lifted[2] = camera[2] + CG_THIRD_PERSON_VERTICAL_TRACE_HEIGHT;
            CG_Trace(CG_THIRD_PERSON_VERTICAL_TRACE_MODEL, camera, traceMaxs, &trace, lifted, traceMins,
                     cg_predictedPlayerState.psClientNum);
            camera[0] = trace.endpos[0];
            camera[1] = trace.endpos[1];
            camera[2] = trace.endpos[2];
        }
    }

    cg_refdef.vieworg[0] = camera[0];
    cg_refdef.vieworg[1] = camera[1];
    cg_refdef.vieworg[2] = camera[2];

    focusPoint[0] -= camera[0];
    focusPoint[1] -= camera[1];
    focusPoint[2] -= camera[2];
    /* 0x3003f947: _CIsqrt consumes the sum-of-squares as a raw st(0) (80-bit,
     * not rounded to float); the x87 adapter keeps the argument in ST0 and the
     * store to horizontalDistance (0x3003f94c FSTP DWORD) is the only rounding. */
    horizontalDistance = (float)coduo_x87_sqrtl((long double)focusPoint[0] * focusPoint[0] + (long double)focusPoint[1] * focusPoint[1]);
    if (horizontalDistance < CG_THIRD_PERSON_MIN_HORIZONTAL_DISTANCE) {
        horizontalDistance = CG_THIRD_PERSON_MIN_HORIZONTAL_DISTANCE;
    }
    {
        /* _CIatan2 consumes both float loads in x87 precision, then the
         * original explicitly rounds its result to float before scaling. */
        float pitchRadians = (float)coduo_x87_atan2l((long double)focusPoint[2], (long double)horizontalDistance);
        cg_refdefViewAngles[0] = pitchRadians * CG_NEGATIVE_RADIANS_TO_DEGREES;
    }
}
