// Sources: uo_cgame_mp_x86.dll 0x30009d30..0x3000a133,
//          uo_game_mp_x86.dll  0x20009ae0..0x20009ee9,
//          game.mp.uo.i386.so  0x00025cb4..0x000262b6
//
// PM_CrashLand -- process a pmove landing, including fall damage, landing stun,
// animation, velocity damping, and predictable landing events. The assigned
// G_SpawnTurret name is a size-only collision with a server function and is
// rejected: this body operates exclusively on player-state/pmove landing state.
//
// ABI: cdecl, no arguments; all state comes from pm and pml. Windows retains
// the initial fall-height solve as one live x87 chain under PC=53. Linux stores
// the named intermediates to binary32 and calls its binary64 libm sqrt under
// PC=64; complete platform bodies preserve that genuine source realization.

#include "bg_pmove.h"

#include "bg_player_state.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

void Com_Printf(const char *format, ...);

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#define PM_CRASH_DAMAGE_SCALE                100.0f
#define PM_CRASH_WATERLEVEL_TWO_SCALE        0.5f
#define PM_CRASH_EVENT_HEIGHT_BASE           12.0f
#define PM_CRASH_EVENT_HEIGHT_SCALE          0.03846154f
#define PM_CRASH_EVENT_PARM_BIAS             1.0f
#define PM_CRASH_EVENT_PARM_SCALE            4.0f
#define PM_CRASH_EVENT_PARM_MAX              24
#define PM_CRASH_MINOR_HEIGHT                4.0f
#define PM_CRASH_MEDIUM_HEIGHT               8.0f
#define PM_CRASH_VELOCITY_DAMP               0.67000002f
#define PM_CRASH_STUN_DAMAGE_MSEC            35
#define PM_CRASH_STUN_BASE_MSEC              500
#define PM_CRASH_STUN_RAMP_END_MSEC          1500
#define PM_CRASH_STUN_MAX_MSEC               2000
#define PM_CRASH_STUN_SPEED_HIGH             0.5f
#define PM_CRASH_STUN_SPEED_LOW              0.2f
#define PM_CRASH_STUN_MSEC_TO_SEC            0.001f
#define PM_CRASH_STUN_SPEED_SPAN             0.30000001f

enum {
    PM_CRASH_WATERLEVEL_TWO = 2,
    PM_CRASH_WATERLEVEL_FULL = 3
};

#if defined(WINDOWS_BEHAVIOR)
void PM_CrashLand(void)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    float gravity;
    float previousVerticalVelocity;
    /* The whole fall-height solve (0x30009d47..0x30009db7) is ONE x87 chain: the
     * DLL never stores displacement, discriminant, impactTime or landingVelocity to
     * a float slot -- discriminant is compared in-register (FCOM 0.0 at 0x30009d77),
     * and landingVelocity is kept on the stack for the debug print. Only gravity
     * (FST-kept at [ESP+8], and an exact integer) and fallHeight (FSTP [ESP+4] at
     * 0x30009db7) round. Carry the intermediates through the x87 shim in
     * emulated builds and through long double in native-x87 builds so `float`
     * locals do not introduce stores absent from the DLL. */
#if EMULATE_X87
    x87f landingVelocity;
#else
    long double landingVelocity;
#endif
    float fallHeight;
    int32_t fallDamage = 0;
    int32_t landingEventParm = 0;

    /* 0x30009d3a: fully submerged players take no landing action. */
    if (move->waterlevel == PM_CRASH_WATERLEVEL_FULL) {
        return;
    }

    /*
     * 0x30009d47..0x30009db7: solve z(t)=currentZ for constant acceleration
     * -gravity, retain the downward landing-speed magnitude, then convert its
     * kinetic term to an equivalent fall height. The discriminant comparison is
     * strictly `< 0`; unordered values continue, matching the x87 branch.
     */
    previousVerticalVelocity = pml.previousVelocity[2];
