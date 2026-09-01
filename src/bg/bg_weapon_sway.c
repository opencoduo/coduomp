// Source: uo_cgame_mp_x86.dll 0x30015ca0..0x30016112
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015ca0_30016112.mcode
//
// BG_CalculateWeaponPosition_Sway -- blend the active weapon's hip/ADS sway
// envelope, derive view-angle deltas from the saved previous view angles, and
// smoothly update the two sway-output carriers. The G_SetUICvars header name is
// rejected: this body reads player/weaponInfo_t data and performs angle math only.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#define BG_SWAY_MILLISECONDS_TO_SECONDS 0.001f
/* The snap epsilon compared at 0x30015ee7 etc. is the QWORD at 0x3007be28 =
 * 0x3f50624de0000000, which is the FLOAT 0.001f widened to double storage --
 * NOT the double 0.001 (0x3f50624dd2f1a9fc). The f-suffixed literal promotes
 * to exactly that value in the double compares below. */
#define BG_SWAY_SNAP_EPSILON 0.001f
#define BG_SWAY_HALF_TURN 180.0f
#define BG_SWAY_FULL_TURN 360.0f

/*
 * The separately emitted smoothing body is present in both authoritative
 * targets (Windows cgame 0x30015c50; Linux game RVA 0x00039236).  Its source
 * argument order is target, current, rate, msec.  Windows returns the live
 * PC=53 x87 expression, whereas Linux explicitly reloads a binary32 result
 * before returning.  The Windows sway body below inlines the same source
 * operation instead of calling this function; Linux calls it four times.
 */
#if defined(WINDOWS_BEHAVIOR)
long double BG_SmoothWeaponSwayValue(float target, float current, float rate,
                                     int32_t msec)
{
#if EMULATE_X87
    const x87f delta = x87f_sub(x87f_load_f32(target),
                                x87f_load_f32(current));
    const float step = x87f_store_f32(x87f_mul(
        x87f_mul(
            x87f_mul(x87f_load_i32(msec),
                     x87f_load_f32(BG_SWAY_MILLISECONDS_TO_SECONDS)),
            delta),
        x87f_load_f32(rate)));
    const x87f absoluteDelta = x87f_abs(delta);
    const x87f absoluteStep = x87f_abs(x87f_load_f32(step));
    const x87f epsilon = x87f_load_f64((double)BG_SWAY_SNAP_EPSILON);

    if (x87f_lt_signaling(epsilon, absoluteDelta) &&
        !x87f_lt_signaling(absoluteDelta, absoluteStep)) {
        return (long double)x87f_store_f64(
            x87f_add(x87f_load_f32(current), x87f_load_f32(step)));
    }
    return (long double)target;
#else
    const long double delta =
        (long double)target - (long double)current;
    const float step = (float)(
        (long double)msec * (long double)BG_SWAY_MILLISECONDS_TO_SECONDS *
        delta * (long double)rate);
    const long double absoluteDelta = __builtin_fabsl(delta);
    const long double absoluteStep =
        (long double)fabsf(step);

    if (absoluteDelta > (long double)(double)BG_SWAY_SNAP_EPSILON &&
        !(absoluteDelta < absoluteStep)) {
        return (long double)current + (long double)step;
    }
    return (long double)target;
#endif
}
#else
float BG_SmoothWeaponSwayValue(float target, float current, float rate,
                               int32_t msec)
{
    float delta;
    float step;

#if EMULATE_X87
    delta = x87f_store_f32(x87f_sub(x87f_load_f32(target),
                                    x87f_load_f32(current)));
    step = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(rate), x87f_load_f32(delta)),
        x87f_mul(x87f_load_i32(msec),
                 x87f_load_f32(BG_SWAY_MILLISECONDS_TO_SECONDS))));
#else
    delta = target - current;
    step = (float)((long double)rate * (long double)delta *
                   ((long double)msec *
                    (long double)BG_SWAY_MILLISECONDS_TO_SECONDS));
#endif

    if (!(fabsf(delta) > 0.001)) {
        return target;
    }
    if (fabsf(delta) < fabsf(step)) {
        return target;
    }

