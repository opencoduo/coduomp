// Source: uo_cgame_mp_x86.dll 0x30004da0..0x300055a4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30004da0_300055a4.mcode
//
// BG_Player_DoControllersInternal builds the six spine-control angles followed by the
// tag_origin angle and offset targets consumed by BG_Player_DoControllers.
// The mcode's size-derived CG_DrawWeaponSelect name is rejected: this routine
// only reads player animation/entity state and writes eight vec3 targets.
// The Mac BG_Player_DoControllersInternal has the same view, vehicle, lean,
// condition, and angle-normalization call pattern and is called by the matching
// BG_Player_DoControllers wrapper, resolving the source name.
// Windows game 0x20004d80 has the same arithmetic and state reads as cgame
// after relocation; both read vehicleAnimState at entityState_t +0x88. Linux
// game RVA 0x1e761 retains the same decisions, with two compiler-visible
// arithmetic differences preserved locally below: its lean scaling remains
// `(lean * 50) * factor`, and its FSINCOS input uses binary64 pi/180 constants.
// The cgame-only debug controller sine modes are exposed by the target service
// header and are disabled by the game service implementation.

#include "bg_animation.h"
#include "bg_animation_services.h"
#include "bg_vehicle.h"

#include "compat/coduo_native_x87.h"
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
    CONTROL_TAG_DEBUG_YAW_POSITIVE = 4,
    CONTROL_TAG_DEBUG_ROLL = 5,
    CONTROL_TAG_DEBUG_YAW_NEGATIVE_AND_ROLL = 6
};

/* NOT_FROM_ORIGINAL_SOURCE: source-level carrier for an x87 multiply followed
 * by an add and one binary32 store. The original platform control word governs
 * native x87; the software path selects the same Windows PC=53 or Linux PC=64
 * policy independently of the host architecture. */
static float bg_compat_controller_add_product(float addend, float left,
                                               float right)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(left), x87f_load_f32(right)),
        x87f_load_f32(addend)));
#else
    return (float)((long double)left * (long double)right +
                   (long double)addend);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: subtraction companion for unspilled x87 chains. */
static float bg_compat_controller_subtract_product(float minuend, float left,
                                                    float right)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_sub(
        x87f_load_f32(minuend),
        x87f_mul(x87f_load_f32(left), x87f_load_f32(right))));
#else
    return (float)((long double)minuend -
                   (long double)left * (long double)right);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: three-factor subtraction chain used by the prone
 * lean offset. */
#if !defined(LINUX_BEHAVIOR)
static float bg_compat_controller_subtract_triple_product(
    float minuend, float first, float second, float third)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_sub(
        x87f_load_f32(minuend),
        x87f_mul(x87f_mul(x87f_load_f32(first), x87f_load_f32(second)),
                 x87f_load_f32(third))));
#else
    return (float)((long double)minuend -
                   (long double)first * (long double)second *
                       (long double)third);
#endif
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: addition form of the three-factor chain retained
 * by the unoptimized Linux controller body. */
#if defined(LINUX_BEHAVIOR)
static float bg_compat_controller_add_triple_product(
    float addend, float first, float second, float third)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(
        x87f_mul(x87f_mul(x87f_load_f32(first), x87f_load_f32(second)),
                 x87f_load_f32(third)),
        x87f_load_f32(addend)));
#else
    return (float)((long double)first * (long double)second *
                       (long double)third +
                   (long double)addend);
#endif
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: retain the two-product x87 subtraction used by
 * the prone spine-yaw outputs without introducing a binary32 spill between
 * the products and the subtraction. */
static float bg_compat_controller_product_subtract_product(
    float left0, float right0, float left1, float right1)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(left0), x87f_load_f32(right0)),
        x87f_mul(x87f_load_f32(left1), x87f_load_f32(right1))));
#else
    return (float)((long double)left0 * (long double)right0 -
                   (long double)left1 * (long double)right1);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: Windows reloads the rounded spine-roll slot for
 * this subtraction; Linux instead retains a second product in x87. */