#if EMULATE_X87
    {
        const x87f gravityRegister = x87f_load_i32(ps->gravity);
        const x87f negativeGravity = x87f_neg(gravityRegister);
        const x87f halfNegativeGravity = x87f_mul(
            x87f_load_f32(0.5f), negativeGravity);
        const x87f previousVelocityRegister =
            x87f_load_f32(previousVerticalVelocity);
        const x87f fallDistance = x87f_sub(
            x87f_load_f32(pml.previousOrigin[2]),
            x87f_load_f32(ps->psOrigin[2]));
        const x87f discriminant = x87f_sub(
            x87f_mul(previousVelocityRegister, previousVelocityRegister),
            x87f_mul(x87f_mul(fallDistance, halfNegativeGravity),
                     x87f_load_f32(4.0f)));

        gravity = x87f_store_f32(gravityRegister);
        if (x87f_lt_signaling(discriminant, x87f_load_f32(0.0f))) {
            return;
        }

        const x87f impactTime = x87f_div(
            x87f_sub(x87f_neg(previousVelocityRegister),
                     x87f_sqrt(discriminant)),
            x87f_add(halfNegativeGravity, halfNegativeGravity));
        landingVelocity = x87f_neg(x87f_add(
            previousVelocityRegister,
            x87f_mul(impactTime, negativeGravity)));
        fallHeight = x87f_store_f32(x87f_div(
            x87f_mul(landingVelocity, landingVelocity),
            x87f_add(x87f_load_f32(gravity),
                     x87f_load_f32(gravity))));
    }
#else
    const long double gravityWide = (long double)ps->gravity;
    const long double negativeGravity = -gravityWide;
    const long double halfNegativeGravity =
        0.5L * negativeGravity;
    const long double previousVelocityRegister =
        (long double)previousVerticalVelocity;
    const long double fallDistance =
        (long double)pml.previousOrigin[2] -
        (long double)ps->psOrigin[2];
    const long double discriminant =
        previousVelocityRegister * previousVelocityRegister -
        (fallDistance * halfNegativeGravity) * 4.0L;

    gravity = (float)gravityWide;
    if (discriminant < 0.0L) {
        return;
    }

    const long double impactTime =
        (-previousVelocityRegister - coduo_x87_sqrtl(discriminant)) /
        (halfNegativeGravity + halfNegativeGravity);
    landingVelocity =
        -(previousVelocityRegister + impactTime * negativeGravity);
    /* 0x30009daf reloads the rounded FST copy of gravity for this denominator. */
    fallHeight =
        (float)((landingVelocity * landingVelocity) /
                (2.0L * (long double)gravity));
