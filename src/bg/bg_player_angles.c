#include "bg_animation.h"
#include "bg_animation_services.h"
#include "bg_vehicle.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    BG_SWING_ANGLE_MASK = 65535
};

/*
 * Windows cgame/game are instruction-identical after relocated addresses are
 * normalized:
 *
 *   BG_SwingAngles  0x30004330 / 0x20004330
 *   BG_PlayerAngles 0x30004550 / 0x20004550
 *
 * Linux game retains the canonical symbols at RVA 0x0001d9b4 and 0x0001dbac.
 * Supporting Mac cgame/game retain both names, equal per-pair sizes, and the
 * same call graph. Linux spills each updated angle to binary32 before calling
 * AngleMod; Windows carries the add/subtract directly into its inline BAMS
 * conversion. BG_SwingAngles therefore has whole-function platform bodies.
 */

#if defined(WINDOWS_BEHAVIOR)
/* NOT_FROM_ORIGINAL_SOURCE: source-level expression of the repeated inline
 * Windows ANGLE2SHORT/SHORT2ANGLE instruction sequence. */
static float bg_compat_quantize_angle_add(float left, float right)
{
    const float angleToShort = 182.04444885253906f;
    const float shortToAngle = 0.0054931640625f;
    int32_t packed;

#if EMULATE_X87
    packed = (int32_t)(uint32_t)x87f_store_i64_trunc(x87f_mul(
        x87f_add(x87f_load_f32(left), x87f_load_f32(right)),
        x87f_load_f32(angleToShort)));
    return x87f_store_f32(x87f_mul(
        x87f_load_i32(packed & BG_SWING_ANGLE_MASK),
        x87f_load_f32(shortToAngle)));
#else
    packed = coduo_fp_to_i32_extended(
        ((long double)left + (long double)right) * angleToShort);
    return (float)((double)(packed & BG_SWING_ANGLE_MASK) * shortToAngle);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: subtraction companion for the two clamp sites. */
static float bg_compat_quantize_angle_subtract(float left, float right)
{
    const float angleToShort = 182.04444885253906f;
    const float shortToAngle = 0.0054931640625f;
    int32_t packed;

#if EMULATE_X87
    packed = (int32_t)(uint32_t)x87f_store_i64_trunc(x87f_mul(
        x87f_sub(x87f_load_f32(left), x87f_load_f32(right)),
        x87f_load_f32(angleToShort)));
    return x87f_store_f32(x87f_mul(
        x87f_load_i32(packed & BG_SWING_ANGLE_MASK),
        x87f_load_f32(shortToAngle)));
#else
    packed = coduo_fp_to_i32_extended(
        ((long double)left - (long double)right) * angleToShort);
    return (float)((double)(packed & BG_SWING_ANGLE_MASK) * shortToAngle);
#endif
}

void BG_SwingAngles(float target, float deadband, float maxDeviation,
                    float stepScale, float *angle, qboolean *active)
{
    float delta;
    float step;

    if (*active == qfalse) {
        delta = AngleSubtract(*angle, target);
        if (!(delta > deadband) && !(-deadband > delta)) {
            return;
        }
        *active = qtrue;
    }

    delta = AngleSubtract(target, *angle);
    step = fabsf(delta) * 0.05f;
    if (step < 0.5f) {
        step = 0.5f;
    }

    if (delta >= 0.0f) {
        float increment =
            (float)bg_compat_animation_frame_time() * step * stepScale;
        if (increment >= delta) {
            increment = delta;
            *active = qfalse;
        } else {
            *active = qtrue;
        }
        *angle = bg_compat_quantize_angle_add(*angle, increment);
    } else if (delta < 0.0f) {
        float increment =
            -((float)bg_compat_animation_frame_time() * step * stepScale);
        if (increment <= delta) {
            increment = delta;
            *active = qfalse;
        } else {
            *active = qtrue;
        }
        *angle = bg_compat_quantize_angle_add(*angle, increment);
    }

    delta = AngleSubtract(target, *angle);
    if (delta > maxDeviation) {
        *angle = bg_compat_quantize_angle_subtract(target, maxDeviation);
    } else if (delta < -maxDeviation) {
        *angle = bg_compat_quantize_angle_add(target, maxDeviation);
    }
}
#else
void BG_SwingAngles(float target, float deadband, float maxDeviation,
                    float stepScale, float *angle, qboolean *active)
{
    if (*active == qfalse) {
        const float delta = AngleSubtract(*angle, target);

        if (delta > deadband || delta < -deadband) {
            *active = qtrue;
        }
    }

    if (*active != qfalse) {
        float delta = AngleSubtract(target, *angle);
        float step = fabsf(delta) * 0.05f;

        if (step < 0.5f) {
            step = 0.5f;
        }

        if (delta >= 0.0f) {
            const float maximumStep =
                (float)bg_compat_animation_frame_time() * step * stepScale;

            if (delta <= maximumStep) {
                *active = qfalse;
            } else {
                *active = qtrue;
                delta = maximumStep;
            }
            *angle = AngleMod(*angle + delta);
        } else if (delta < 0.0f) {
            const float minimumStep =
                -((float)bg_compat_animation_frame_time() * step * stepScale);

            if (minimumStep <= delta) {
                *active = qfalse;
            } else {
                *active = qtrue;
                delta = minimumStep;
            }
            *angle = AngleMod(*angle + delta);
        }

        delta = AngleSubtract(target, *angle);
        if (delta > maxDeviation) {
            *angle = AngleMod(target - maxDeviation);
        } else if (delta < -maxDeviation) {
            *angle = AngleMod(target + maxDeviation);
        }
    }
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: preserve an unspilled x87 multiply/add chain in
 * the shared source on both original precision-control modes. */
static float bg_compat_player_angle_multiply_add(float left, float scale,
                                                  float addend)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(left), x87f_load_f32(scale)),
        x87f_load_f32(addend)));
