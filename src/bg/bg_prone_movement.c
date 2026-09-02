#include "bg_pmove.h"

#include "bg_player_state.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stddef.h>

/*
 * Shared prone movement cluster.  The Windows cgame/game bodies are
 * instruction-identical apart from relocations and dependency addresses:
 *
 *   uo_cgame_mp_x86.dll  BG_CheckProne       0x30008130
 *   uo_game_mp_x86.dll   BG_CheckProne       0x20007ee0
 *   uo_cgame_mp_x86.dll  BG_CheckProneTurned 0x3000c800
 *   uo_game_mp_x86.dll   BG_CheckProneTurned 0x2000c5c0
 *   uo_cgame_mp_x86.dll  PM_UpdatePronePitch 0x3000d470
 *   uo_game_mp_x86.dll   PM_UpdatePronePitch 0x2000d220
 *
 * Linux game retains the same functions at RVAs 0x00022ff3, 0x0002b031,
 * and 0x0002c158.  The state gates, trace arguments, event writes, and angle
 * updates agree.  The two proven floating-point spill differences are kept at
 * their exact expression boundaries below; see the platform discrepancy note.
 */

int32_t BG_CheckProne(int32_t clientNum, const vec3_t origin,
                      float radius, float height, float yaw,
                      float *groundOffset, float *pitchDown,
                      float *pitchUp, qboolean skipInitialTrace,
                      qboolean allowFallback, const vec3_t groundNormal,
                      pm_trace_fn_t traceFunc,
                      pm_trace_fn_t traceDownFunc,
                      qboolean useAltContentMask, float proneLength,
                      pm_entity_type_fn_t entityTypeFunc)
{
    return BG_CheckProneValid(
        clientNum, origin, radius, height, yaw,
        groundOffset, pitchDown, pitchUp,
        skipInitialTrace, allowFallback, groundNormal,
        traceFunc, traceDownFunc, useAltContentMask, proneLength,
        qfalse, entityTypeFunc);
}

int32_t BG_CheckProneTurned(playerState_t *ps, float yaw,
                            pm_trace_fn_t traceFunc)
{
    const volatile float delta = AngleDelta(yaw, ps->viewAngles[1]);
    volatile float fraction;
    float checkYaw;
    float proneLength;

#if EMULATE_X87
    fraction = x87f_store_f32(x87f_div(
        x87f_abs(x87f_load_f32(delta)), x87f_load_f32(240.0f)));
#if defined(WINDOWS_BEHAVIOR)
    {
        const float complement = x87f_store_f32(x87f_sub(
            x87f_load_f32(1.0f), x87f_load_f32(fraction)));
        checkYaw = AngleNormalize360Accurate(x87f_store_f32(x87f_sub(
            x87f_load_f32(yaw),
            x87f_mul(x87f_load_f32(complement),
                     x87f_load_f32(delta)))));
        proneLength = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(complement), x87f_load_f32(60.0f)),
            x87f_mul(x87f_load_f32(fraction), x87f_load_f32(45.0f))));
    }
#else
    {
        const x87f complement = x87f_sub(
            x87f_load_f32(1.0f), x87f_load_f32(fraction));
        checkYaw = AngleNormalize360Accurate(x87f_store_f32(x87f_sub(
            x87f_load_f32(yaw),
            x87f_mul(complement, x87f_load_f32(delta)))));
        proneLength = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(fraction), x87f_load_f32(45.0f)),
            x87f_mul(complement, x87f_load_f32(60.0f))));
    }
#endif
#else
    fraction = (float)((long double)PM_FloatAbs(delta) / 240.0L);
#if defined(WINDOWS_BEHAVIOR)
    {
        const volatile float complement =
            (float)(1.0L - (long double)fraction);
        checkYaw = AngleNormalize360Accurate((float)(
            (long double)yaw -
            (long double)complement * (long double)delta));
        proneLength = (float)(
            (long double)complement * 60.0L +
            (long double)fraction * 45.0L);
    }
#else
    {
        const long double complement = 1.0L - (long double)fraction;
        checkYaw = AngleNormalize360Accurate((float)(
            (long double)yaw - complement * (long double)delta));
        proneLength = (float)(
            (long double)fraction * 45.0L + complement * 60.0L);
    }
#endif
#endif

    return BG_CheckProne(
        ps->psClientNum, ps->psOrigin, ps->playerMaxs[0], 30.0f,
        checkYaw, &ps->torsoHeight, &ps->torsoPitch, &ps->waistPitch,
        qtrue, ps->groundEntityNum != ENTITYNUM_NONE, NULL, traceFunc,
        NULL, qfalse, proneLength, NULL);
}