#if !defined(LINUX_BEHAVIOR)
static float bg_compat_controller_product_subtract(
    float left, float right, float subtrahend)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(left), x87f_load_f32(right)),
        x87f_load_f32(subtrahend)));
#else
    return (float)((long double)left * (long double)right -
                   (long double)subtrahend);
#endif
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: preserve the extended comparison of an unspilled
 * product against positive zero. */
static qboolean bg_compat_controller_product_is_positive(float left,
                                                          float right)
{
#if EMULATE_X87
    return x87f_lt(x87f_load_f32(0.0f),
                   x87f_mul(x87f_load_f32(left), x87f_load_f32(right)))
               ? qtrue : qfalse;
#else
    return (long double)left * (long double)right > 0.0L
               ? qtrue : qfalse;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original constant load widths in the
 * multiply/divide chain feeding FSINCOS. */
static float bg_compat_controller_yaw_radians(float yaw)
{
#if EMULATE_X87
#if defined(LINUX_BEHAVIOR)
    return x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(yaw),
                 x87f_load_f64(3.141592653589793)),
        x87f_load_f64(180.0)));
#else
    return x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(yaw), x87f_load_f32(3.1415927f)),
        x87f_load_f32(180.0f)));
#endif
#elif defined(LINUX_BEHAVIOR)
    return (float)((long double)yaw * 3.141592653589793L / 180.0L);
#else
    return (float)((long double)yaw * (long double)3.1415927f /
                   (long double)180.0f);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame-only debug motion still executes the
 * original x87 phase chain before passing a binary64 argument to sin, then
 * spills sin's return to binary32 before multiplying by 45. */
static float bg_compat_controller_debug_sin(int32_t time, int32_t clientNum)
{
    const float timeAsFloat = (float)time;
    const float clientNumAsFloat = (float)clientNum;
    float phaseSin;

#if EMULATE_X87
    const x87f phase = x87f_add(
        x87f_mul(x87f_load_f32(timeAsFloat), x87f_load_f32(0.003f)),
        x87f_load_f32(clientNumAsFloat));
    phaseSin = (float)sin(x87f_store_f64(phase));
    return phaseSin;
#else
    const long double phase =
        (long double)timeAsFloat * (long double)0.003f +
        (long double)clientNumAsFloat;
    phaseSin = (float)sin((double)phase);
    return phaseSin;
#endif
}