#elif defined(WINDOWS_BEHAVIOR)
    return (float)((double)left * scale + addend);
#else
    return (float)((long double)left * scale + addend);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the corresponding subtract/multiply
 * chain used for the wrapped pitch target. */
static float bg_compat_player_angle_subtract_multiply(float left,
                                                       float subtract,
                                                       float scale)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_mul(
        x87f_sub(x87f_load_f32(left), x87f_load_f32(subtract)),
        x87f_load_f32(scale)));
#elif defined(WINDOWS_BEHAVIOR)
    return (float)(((double)left - subtract) * scale);
#else
    return (float)(((long double)left - subtract) * scale);
#endif
}

void BG_PlayerAngles(const entityState_t *entity, clientInfo_t *clientInfo)
{
    const uint32_t entityFlags = entity->eFlags;
    const uint32_t vehicleState = (uint32_t)entity->vehicleAnimState;
    const vehicle_type_t vehicleType = (vehicle_type_t)(
        (vehicleState & VEHICLE_ANIM_STATE_TYPE_MASK) >>
        VEHICLE_ANIM_STATE_TYPE_SHIFT);
    const int32_t vehiclePosition = (int32_t)(
        (vehicleState & VEHICLE_ANIM_STATE_POS_MASK) >>
        VEHICLE_ANIM_STATE_POS_SHIFT);
    const uint32_t animationIndex =
        (uint32_t)entity->legsAnim & ~ANIM_TOGGLEBIT;
    float leanAmount;
    float viewPitch;
    float viewYaw;
    const qboolean restricted = (entityFlags & EF_RESTRICTED_MASK) != 0;
    float torsoTarget;
    float legsTarget;
    float torsoMaximum;
    uint32_t moveType;

#if defined(LINUX_BEHAVIOR)
    /* NOT_FROM_ORIGINAL_SOURCE: preserve the unoptimized Linux source edge
     * whose result is stored but never subsequently consumed. */
    volatile float evaluatedLean =
        (float)GetLeanFraction(clientInfo->leanFraction);
    (void)evaluatedLean;
#endif
    leanAmount = clientInfo->leanAmount;
    viewPitch = clientInfo->viewPitch;
    viewYaw = AngleMod(clientInfo->viewYaw);

    if (!restricted ||
        BG_AllowPlayerWeaponAtVehiclePos(vehicleType, vehiclePosition)) {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0 ||
            (moveType & BG_ANIM_IDLE_MOVE_TYPE_MASK) == 0) {
            clientInfo->torsoYawActive = qtrue;
            clientInfo->leanActive = qtrue;
            clientInfo->legsYawActive = qtrue;
        } else if (BG_GetConditionValue(clientInfo, ANIM_COND_FIRING,
                                        qtrue) != 0) {
            clientInfo->torsoYawActive = qtrue;
            clientInfo->leanActive = qtrue;
        }
    } else {
        clientInfo->torsoYawActive = qtrue;
        clientInfo->leanActive = qtrue;
        clientInfo->legsYawActive = qtrue;
    }

    legsTarget = viewYaw + leanAmount;
    torsoTarget = viewYaw;
    if ((entityFlags & EF_ANGLE_FIXED_YAW) == 0) {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0) {
            torsoMaximum = 0.0f;
            torsoTarget = legsTarget;
        } else if ((entityFlags & EF_ANGLE_TORSO_ONLY_YAW) != 0) {
            torsoMaximum = 90.0f;
        } else if ((entityFlags & EF_ANGLE_TURRET_TIGHT) != 0) {
            torsoMaximum = 45.0f;
        } else {
            if ((entityFlags & EF_ANGLE_TURRET_YAW) == 0) {
                torsoTarget = bg_compat_player_angle_multiply_add(
                    leanAmount, 0.3f, viewYaw);
            }
            torsoMaximum = 90.0f;
        }
    } else {
        torsoMaximum = 90.0f;
        legsTarget = viewYaw;
    }

    if (restricted &&
        BG_AllowPlayerWeaponAtVehiclePos(vehicleType, vehiclePosition) &&
        entity->weapon != 0) {
        legsTarget = clientInfo->turretOverrideAngles[1];
    }

    BG_SwingAngles(torsoTarget, 0.0f, torsoMaximum,
                   bg_compat_animation_swing_speed(),
                   &clientInfo->torsoYawAngle,
                   &clientInfo->torsoYawActive);

    if ((entityFlags & EF_ANGLE_FIXED_YAW) != 0) {
        BG_SwingAngles(legsTarget, 0.0f, 150.0f,
                       bg_compat_animation_swing_speed(),
                       &clientInfo->legsYawAngle,
                       &clientInfo->legsYawActive);
    } else if ((entityFlags & EF_ANGLE_TORSO_ONLY_YAW) != 0) {
        clientInfo->legsYawActive = qfalse;
        clientInfo->legsYawAngle = viewYaw + leanAmount;
    } else {
        /* The original bodies clear ANIM_TOGGLEBIT and index the 512-entry
         * animation table directly.  This is implicitly bounded: legsAnim is
         * a 10-bit entity netfield, and clearing its 0x200 toggle bit leaves
         * exactly the table's 0..511 index domain. */
        if ((bgs.animationTable.entries[animationIndex].flagsLowByte &
             BG_ANIM_ENTRY_STRAFE_MASK) != 0) {
            clientInfo->legsYawActive = qfalse;
            BG_SwingAngles(viewYaw, 0.0f, 150.0f,
                           bg_compat_animation_swing_speed(),
                           &clientInfo->legsYawAngle,
                           &clientInfo->legsYawActive);
        } else {
            BG_SwingAngles(legsTarget,
                           clientInfo->legsYawActive != qfalse ? 0.0f : 40.0f,
                           150.0f, bg_compat_animation_swing_speed(),
                           &clientInfo->legsYawAngle,
                           &clientInfo->legsYawActive);
        }
    }

    if (restricted) {
        clientInfo->torsoYawAngle = viewYaw;
        clientInfo->legsYawAngle = viewYaw;
    } else {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0) {
            clientInfo->torsoYawAngle = viewYaw + leanAmount;
            clientInfo->legsYawAngle = viewYaw + leanAmount;
        }
    }

    if ((entityFlags & EF_ANGLE_FIXED_YAW) != 0 || restricted) {
        viewPitch = 0.0f;
    } else {
        moveType = (uint32_t)BG_GetConditionValue(
            clientInfo, ANIM_COND_MOVETYPE, qfalse);
        if ((moveType & BG_ANIM_CLIMB_MOVE_TYPE_MASK) != 0) {
            viewPitch = 0.0f;
        } else if (viewPitch > 180.0f) {
            viewPitch = bg_compat_player_angle_subtract_multiply(
                viewPitch, 360.0f, 0.6f);
        } else {
            viewPitch = bg_compat_player_angle_multiply_add(
                viewPitch, 0.6f, 0.0f);
        }
    }

    BG_SwingAngles(viewPitch, 0.0f, 45.0f, 0.15f,
                   &clientInfo->leanAngle, &clientInfo->leanActive);
}