#if EMULATE_X87
    return x87f_store_f32(x87f_add(x87f_load_f32(current),
                                   x87f_load_f32(step)));
#else
    return current + step;
#endif
}
#endif

#if defined(WINDOWS_BEHAVIOR)
void BG_CalculateWeaponPosition_Sway(const playerState_t *ps,
                                     vec3_t previousViewAngles,
                                     vec3_t outPosition, vec2_t outAngles,
                                     float scale, int frameTime)
{
    weaponInfo_t **weaponTable = bg_weaponInfos;
    float adsFraction = ps->adsFraction;
    int32_t currentWeapon = ps->currentWeapon;
    const weaponInfo_t *weapon = weaponTable[currentWeapon];
    float maxAngle;
    float lerpSpeed;
    float pitchScale;
    float yawScale;
    float horizontalScale;
    float verticalScale;
    float pitchDelta;
    float yawDelta;
    float target;
    float error;
    float step;
    float dt;

    if (weapon->adsEnabled != 0) {
        /* 0x30015cde TEST AH,0x41 / JNZ skips the gate check when
         * adsFraction < 0 OR == 0 (or unordered): the early return needs
         * adsFraction STRICTLY greater than zero. */
        if (adsFraction > 0.0f && weapon->adsOverlayReticle != 0) {
            return;
        }
#if EMULATE_X87
        maxAngle = x87f_store_f32(x87f_add(
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayMaxAngle),
                         x87f_load_f32(weapon->swayMaxAngle)),
                x87f_load_f32(adsFraction)),
            x87f_load_f32(weapon->swayMaxAngle)));
        lerpSpeed = x87f_store_f32(x87f_add(
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayLerpSpeed),
                         x87f_load_f32(weapon->swayLerpSpeed)),
                x87f_load_f32(adsFraction)),
            x87f_load_f32(weapon->swayLerpSpeed)));
        pitchScale = x87f_store_f32(x87f_add(
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayPitchScale),
                         x87f_load_f32(weapon->swayPitchScale)),
                x87f_load_f32(adsFraction)),
            x87f_load_f32(weapon->swayPitchScale)));
        yawScale = x87f_store_f32(x87f_add(
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayYawScale),
                         x87f_load_f32(weapon->swayYawScale)),
                x87f_load_f32(adsFraction)),
            x87f_load_f32(weapon->swayYawScale)));
        horizontalScale = x87f_store_f32(x87f_add(
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayHorizScale),
                         x87f_load_f32(weapon->swayHorizScale)),
                x87f_load_f32(adsFraction)),
            x87f_load_f32(weapon->swayHorizScale)));
        verticalScale = x87f_store_f32(x87f_add(
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayVertScale),
                         x87f_load_f32(weapon->swayVertScale)),
                x87f_load_f32(adsFraction)),
            x87f_load_f32(weapon->swayVertScale)));
#else
        maxAngle = (float)(
            ((long double)weapon->adsSwayMaxAngle -
             (long double)weapon->swayMaxAngle) *
                (long double)adsFraction +
            (long double)weapon->swayMaxAngle);
        lerpSpeed = (float)(
            ((long double)weapon->adsSwayLerpSpeed -
             (long double)weapon->swayLerpSpeed) *
                (long double)adsFraction +
            (long double)weapon->swayLerpSpeed);
        pitchScale = (float)(
            ((long double)weapon->adsSwayPitchScale -
             (long double)weapon->swayPitchScale) *
                (long double)adsFraction +
            (long double)weapon->swayPitchScale);
        yawScale = (float)(
            ((long double)weapon->adsSwayYawScale -
             (long double)weapon->swayYawScale) *
                (long double)adsFraction +
            (long double)weapon->swayYawScale);
        horizontalScale = (float)(
            ((long double)weapon->adsSwayHorizScale -
             (long double)weapon->swayHorizScale) *
                (long double)adsFraction +
            (long double)weapon->swayHorizScale);
        verticalScale = (float)(
            ((long double)weapon->adsSwayVertScale -
             (long double)weapon->swayVertScale) *
                (long double)adsFraction +
            (long double)weapon->swayVertScale);