void BG_Player_DoControllersInternal(clientInfo_t *anim,
                             const entityState_t *entity, vec3_t out[8])
{
    const uint32_t entityFlags = entity->eFlags;
    const int32_t entityClientNum = entity->clientNum;
    float basePitch = 0.0f;
    float baseYaw = anim->torsoYawAngle;
    float adjustedViewYaw = anim->viewYaw;
    vec3_t originAngles = {0.0f, anim->legsYawAngle, 0.0f};
    const int specialView =
        (entityFlags & EF_IN_VEHICLE) != 0;

    /* 0x30004def..0x30004ead: vehicle/turret override selection. */
    if (specialView) {
        basePitch = anim->turretOverrideAngles[0];
        baseYaw = anim->turretOverrideAngles[1];
        originAngles[0] = anim->turretOverrideAngles[0];
        originAngles[1] = anim->turretOverrideAngles[1];
        originAngles[2] = anim->turretOverrideAngles[2];

        const uint32_t vehicleState = (uint32_t)entity->vehicleAnimState;
        const vehicle_type_t vehicleType = (vehicle_type_t)(
            (vehicleState & VEHICLE_ANIM_STATE_TYPE_MASK) >>
            VEHICLE_ANIM_STATE_TYPE_SHIFT);
        const int32_t vehiclePosition = (int32_t)(
            (vehicleState & VEHICLE_ANIM_STATE_POS_MASK) >>
            VEHICLE_ANIM_STATE_POS_SHIFT);

        if (entity->weapon != 0 &&
            BG_AllowPlayerWeaponAtVehiclePos(vehicleType, vehiclePosition)) {
            basePitch = anim->viewPitch;
            baseYaw = anim->viewYaw;
        }

        adjustedViewYaw = AngleSubtract(anim->viewYaw, baseYaw);
        if (adjustedViewYaw > 90.0f) {
            adjustedViewYaw = 180.0f - adjustedViewYaw;
        } else if (adjustedViewYaw < -90.0f) {
            adjustedViewYaw = -180.0f - adjustedViewYaw;
        }
        adjustedViewYaw = AngleNormalize180(adjustedViewYaw + baseYaw);
    } else if ((anim->conditionWords[ANIM_COND_MOVETYPE][0] &
                BG_ANIM_CLIMB_MOVE_TYPE_MASK) == 0) {
        /* 0x30004ec4: the raw leanAngle load precedes the prone-flag branch,
         * so basePitch picks it up even when the prone flag is clear. */
        basePitch = anim->leanAngle;
        if ((entityFlags & EF_PRONE) != 0) {
            /* 0x30004ed6..0x30004f07: TEST AH,0x41 admits only a strictly
             * positive normalized value to the 1/2 branch.  Zero, negative,
             * and unordered values use 1/4. */
            basePitch = AngleNormalize180(basePitch);
            basePitch *= basePitch > 0.0f ? 0.5f : 0.25f;
        }
    }

    /* 0x30004f0f..0x30004f49: the four independent wrapped deltas. */
    const float viewPitchDelta = AngleSubtract(anim->viewPitch, basePitch);
    const float viewYawDelta = AngleSubtract(adjustedViewYaw, baseYaw);
    const float originPitchDelta = AngleSubtract(basePitch, originAngles[0]);
    const float originYawDelta = AngleSubtract(baseYaw, originAngles[1]);

    if (specialView) {
        originAngles[0] = 0.0f;
        originAngles[1] = 0.0f;
        originAngles[2] = 0.0f;
    }

    /* 0x30004f62..0x30004fe4: signed lean shaping and tag-origin defaults. */
    /* Stock snapshots this field before evaluating the lean curve. */
    const float originOffsetZ = entity->viewAngles[0];
    const float leanInput = anim->leanFraction;
    /* Windows inlines GetLeanFraction as integer sign clear plus FSUB/FMUL;
     * Linux calls the same shared leaf and spills its binary32 return. */
    const float leanPhase = GetLeanFraction(leanInput);
#if defined(LINUX_BEHAVIOR)
#if EMULATE_X87
    float leanSwing = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(leanPhase), x87f_load_f32(50.0f)),
        x87f_load_f32(0.925f)));
#else
    float leanSwing = (float)((long double)leanPhase * 50.0L * 0.925L);
#endif
#else
    float leanSwing = (float)((long double)leanPhase * 46.25L);
#endif
    float secondaryLeanSwing = leanSwing;
    float originOffsetX = 0.0f;
    /* 0x30004fb5..0x30004fc5: FMUL 2.5f then FSUBR from the 0.0f constant --
     * 0 - x, not a negated-constant multiply (matches the zero-sign edge). */
#if defined(LINUX_BEHAVIOR)
    float originOffsetY =
        bg_compat_controller_add_product(0.0f, -leanPhase, 2.5f);
#else
    float originOffsetY =
        bg_compat_controller_subtract_product(0.0f, leanPhase, 2.5f);