#endif

    if (move->debugMove != 0) {
#if EMULATE_X87
        Com_Printf("landing vel: %.1f fall height: %.1f\n",
                   x87f_store_f64(landingVelocity),
                   (double)fallHeight);
#else
        Com_Printf("landing vel: %.1f fall height: %.1f\n",
                   (double)landingVelocity, (double)fallHeight);
#endif
        move = pm;
    }

    /*
     * 0x30009de2..0x30009e87: map the configured [min,max] fall-height envelope
     * to integer damage [0,100]. Invalid thresholds, excluded pmTypes, or the
     * no-fall-damage state suppress the result.
     */
    float maxHeight = bg_fallDamageMaxHeight.value;
    float minHeightForRange = bg_fallDamageMinHeight.value;
    if (!(maxHeight <= minHeightForRange)) {
        float minHeightForSign = bg_fallDamageMinHeight.value;
        if (!(minHeightForSign < 0.0f)) {
            float minHeightForFall = bg_fallDamageMinHeight.value;
            if (!(fallHeight <= minHeightForFall) &&
                ((uint8_t)pml.groundTrace.surfaceFlags &
                 SURF_NODAMAGE) == 0) {
                playerState_t *damagePs = move->ps;
                if (damagePs->pmType < PM_TYPE_DEAD) {
                    float maxHeightForCap = bg_fallDamageMaxHeight.value;
                    if (fallHeight >= maxHeightForCap) {
                        fallDamage = 100;
                    } else {
                        float minHeightForNumerator = bg_fallDamageMinHeight.value;
#if !EMULATE_X87
                        long double numerator =
                            (long double)fallHeight - minHeightForNumerator;
#endif
                        float maxHeightForDenominator =
                            bg_fallDamageMaxHeight.value;
                        float minHeightForDenominator =
                            bg_fallDamageMinHeight.value;
#if !EMULATE_X87
                        long double denominator =
                            (long double)maxHeightForDenominator
                            - minHeightForDenominator;
#endif
#if EMULATE_X87
                        fallDamage = (int32_t)(uint32_t)x87f_store_i64_trunc(
                            x87f_mul(
                                x87f_div(
                                    x87f_sub(x87f_load_f32(fallHeight),
                                             x87f_load_f32(
                                                 minHeightForNumerator)),
                                    x87f_sub(
                                        x87f_load_f32(
                                            maxHeightForDenominator),
                                        x87f_load_f32(
                                            minHeightForDenominator))),
                                x87f_load_f32(PM_CRASH_DAMAGE_SCALE)));
#else
                        fallDamage = coduo_fp_to_i32_extended(
                            numerator / denominator * PM_CRASH_DAMAGE_SCALE);
#endif
                        if (fallDamage < 0) {
                            fallDamage = 0;
                        } else if (fallDamage > 100) {
                            fallDamage = 100;
                        }
                    }
                }
            }
        }
    }

    /* 0x30009e8b..0x30009ea3: waist-deep water halves and re-rounds damage. */
    if (move->waterlevel == PM_CRASH_WATERLEVEL_TWO) {
        /* fallDamage enters via bare FILD (0x30009e94) -- no (float) cast. */
#if EMULATE_X87
        fallDamage = (int32_t)(uint32_t)x87f_store_i64_trunc(x87f_mul(
            x87f_load_i32(fallDamage),
            x87f_load_f32(PM_CRASH_WATERLEVEL_TWO_SCALE)));
#else
        fallDamage = coduo_fp_to_i32_extended(
            (long double)fallDamage *
            (long double)PM_CRASH_WATERLEVEL_TWO_SCALE);
#endif
    }

    /* 0x30009ea5..0x30009ee7: derive the capped landing-event parameter. */
    if (!(fallHeight <= PM_CRASH_EVENT_HEIGHT_BASE)) {
#if EMULATE_X87
        landingEventParm = (int32_t)(uint32_t)x87f_store_i64_trunc(
            x87f_mul(
                x87f_add(
                    x87f_mul(
                        x87f_sub(
                            x87f_load_f32(fallHeight),
                            x87f_load_f32(PM_CRASH_EVENT_HEIGHT_BASE)),
                        x87f_load_f32(PM_CRASH_EVENT_HEIGHT_SCALE)),
                    x87f_load_f32(PM_CRASH_EVENT_PARM_BIAS)),
                x87f_load_f32(PM_CRASH_EVENT_PARM_SCALE)));
#else
        landingEventParm = coduo_fp_to_i32_extended(
            (((long double)fallHeight -
              (long double)PM_CRASH_EVENT_HEIGHT_BASE) *
                 (long double)PM_CRASH_EVENT_HEIGHT_SCALE +
             (long double)PM_CRASH_EVENT_PARM_BIAS) *
            (long double)PM_CRASH_EVENT_PARM_SCALE);
#endif
        if (landingEventParm > PM_CRASH_EVENT_PARM_MAX) {
            landingEventParm = PM_CRASH_EVENT_PARM_MAX;
        }
    }

    if (fallDamage != 0) {
        /*
         * 0x30009eef..0x30009f55: when no legs animation is active, choose and
         * run one matching command from landing event list 5.
         */
        playerState_t *landingAnimPs = move->ps; /* 0x30009ef0 reload. */
        if (landingAnimPs->legsTimer == 0) {
            /* Linux retains this source-level call. Both Windows optimizers
             * inline BG_AnimScriptEvent's event-list selection here. */
            BG_AnimScriptEvent(landingAnimPs, ANIM_EVENT_LAND,
                               qfalse, qtrue);
            move = pm;
        }

        if (move->debugMove != 0) {
            Com_Printf("falling damage: %i\n", fallDamage);
            move = pm;
        }

        if (fallDamage < 100 &&
            ((uint8_t)pml.groundTrace.surfaceFlags & SURF_SLICK) == 0) {
            int32_t stunTime = fallDamage * PM_CRASH_STUN_DAMAGE_MSEC +
                               PM_CRASH_STUN_BASE_MSEC;
            float speedScale;

            if (stunTime > PM_CRASH_STUN_MAX_MSEC) {
                stunTime = PM_CRASH_STUN_MAX_MSEC;
                speedScale = PM_CRASH_STUN_SPEED_LOW;
            } else if (stunTime <= PM_CRASH_STUN_BASE_MSEC) {
                speedScale = PM_CRASH_STUN_SPEED_HIGH;
            } else if (stunTime >= PM_CRASH_STUN_RAMP_END_MSEC) {
                speedScale = PM_CRASH_STUN_SPEED_LOW;
            } else {
                /* stunTime enters via bare FILD (0x30009fd4) -- no (float) cast;
                 * 500 stays the float constant the FSUB 500.0f loads. */
#if EMULATE_X87
                speedScale = x87f_store_f32(x87f_sub(
                    x87f_load_f32(PM_CRASH_STUN_SPEED_HIGH),
                    x87f_mul(
                        x87f_mul(
                            x87f_sub(
                                x87f_load_i32(stunTime),
                                x87f_load_f32(
                                    (float)PM_CRASH_STUN_BASE_MSEC)),
                            x87f_load_f32(PM_CRASH_STUN_MSEC_TO_SEC)),
                        x87f_load_f32(PM_CRASH_STUN_SPEED_SPAN))));
#else
                speedScale = (float)(
                    (long double)PM_CRASH_STUN_SPEED_HIGH -
                    ((long double)stunTime -
                     (long double)PM_CRASH_STUN_BASE_MSEC) *
                        (long double)PM_CRASH_STUN_MSEC_TO_SEC *
                        (long double)PM_CRASH_STUN_SPEED_SPAN);
#endif
            }

            if (move->debugMove > 1) {
                Com_Printf("landing stun time: %i speed mult: %.2f\n",
                           stunTime, (double)speedScale);
                move = pm;
            }

            playerState_t *timerPs = move->ps;
            long double liveSpeedScale = speedScale;
            timerPs->pmTime = stunTime;
            playerState_t *flagPs = move->ps;
            flagPs->playerStateFlags |= PMF_LAND_STUN;
            playerState_t *velocityXPs = move->ps;
            velocityXPs->velocity[0] =
                (float)(liveSpeedScale * velocityXPs->velocity[0]);
            playerState_t *velocityYPs = move->ps;
            velocityYPs->velocity[1] =
                (float)((long double)speedScale * velocityYPs->velocity[1]);
            playerState_t *velocityZPs = move->ps;
            velocityZPs->velocity[2] =
                (float)((long double)speedScale * velocityZPs->velocity[2]);
        } else {
            playerState_t *velocityXPs = move->ps;
            velocityXPs->velocity[0] =
                (float)((long double)velocityXPs->velocity[0] * PM_CRASH_VELOCITY_DAMP);
            playerState_t *velocityYPs = move->ps;
            velocityYPs->velocity[1] =
                (float)((long double)velocityYPs->velocity[1] * PM_CRASH_VELOCITY_DAMP);
            playerState_t *velocityZPs = move->ps;
            velocityZPs->velocity[2] =
                (float)((long double)velocityZPs->velocity[2] * PM_CRASH_VELOCITY_DAMP);
        }

        {
            int32_t event = PM_DamageLandingForSurface();
            ps = move->ps;
            BG_AddPredictableEventToPlayerstate(event, fallDamage, ps);
        }
        return;
    }

    /*
     * 0x3000a08c..0x3000a11c: non-damaging landings select three height bands.
     * The smallest two use PM_AddEvent (implicit parm 0); the >=12-unit family
     * damps velocity and appends the explicit landingEventParm.
     */
    if (!(fallHeight > PM_CRASH_MINOR_HEIGHT)) {
        return;
    }
    if (fallHeight < PM_CRASH_MEDIUM_HEIGHT) {
        PM_AddEvent(PM_LightLandingForSurface());
        return;
    }
    if (fallHeight < PM_CRASH_EVENT_HEIGHT_BASE) {
        PM_AddEvent(PM_MediumLandingForSurface());
        return;
    }

    playerState_t *velocityXPs = move->ps;
    velocityXPs->velocity[0] =
        (float)((long double)velocityXPs->velocity[0] * PM_CRASH_VELOCITY_DAMP);
    playerState_t *velocityYPs = move->ps;
    velocityYPs->velocity[1] =
        (float)((long double)velocityYPs->velocity[1] * PM_CRASH_VELOCITY_DAMP);
    playerState_t *velocityZPs = move->ps;
    velocityZPs->velocity[2] =
        (float)((long double)velocityZPs->velocity[2] * PM_CRASH_VELOCITY_DAMP);
    {
        int32_t event = PM_HardLandingForSurface();
        ps = move->ps;
        BG_AddPredictableEventToPlayerstate(event, landingEventParm, ps);
    }
}
#else
void PM_CrashLand(void)
{
    float fallVelocity;
    float fallHeight;
    float gravity;
    float minimumDamageHeight;
    float maximumDamageHeight;
    float discriminant;
    float fallDistance;
    float impactVelocity;
    float halfNegativeGravity;
    float impactTime;
    int32_t damagePercent;
    int32_t stepEvent;

    if (pm->waterlevel == PM_CRASH_WATERLEVEL_FULL) {
        return;
    }

    /* Linux stores each named input and each result in binary32.  The
     * discriminant expression itself remains one x87 chain before its store
     * (0x00025d39..0x00025d4f). */
    fallDistance = pml.previousOrigin[2] - pm->ps->psOrigin[2];
    gravity = (float)pm->ps->gravity;
    fallVelocity = pml.previousVelocity[2];
    halfNegativeGravity = -gravity * 0.5f;

#if EMULATE_X87
    discriminant = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(fallVelocity),
                 x87f_load_f32(fallVelocity)),
        x87f_mul(
            x87f_mul(x87f_load_f32(halfNegativeGravity),
                     x87f_load_f32(4.0f)),
            x87f_load_f32(fallDistance))));