#endif
    } else {
        maxAngle = weapon->swayMaxAngle;
        lerpSpeed = weapon->swayLerpSpeed;
        pitchScale = weapon->swayPitchScale;
        yawScale = weapon->swayYawScale;
        horizontalScale = weapon->swayHorizScale;
        verticalScale = weapon->swayVertScale;
    }

#if EMULATE_X87
    pitchScale = x87f_store_f32(x87f_mul(x87f_load_f32(pitchScale),
                                         x87f_load_f32(scale)));
    yawScale = x87f_store_f32(x87f_mul(x87f_load_f32(yawScale),
                                       x87f_load_f32(scale)));
    horizontalScale = x87f_store_f32(x87f_mul(
        x87f_load_f32(horizontalScale), x87f_load_f32(scale)));
    verticalScale = x87f_store_f32(x87f_mul(
        x87f_load_f32(verticalScale), x87f_load_f32(scale)));
#else
    pitchScale *= scale;
    yawScale *= scale;
    horizontalScale *= scale;
    verticalScale *= scale;
#endif

    pitchDelta = AngleSubtract(ps->viewAngles[0], previousViewAngles[0]);
    yawDelta = AngleSubtract(ps->viewAngles[1], previousViewAngles[1]);

    /* Both deltas are clamped to +/-maxAngle. The negated bound is built by
     * the FLD at 0x30015e27, which executes while the four AngleSubtract args
     * are still pushed: [ESP+0x1c] there is frame slot +0xc, the maxAngle
     * blend (NOT the yawScale slot the raw displacement suggests). FCHS then
     * stores -maxAngle for both lower-bound FCOMs (0x30015e3c/0x30015e64). */
    if (pitchDelta < -maxAngle) {
        pitchDelta = -maxAngle;
    } else if (pitchDelta > maxAngle) {
        pitchDelta = maxAngle;
    }
    if (yawDelta < -maxAngle) {
        yawDelta = -maxAngle;
    } else if (yawDelta > maxAngle) {
        yawDelta = maxAngle;
    }

#if EMULATE_X87
    dt = x87f_store_f32(x87f_load_i32(frameTime));
    dt = x87f_store_f32(x87f_mul(
        x87f_load_f32(dt),
        x87f_load_f32(BG_SWAY_MILLISECONDS_TO_SECONDS)));
#else
    dt = (float)frameTime * BG_SWAY_MILLISECONDS_TO_SECONDS;
#endif

    /* Snap-condition polarity at every carrier (0x30015eef/0x30015f00 etc.):
     * the step-add path requires |error| STRICTLY greater than the epsilon
     * (TEST AH,0x41/JNZ snaps on less-or-equal), and is taken when
     * |step| <= |error| OR the compare is unordered (TEST AH,0x41/JZ snaps
     * only on strictly-greater) -- hence `>` and `!(>)` below. */
#if EMULATE_X87
    target = x87f_store_f32(x87f_mul(x87f_load_f32(yawDelta),
                                     x87f_load_f32(horizontalScale)));
    error = x87f_store_f32(x87f_sub(x87f_load_f32(target),
                                    x87f_load_f32(outPosition[1])));
    step = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(dt), x87f_load_f32(error)),
        x87f_load_f32(lerpSpeed)));
    if (x87f_lt_signaling(
            x87f_load_f64((double)BG_SWAY_SNAP_EPSILON),
            x87f_abs(x87f_load_f32(error))) &&
        !x87f_lt_signaling(x87f_abs(x87f_load_f32(error)),
                           x87f_abs(x87f_load_f32(step)))) {
        outPosition[1] = x87f_store_f32(x87f_add(
            x87f_load_f32(outPosition[1]), x87f_load_f32(step)));
    } else {
        outPosition[1] = target;
    }

    target = x87f_store_f32(x87f_mul(x87f_load_f32(pitchDelta),
                                     x87f_load_f32(verticalScale)));
    error = x87f_store_f32(x87f_sub(x87f_load_f32(target),
                                    x87f_load_f32(outPosition[2])));
    step = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(dt), x87f_load_f32(error)),
        x87f_load_f32(lerpSpeed)));
    if (x87f_lt_signaling(
            x87f_load_f64((double)BG_SWAY_SNAP_EPSILON),
            x87f_abs(x87f_load_f32(error))) &&
        !x87f_lt_signaling(x87f_abs(x87f_load_f32(error)),
                           x87f_abs(x87f_load_f32(step)))) {
        outPosition[2] = x87f_store_f32(x87f_add(
            x87f_load_f32(outPosition[2]), x87f_load_f32(step)));
    } else {
        outPosition[2] = target;
    }