#endif

    if ((entityFlags & EF_DEAD) == 0 && !specialView) {
        /* 0x30004fe0: the wrapped legs-versus-view delta replaces
         * originAngles[1] itself (out[6][1]); originOffsetX stays zero on
         * this path. */
        originAngles[1] = AngleSubtract(originAngles[1], anim->viewYaw);
    }

    float spine0Pitch;
    float spine0Yaw;
    float spine0Roll;
    float spine1Pitch;
    float spine1Yaw;
    float spine1Roll;
    float spine2Pitch;
    float spine2Yaw;
    float spine2Roll;

    if ((entityFlags & EF_PRONE) != 0) {
        /* 0x30004ffe..0x30005210: prone spine and tag-origin shaping. */
        if (leanPhase != 0.0f) {
            secondaryLeanSwing *= 0.5f;
        }

        originAngles[0] += entity->viewAngles[1];
        /* FMUL pi and FDIV 180 are one 53-bit chain before the float spill
         * consumed by FSINCOS. */
        const float yawRadians =
            bg_compat_controller_yaw_radians(originYawDelta);
        float yawSin;
        float yawCos;
        coduo_x87_sincosf(yawRadians, &yawSin, &yawCos);
        /* 0x3000506f: the cosine slot is overwritten with 1-yawCos, which
         * both the X offset and the lean term below consume. */
        const float yawCosInv = 1.0f - yawCos;
#if defined(LINUX_BEHAVIOR)
        originOffsetX = bg_compat_controller_add_product(
            originOffsetX, yawCosInv, -24.0f);
        originOffsetY = bg_compat_controller_add_product(
            originOffsetY, yawSin, -12.0f);
#else
        originOffsetX =
            bg_compat_controller_subtract_product(0.0f, yawCosInv, 24.0f);
        originOffsetY =
            bg_compat_controller_subtract_product(originOffsetY, yawSin,
                                                   12.0f);
#endif
        /* FMUL remains in ST0 for the comparison; do not round a tiny product
         * to binary32 before deciding its sign. */
        if (bg_compat_controller_product_is_positive(yawSin, leanPhase)) {
#if defined(LINUX_BEHAVIOR)
            originOffsetY = bg_compat_controller_add_triple_product(
                originOffsetY, yawCosInv, -leanPhase, 16.0f);
#else
            originOffsetY =
                bg_compat_controller_subtract_triple_product(
                    originOffsetY, yawCosInv, leanPhase, 16.0f);
#endif
        }

        spine0Pitch = 0.0f;
        spine0Yaw = leanSwing * -1.2f;
        spine0Roll = leanSwing * 0.30000001f;
        spine1Pitch = 0.0f;
        /* 0x300051d2/0x300051e4: leanSwing * 0.2f is rounded to a float slot
         * (the roll value) and that rounded value is what the yaw subtracts. */
        spine1Roll = leanSwing * 0.2f;
#if defined(LINUX_BEHAVIOR)
        spine1Yaw =
            bg_compat_controller_product_subtract_product(
                originYawDelta, 0.1f, leanSwing, 0.2f);
#else
        spine1Yaw = bg_compat_controller_product_subtract(
            originYawDelta, 0.1f, spine1Roll);
#endif
        spine2Pitch = originPitchDelta;
        /* 0x300051fa multiplies leanSwing by the .rdata -1.0f at 0x3007bdb0
         * before the subtract, i.e. spine2Yaw adds the full lean swing. */
        spine2Yaw =
            bg_compat_controller_product_subtract_product(
                originYawDelta, 0.80000001f, leanSwing, -1.0f);
        spine2Roll = leanSwing * -0.2f;
    } else {
        /* 0x30005215..0x300053df: standing/crouched stance-dependent scaling. */
        if (leanPhase != 0.0f) {
            float stanceScale;
            if ((entityFlags & EF_CROUCHING) != 0) {
                stanceScale = leanPhase > 0.0f ? 1.5f : 1.25f;
            } else {
                stanceScale = leanPhase > 0.0f ? 0.80000001f : 1.25f;
            }
            leanSwing *= stanceScale;
            secondaryLeanSwing *= stanceScale;
        }

#if defined(LINUX_BEHAVIOR)
#if EMULATE_X87
        originAngles[2] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_mul(x87f_load_f32(leanPhase),
                              x87f_load_f32(50.0f)),
                     x87f_load_f32(0.075f)),
            x87f_load_f32(originAngles[2])));