#else
    discriminant = (float)(
        (long double)fallVelocity * (long double)fallVelocity -
        ((long double)halfNegativeGravity * 4.0L) *
            (long double)fallDistance);
#endif
    if (discriminant < 0.0f) {
        return;
    }

    /* glibc sqrt returns binary64. Linux then continues in x87 width but
     * stores impactTime, impactVelocity, and fallHeight to binary32 in turn. */
#if EMULATE_X87
    impactTime = x87f_store_f32(x87f_div(
        x87f_sub(x87f_neg(x87f_load_f32(fallVelocity)),
                 x87f_load_f64(CoduoLibm_Sqrt((double)discriminant))),
        x87f_add(x87f_load_f32(halfNegativeGravity),
                 x87f_load_f32(halfNegativeGravity))));
    impactVelocity = x87f_store_f32(x87f_neg(x87f_add(
        x87f_mul(x87f_load_f32(impactTime),
                 x87f_neg(x87f_load_f32(gravity))),
        x87f_load_f32(fallVelocity))));
    fallHeight = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(impactVelocity),
                 x87f_load_f32(impactVelocity)),
        x87f_add(x87f_load_f32(gravity),
                 x87f_load_f32(gravity))));
#else
    impactTime = (float)(
        (-(long double)fallVelocity -
         (long double)CoduoLibm_Sqrt((double)discriminant)) /
        ((long double)halfNegativeGravity +
         (long double)halfNegativeGravity));
    impactVelocity = (float)(-
        ((long double)impactTime * -(long double)gravity +
         (long double)fallVelocity));
    fallHeight = (float)(
        ((long double)impactVelocity * (long double)impactVelocity) /
        ((long double)gravity + (long double)gravity));