#else
    target = yawDelta * horizontalScale;
    error = target - outPosition[1];
    step = dt * error * lerpSpeed;
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outPosition[1] += step;
    } else {
        outPosition[1] = target;
    }

    target = pitchDelta * verticalScale;
    error = target - outPosition[2];
    step = dt * error * lerpSpeed;
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outPosition[2] += step;
    } else {
        outPosition[2] = target;
    }
#endif

#if EMULATE_X87
    target = x87f_store_f32(x87f_mul(x87f_load_f32(pitchDelta),
                                     x87f_load_f32(pitchScale)));
    while (x87f_lt_signaling(
        x87f_load_f32(BG_SWAY_HALF_TURN),
        x87f_sub(x87f_load_f32(target),
                 x87f_load_f32(outAngles[0])))) {
        target = x87f_store_f32(x87f_sub(
            x87f_load_f32(target), x87f_load_f32(BG_SWAY_FULL_TURN)));
    }
    error = x87f_store_f32(x87f_sub(x87f_load_f32(target),
                                    x87f_load_f32(outAngles[0])));
    step = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(dt), x87f_load_f32(error)),
        x87f_load_f32(lerpSpeed)));
    if (x87f_lt_signaling(
            x87f_load_f64((double)BG_SWAY_SNAP_EPSILON),
            x87f_abs(x87f_load_f32(error))) &&
        !x87f_lt_signaling(x87f_abs(x87f_load_f32(error)),
                           x87f_abs(x87f_load_f32(step)))) {
        outAngles[0] = x87f_store_f32(x87f_add(
            x87f_load_f32(outAngles[0]), x87f_load_f32(step)));
    } else {
        outAngles[0] = target;
    }

    target = x87f_store_f32(x87f_mul(x87f_load_f32(yawDelta),
                                     x87f_load_f32(yawScale)));
    while (x87f_lt_signaling(
        x87f_load_f32(BG_SWAY_HALF_TURN),
        x87f_sub(x87f_load_f32(target),
                 x87f_load_f32(outAngles[1])))) {
        target = x87f_store_f32(x87f_sub(
            x87f_load_f32(target), x87f_load_f32(BG_SWAY_FULL_TURN)));
    }
    error = x87f_store_f32(x87f_sub(x87f_load_f32(target),
                                    x87f_load_f32(outAngles[1])));
    step = x87f_store_f32(x87f_mul(
        x87f_mul(x87f_load_f32(dt), x87f_load_f32(error)),
        x87f_load_f32(lerpSpeed)));
    if (x87f_lt_signaling(
            x87f_load_f64((double)BG_SWAY_SNAP_EPSILON),
            x87f_abs(x87f_load_f32(error))) &&
        !x87f_lt_signaling(x87f_abs(x87f_load_f32(error)),
                           x87f_abs(x87f_load_f32(step)))) {
        outAngles[1] = x87f_store_f32(x87f_add(
            x87f_load_f32(outAngles[1]), x87f_load_f32(step)));
    } else {
        outAngles[1] = target;
    }
#else
    target = pitchDelta * pitchScale;
    while (target - outAngles[0] > BG_SWAY_HALF_TURN) {
        target -= BG_SWAY_FULL_TURN;
    }
    error = target - outAngles[0];
    step = dt * error * lerpSpeed;
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outAngles[0] += step;
    } else {
        outAngles[0] = target;
    }

    target = yawDelta * yawScale;
    while (target - outAngles[1] > BG_SWAY_HALF_TURN) {
        target -= BG_SWAY_FULL_TURN;
    }
    error = target - outAngles[1];
    step = dt * error * lerpSpeed;
    if (fabs((double)error) > BG_SWAY_SNAP_EPSILON &&
        !(fabs((double)step) > fabs((double)error))) {
        outAngles[1] += step;
    } else {
        outAngles[1] = target;
    }