void PM_UpdatePronePitch(void)
{
    const float *groundNormal;
    float targetPitch;

    if ((pm->ps->playerStateFlags & PMF_PRONE) == 0) {
        return;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        groundNormal = pml.groundPlane == 0
                           ? NULL
                           : pml.groundTrace.normal;
        if (BG_CheckProne(
                pm->ps->psClientNum, pm->ps->psOrigin,
                pm->ps->playerMaxs[0], 30.0f, pm->ps->proneDirection,
                &pm->ps->torsoHeight, &pm->ps->torsoPitch,
                &pm->ps->waistPitch, qtrue,
                pm->ps->groundEntityNum != ENTITYNUM_NONE,
                groundNormal, pm->trace3, pm->trace2, qfalse, 60.0f,
                pm->entityType) == 0 ||
            pm->waterlevel != 0) {
            BG_AddPredictableEventToPlayerstate(
                EV_STANCE_FORCE_CROUCH, 0, pm->ps);
            pm->ps->playerStateFlags |= PMF_PRONE_BLOCKED;
        }
    } else if (pml.groundPlane != 0 &&
               pml.groundTrace.normal[2] < 0.7f) {
        BG_AddPredictableEventToPlayerstate(
            EV_STANCE_FORCE_CROUCH, 0, pm->ps);
    }

    targetPitch = pml.groundPlane == 0
                      ? 0.0f
                      : PitchForYawOnNormal(
                            pm->ps->proneDirection,
                            pml.groundTrace.normal);
    {
        const volatile float delta =
            AngleDelta(targetPitch, pm->ps->proneDirectionPitch);

        if (delta != 0.0f) {
#if EMULATE_X87
            x87f maxStep = x87f_mul(
                x87f_load_f32(pml.frametime), x87f_load_f32(70.0f));
#if defined(WINDOWS_BEHAVIOR)
            maxStep = x87f_load_f32(x87f_store_f32(maxStep));
#endif
            if (x87f_lt(maxStep,
                        x87f_abs(x87f_load_f32(delta)))) {
#if defined(WINDOWS_BEHAVIOR)
                const x87f updateStep = maxStep;
#else
                const x87f updateStep = x87f_mul(
                    x87f_load_f32(pml.frametime),
                    x87f_load_f32(70.0f));
#endif
                pm->ps->proneDirectionPitch = x87f_store_f32(x87f_add(
                    x87f_load_f32(pm->ps->proneDirectionPitch),
                    x87f_mul(x87f_load_i32(PM_FloatSign(delta)),
                             updateStep)));
            } else {
                pm->ps->proneDirectionPitch = x87f_store_f32(x87f_add(
                    x87f_load_f32(pm->ps->proneDirectionPitch),
                    x87f_load_f32(delta)));
            }
#else
#if defined(WINDOWS_BEHAVIOR)
            const volatile float maxStep =
                (float)((long double)pml.frametime * 70.0L);
#else
            const long double maxStep =
                (long double)pml.frametime * 70.0L;
#endif
            if ((long double)PM_FloatAbs(delta) >
                (long double)maxStep) {
#if defined(WINDOWS_BEHAVIOR)
                const long double updateStep = (long double)maxStep;
#else
                const long double updateStep =
                    (long double)pml.frametime * 70.0L;
#endif
                pm->ps->proneDirectionPitch = (float)(
                    (long double)pm->ps->proneDirectionPitch +
                    (long double)PM_FloatSign(delta) *
                        updateStep);
            } else {
                pm->ps->proneDirectionPitch = (float)(
                    (long double)pm->ps->proneDirectionPitch +
                    (long double)delta);
            }
#endif
            pm->ps->proneDirectionPitch = AngleNormalize180Accurate(
                pm->ps->proneDirectionPitch);
        }
    }

    targetPitch = pml.groundPlane == 0
                      ? 0.0f
                      : PitchForYawOnNormal(
                            pm->ps->viewAngles[1],
                            pml.groundTrace.normal);
    {
        const volatile float delta =
            AngleDelta(targetPitch, pm->ps->proneTorsoPitch);

        if (delta != 0.0f) {
#if EMULATE_X87
            x87f maxStep = x87f_mul(
                x87f_load_f32(pml.frametime), x87f_load_f32(70.0f));
#if defined(WINDOWS_BEHAVIOR)
            maxStep = x87f_load_f32(x87f_store_f32(maxStep));
#endif
            if (x87f_lt(maxStep,
                        x87f_abs(x87f_load_f32(delta)))) {
#if defined(WINDOWS_BEHAVIOR)
                const x87f updateStep = maxStep;
#else
                const x87f updateStep = x87f_mul(
                    x87f_load_f32(pml.frametime),
                    x87f_load_f32(70.0f));
#endif
                pm->ps->proneTorsoPitch = x87f_store_f32(x87f_add(
                    x87f_load_f32(pm->ps->proneTorsoPitch),
                    x87f_mul(x87f_load_i32(PM_FloatSign(delta)),
                             updateStep)));
            } else {
                pm->ps->proneTorsoPitch = x87f_store_f32(x87f_add(
                    x87f_load_f32(pm->ps->proneTorsoPitch),
                    x87f_load_f32(delta)));
            }
#else
#if defined(WINDOWS_BEHAVIOR)
            const volatile float maxStep =
                (float)((long double)pml.frametime * 70.0L);
#else
            const long double maxStep =
                (long double)pml.frametime * 70.0L;
#endif
            if ((long double)PM_FloatAbs(delta) >
                (long double)maxStep) {
#if defined(WINDOWS_BEHAVIOR)
                const long double updateStep = (long double)maxStep;
#else
                const long double updateStep =
                    (long double)pml.frametime * 70.0L;
#endif
                pm->ps->proneTorsoPitch = (float)(
                    (long double)pm->ps->proneTorsoPitch +
                    (long double)PM_FloatSign(delta) *
                        updateStep);
            } else {
                pm->ps->proneTorsoPitch = (float)(
                    (long double)pm->ps->proneTorsoPitch +
                    (long double)delta);
            }
#endif
            pm->ps->proneTorsoPitch = AngleNormalize180Accurate(
                pm->ps->proneTorsoPitch);
        }
    }
}