#else
        originAngles[2] =
            (float)((long double)originAngles[2] +
                    (long double)leanPhase * 50.0L * 0.075L);
#endif
#else
        originAngles[2] =
            bg_compat_controller_add_product(originAngles[2], leanPhase,
                                              3.7500002f);
#endif
        spine0Pitch = originPitchDelta * 0.2f;
        spine0Yaw = originYawDelta * 0.40000001f;
        spine0Roll = leanSwing * 0.5f;
        spine1Pitch = originPitchDelta * 0.30000001f;
        spine1Yaw = originYawDelta * 0.40000001f;
        spine1Roll = leanSwing * 0.5f;
        spine2Pitch = originPitchDelta * 0.5f;
        spine2Yaw = originYawDelta * 0.2f;
        spine2Roll = leanSwing * -0.60000002f;
    }

    /* 0x300050c8..0x300051be / 0x30005281..0x3000539b: yaw test motion. */
    const int32_t controllerPitchDebug =
        bg_compat_controller_debug_value();
    if (controllerPitchDebug == CONTROL_TAG_DEBUG_YAW_POSITIVE) {
        /* The CRT sin result is explicitly spilled to float before FMUL 45;
         * that product and the following add share one x87 chain. */
        const float phaseSin = bg_compat_controller_debug_sin(
            bg_compat_controller_debug_time(), entityClientNum);
        spine0Pitch =
            bg_compat_controller_add_product(spine0Pitch, phaseSin, 45.0f);
    } else if (controllerPitchDebug ==
               CONTROL_TAG_DEBUG_YAW_NEGATIVE_AND_ROLL) {
        const float phaseSin = bg_compat_controller_debug_sin(
            bg_compat_controller_debug_time(), entityClientNum);
        spine0Pitch =
            bg_compat_controller_subtract_product(spine0Pitch, phaseSin,
                                                   45.0f);
    } else if (entity->viewAngles[1] != 0.0f ||
               entity->viewAngles[2] != 0.0f) {
        spine0Pitch += AngleSubtract(entity->viewAngles[1],
                                     entity->viewAngles[2]);
    }

    out[0][0] = spine0Pitch;
    out[0][1] = spine0Yaw;
    out[0][2] = spine0Roll;
    out[1][0] = spine1Pitch;
    out[1][1] = spine1Yaw;
    out[1][2] = spine1Roll;
    out[2][0] = spine2Pitch;
    out[2][1] = spine2Yaw;
    out[2][2] = spine2Roll;

    out[3][0] = viewPitchDelta * 0.30000001f;
    out[3][1] = viewYawDelta * 0.30000001f;
    out[3][2] = 0.0f;
    out[4][0] = viewPitchDelta * 0.69999999f;
    out[4][1] = viewYawDelta * 0.69999999f;
    out[4][2] = secondaryLeanSwing * -0.30000001f;

    const int32_t controllerRollDebug =
        bg_compat_controller_debug_value();
    if (controllerRollDebug == CONTROL_TAG_DEBUG_ROLL ||
        controllerRollDebug ==
            CONTROL_TAG_DEBUG_YAW_NEGATIVE_AND_ROLL) {
        const float phaseSin = bg_compat_controller_debug_sin(
            bg_compat_controller_debug_time(), entityClientNum);
        out[5][0] =
            bg_compat_controller_add_product(0.0f, phaseSin, 45.0f);
    } else if (entity->viewAngles[1] != 0.0f ||
               entity->viewAngles[2] != 0.0f) {
        out[5][0] = AngleSubtract(entity->viewAngles[2],
                                  entity->viewAngles[1]);
    } else {
        out[5][0] = 0.0f;
    }
    out[5][1] = 0.0f;
    out[5][2] = 0.0f;

    out[6][0] = originAngles[0];
    out[6][1] = originAngles[1];
    out[6][2] = originAngles[2];
    out[7][0] = originOffsetX;
    out[7][1] = originOffsetY;
    out[7][2] = originOffsetZ;
}