#endif

    outAngles[0] = AngleNormalize180(outAngles[0]);
    outAngles[1] = AngleNormalize180(outAngles[1]);
    previousViewAngles[0] = ps->viewAngles[0];
    previousViewAngles[1] = ps->viewAngles[1];
    previousViewAngles[2] = ps->viewAngles[2];
}
#else
void BG_CalculateWeaponPosition_Sway(const playerState_t *ps,
                                     vec3_t previousViewAngles,
                                     vec3_t outPosition, vec2_t outAngles,
                                     float scale, int32_t frameTime)
{
    const weaponInfo_t *weapon = BG_GetInfoForWeapon(ps->currentWeapon);
    float adsFraction = ps->adsFraction;
    float maxAngle;
    float lerpSpeed;
    float pitchScale;
    float yawScale;
    float horizontalScale;
    float verticalScale;
    vec3_t viewDelta;
    float pitchDelta;
    float yawDelta;
    float targetPitch;
    float targetYaw;

    if (weapon->adsEnabled != 0) {
        if (adsFraction > 0.0f && weapon->adsOverlayReticle != 0) {
            return;
        }
#if EMULATE_X87
        maxAngle = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->swayMaxAngle),
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayMaxAngle),
                         x87f_load_f32(weapon->swayMaxAngle)),
                x87f_load_f32(adsFraction))));
        lerpSpeed = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->swayLerpSpeed),
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayLerpSpeed),
                         x87f_load_f32(weapon->swayLerpSpeed)),
                x87f_load_f32(adsFraction))));
        pitchScale = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->swayPitchScale),
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayPitchScale),
                         x87f_load_f32(weapon->swayPitchScale)),
                x87f_load_f32(adsFraction))));
        yawScale = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->swayYawScale),
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayYawScale),
                         x87f_load_f32(weapon->swayYawScale)),
                x87f_load_f32(adsFraction))));
        horizontalScale = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->swayHorizScale),
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayHorizScale),
                         x87f_load_f32(weapon->swayHorizScale)),
                x87f_load_f32(adsFraction))));
        verticalScale = x87f_store_f32(x87f_add(
            x87f_load_f32(weapon->swayVertScale),
            x87f_mul(
                x87f_sub(x87f_load_f32(weapon->adsSwayVertScale),
                         x87f_load_f32(weapon->swayVertScale)),
                x87f_load_f32(adsFraction))));
#else
        maxAngle = weapon->swayMaxAngle +
                   (weapon->adsSwayMaxAngle - weapon->swayMaxAngle) *
                       adsFraction;
        lerpSpeed = weapon->swayLerpSpeed +
                    (weapon->adsSwayLerpSpeed - weapon->swayLerpSpeed) *
                        adsFraction;
        pitchScale = weapon->swayPitchScale +
                     (weapon->adsSwayPitchScale - weapon->swayPitchScale) *
                         adsFraction;
        yawScale = weapon->swayYawScale +
                   (weapon->adsSwayYawScale - weapon->swayYawScale) *
                       adsFraction;
        horizontalScale = weapon->swayHorizScale +
                          (weapon->adsSwayHorizScale -
                           weapon->swayHorizScale) * adsFraction;
        verticalScale = weapon->swayVertScale +
                        (weapon->adsSwayVertScale - weapon->swayVertScale) *
                            adsFraction;
#endif
    } else {
        adsFraction = 0.0f;
        maxAngle = weapon->swayMaxAngle;
        lerpSpeed = weapon->swayLerpSpeed;
        pitchScale = weapon->swayPitchScale;
        yawScale = weapon->swayYawScale;
        horizontalScale = weapon->swayHorizScale;
        verticalScale = weapon->swayVertScale;
    }