#endif

    if (pm->debugMove != 0) {
        Com_Printf("landing vel: %.1f fall height: %.1f\n",
                   (double)impactVelocity, (double)fallHeight);
    }

    minimumDamageHeight = bg_fallDamageMinHeight.value;
    maximumDamageHeight = bg_fallDamageMaxHeight.value;
    if (maximumDamageHeight <= minimumDamageHeight ||
        minimumDamageHeight < 0.0f) {
        Com_Printf("bg_fallDamageMaxHeight and bg_fallDamageMinHeight "
                   "have bad values\n");
        damagePercent = 0;
    } else if (fallHeight <= minimumDamageHeight ||
               (pml.groundTrace.surfaceFlags & SURF_NODAMAGE) != 0 ||
               pm->ps->pmType >= PM_TYPE_DEAD) {
        damagePercent = 0;
    } else if (maximumDamageHeight <= fallHeight) {
        damagePercent = 100;
    } else {
#if EMULATE_X87
        damagePercent = x87f_store_i32_trunc(x87f_mul(
            x87f_div(
                x87f_sub(x87f_load_f32(fallHeight),
                         x87f_load_f32(minimumDamageHeight)),
                x87f_sub(x87f_load_f32(maximumDamageHeight),
                         x87f_load_f32(minimumDamageHeight))),
            x87f_load_f32(100.0f)));
#else
        damagePercent = coduo_fp_to_i32_extended(
            ((long double)fallHeight -
             (long double)minimumDamageHeight) /
            ((long double)maximumDamageHeight -
             (long double)minimumDamageHeight) *
            100.0L);
#endif
        if (damagePercent < 0) {
            damagePercent = 0;
        } else if (damagePercent > 100) {
            damagePercent = 100;
        }
    }

    if (pm->waterlevel == PM_CRASH_WATERLEVEL_TWO) {
#if EMULATE_X87
        damagePercent = x87f_store_i32_trunc(x87f_mul(
            x87f_load_i32(damagePercent), x87f_load_f32(0.5f)));
#else
        damagePercent = coduo_fp_to_i32_extended(
            (long double)damagePercent * 0.5L);
#endif
    }

    /* Linux spells the event mapping as (height-12)/26*4+4 and carries it
     * live into the truncating integer conversion. */
    if (fallHeight <= PM_CRASH_EVENT_HEIGHT_BASE) {
        stepEvent = 0;
    } else {
#if EMULATE_X87
        stepEvent = x87f_store_i32_trunc(x87f_add(
            x87f_mul(
                x87f_div(
                    x87f_sub(x87f_load_f32(fallHeight),
                             x87f_load_f32(PM_CRASH_EVENT_HEIGHT_BASE)),
                    x87f_load_f32(26.0f)),
                x87f_load_f32(PM_CRASH_EVENT_PARM_SCALE)),
            x87f_load_f32(PM_CRASH_EVENT_PARM_SCALE)));
#else
        stepEvent = coduo_fp_to_i32_extended(
            ((long double)fallHeight -
             (long double)PM_CRASH_EVENT_HEIGHT_BASE) /
                26.0L *
                (long double)PM_CRASH_EVENT_PARM_SCALE +
            (long double)PM_CRASH_EVENT_PARM_SCALE);
#endif
    }
    if (stepEvent > PM_CRASH_EVENT_PARM_MAX) {
        stepEvent = PM_CRASH_EVENT_PARM_MAX;
    }

    if (damagePercent == 0) {
        if (fallHeight > PM_CRASH_MINOR_HEIGHT) {
            if (fallHeight < PM_CRASH_MEDIUM_HEIGHT) {
                PM_AddEvent(PM_LightLandingForSurface());
            } else if (fallHeight < PM_CRASH_EVENT_HEIGHT_BASE) {
                PM_AddEvent(PM_MediumLandingForSurface());
            } else {
                pm->ps->velocity[0] *= PM_CRASH_VELOCITY_DAMP;
                pm->ps->velocity[1] *= PM_CRASH_VELOCITY_DAMP;
                pm->ps->velocity[2] *= PM_CRASH_VELOCITY_DAMP;
                BG_AddPredictableEventToPlayerstate(
                    PM_HardLandingForSurface(), stepEvent, pm->ps);
            }
        }
        return;
    }

    if (pm->ps->legsTimer == 0) {
        BG_AnimScriptEvent(pm->ps, ANIM_EVENT_LAND, qfalse, qtrue);
    }
    if (pm->debugMove != 0) {
        Com_Printf("falling damage: %i\n", damagePercent);
    }

    if (damagePercent < 100 &&
        (pml.groundTrace.surfaceFlags & SURF_SLICK) == 0) {
        int32_t stunTime =
            damagePercent * PM_CRASH_STUN_DAMAGE_MSEC +
            PM_CRASH_STUN_BASE_MSEC;
        float speedScale;

        if (stunTime > PM_CRASH_STUN_MAX_MSEC) {
            stunTime = PM_CRASH_STUN_MAX_MSEC;
        }
        if (stunTime < PM_CRASH_STUN_BASE_MSEC + 1) {
            speedScale = PM_CRASH_STUN_SPEED_HIGH;
        } else if (stunTime < PM_CRASH_STUN_RAMP_END_MSEC) {
#if EMULATE_X87
            speedScale = x87f_store_f32(x87f_sub(
                x87f_load_f32(PM_CRASH_STUN_SPEED_HIGH),
                x87f_mul(
                    x87f_div(
                        x87f_sub(x87f_load_i32(stunTime),
                                 x87f_load_f32(
                                     (float)PM_CRASH_STUN_BASE_MSEC)),
                        x87f_load_f32(1000.0f)),
                    x87f_load_f32(PM_CRASH_STUN_SPEED_SPAN))));
#else
            speedScale = (float)(
                (long double)PM_CRASH_STUN_SPEED_HIGH -
                ((long double)stunTime -
                 (long double)PM_CRASH_STUN_BASE_MSEC) /
                    1000.0L *
                    (long double)PM_CRASH_STUN_SPEED_SPAN);
#endif
        } else {
            speedScale = PM_CRASH_STUN_SPEED_LOW;
        }

        if (pm->debugMove > 1) {
            Com_Printf("landing stun time: %i speed mult: %.2f\n",
                       stunTime, (double)speedScale);
        }
        pm->ps->pmTime = stunTime;
        pm->ps->playerStateFlags |= PMF_LAND_STUN;
        pm->ps->velocity[0] *= speedScale;
        pm->ps->velocity[1] *= speedScale;
        pm->ps->velocity[2] *= speedScale;
    } else {
        pm->ps->velocity[0] *= PM_CRASH_VELOCITY_DAMP;
        pm->ps->velocity[1] *= PM_CRASH_VELOCITY_DAMP;
        pm->ps->velocity[2] *= PM_CRASH_VELOCITY_DAMP;
    }
    BG_AddPredictableEventToPlayerstate(
        PM_DamageLandingForSurface(), damagePercent, pm->ps);
}
#endif