#if EMULATE_X87
    pitchScale = x87f_store_f32(x87f_mul(x87f_load_f32(pitchScale),
                                         x87f_load_f32(scale)));
    yawScale = x87f_store_f32(x87f_mul(x87f_load_f32(yawScale),
                                       x87f_load_f32(scale)));
    horizontalScale = x87f_store_f32(x87f_mul(
        x87f_load_f32(horizontalScale), x87f_load_f32(scale)));
    verticalScale = x87f_store_f32(x87f_mul(
        x87f_load_f32(verticalScale), x87f_load_f32(scale)));
#else
    pitchScale *= scale;
    yawScale *= scale;
    horizontalScale *= scale;
    verticalScale *= scale;
#endif

    AnglesSubtract(ps->viewAngles, previousViewAngles, viewDelta);
    pitchDelta = viewDelta[0];
    yawDelta = viewDelta[1];
    if (pitchDelta < -maxAngle) {
        pitchDelta = -maxAngle;
    } else if (pitchDelta > maxAngle) {
        pitchDelta = maxAngle;
    }
    if (yawDelta < -maxAngle) {
        yawDelta = -maxAngle;
    } else if (yawDelta > maxAngle) {
        yawDelta = maxAngle;
    }

#if EMULATE_X87
    outPosition[1] = BG_SmoothWeaponSwayValue(
        x87f_store_f32(x87f_mul(x87f_load_f32(yawDelta),
                                x87f_load_f32(horizontalScale))),
        outPosition[1], lerpSpeed, frameTime);
    outPosition[2] = BG_SmoothWeaponSwayValue(
        x87f_store_f32(x87f_mul(x87f_load_f32(pitchDelta),
                                x87f_load_f32(verticalScale))),
        outPosition[2], lerpSpeed, frameTime);
    targetPitch = x87f_store_f32(x87f_mul(x87f_load_f32(pitchDelta),
                                          x87f_load_f32(pitchScale)));
    targetYaw = x87f_store_f32(x87f_mul(x87f_load_f32(yawDelta),
                                        x87f_load_f32(yawScale)));
#else
    outPosition[1] = BG_SmoothWeaponSwayValue(
        yawDelta * horizontalScale, outPosition[1], lerpSpeed, frameTime);
    outPosition[2] = BG_SmoothWeaponSwayValue(
        pitchDelta * verticalScale, outPosition[2], lerpSpeed, frameTime);
    targetPitch = pitchDelta * pitchScale;
    targetYaw = yawDelta * yawScale;
#endif

#if EMULATE_X87
    while (x87f_lt(
        x87f_load_f32(BG_SWAY_HALF_TURN),
        x87f_sub(x87f_load_f32(targetPitch),
                 x87f_load_f32(outAngles[0])))) {
        targetPitch = x87f_store_f32(x87f_sub(
            x87f_load_f32(targetPitch),
            x87f_load_f32(BG_SWAY_FULL_TURN)));
    }
    while (x87f_lt(
        x87f_load_f32(BG_SWAY_HALF_TURN),
        x87f_sub(x87f_load_f32(targetYaw),
                 x87f_load_f32(outAngles[1])))) {
        targetYaw = x87f_store_f32(x87f_sub(
            x87f_load_f32(targetYaw),
            x87f_load_f32(BG_SWAY_FULL_TURN)));
    }
#else
    while ((long double)targetPitch - (long double)outAngles[0] >
           (long double)BG_SWAY_HALF_TURN) {
        targetPitch -= BG_SWAY_FULL_TURN;
    }
    while ((long double)targetYaw - (long double)outAngles[1] >
           (long double)BG_SWAY_HALF_TURN) {
        targetYaw -= BG_SWAY_FULL_TURN;
    }
#endif

    outAngles[0] = BG_SmoothWeaponSwayValue(
        targetPitch, outAngles[0], lerpSpeed, frameTime);
    outAngles[1] = BG_SmoothWeaponSwayValue(
        targetYaw, outAngles[1], lerpSpeed, frameTime);
    outAngles[0] = AngleNormalize180(outAngles[0]);
    outAngles[1] = AngleNormalize180(outAngles[1]);
    previousViewAngles[0] = ps->viewAngles[0];
    previousViewAngles[1] = ps->viewAngles[1];
    previousViewAngles[2] = ps->viewAngles[2];
}
#endif
