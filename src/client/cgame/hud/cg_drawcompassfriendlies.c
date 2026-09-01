// Source: uo_cgame_mp_x86.dll 0x30016570..0x30016bf6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30016570_30016bf6.mcode
//
// CG_DrawCompassFriendlies - refresh the 64 friendly-player compass blips from
// the next snapshot and its packed local report, then draw the recent non-local
// entries around the animated compass ring. The former CG_Asset_Parse label was
// a size-only match; this body is entirely snapshot/team/compass rendering work.

#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"
#include "client/cgame/globals.h"

#include <math.h>
#include <stdint.h>

enum {
    COMPASS_BLIP_LIFETIME_MS = 800,
    COMPASS_CHAT_PULSE_MS = 4000,
    COMPASS_CHAT_PERIOD_MS = 500,
    COMPASS_CHAT_ON_MS = 250,
    COMPASS_FRIEND_ENTITY_CHAT_FLAG = 0x40000,
    COMPASS_LOCAL_CHAT_FLAG = 0x80000
};

#define COMPASS_ANGLE2SHORT (65536.0f / 360.0f)
#define COMPASS_SHORT2ANGLE (360.0f / 65536.0f)
/* NOT a folded deg->rad factor: the machine multiplies by PI (FMUL [0x3007bd88]
 * = 3.1415927f, 0x30016a92) and then DIVIDES by 180.0f (FDIV [0x3007bd50],
 * 0x30016a98), keeping the intermediate 80-bit between the two ops. A folded
 * `(3.1415927f / 180.0f)` constant would round to the single float 0.017453292f
 * and emit one FMUL -- a different result. (That folded constant does exist in
 * this DLL at 0x3007bd70 and other functions use it; this one deliberately
 * does not.) Used as: bearingDeg * COMPASS_PI / COMPASS_DEG_PER_RAD. */
#define COMPASS_PI          3.1415927f     /* 0x3007bd88 */
#define COMPASS_DEG_PER_RAD 180.0f         /* 0x3007bd50 */
#define COMPASS_RING_SCALE 43.75f
#define COMPASS_FRIEND_SIZE 10.0f
#define COMPASS_SLIDE_Y 160.0f
/* |origin[0|1]| <= 1.0 marks a blip parked at the world origin. The machine
 * compares FABS(float) against the DOUBLE 1.0 at .rdata 0x3007bcf8
 * (FCOMP double [0x3007bcf8], 0x3ff0000000000000); there is no 1e-6 epsilon
 * anywhere in the function's machine code. */
#define COMPASS_AT_ORIGIN_MAX 1.0
#define COMPASS_PACKED_COORD_MAX 1024.0f
#define COMPASS_PACKED_COORD_MIN (-1020.0f)
#define COMPASS_PACKED_YAW_SCALE 1.40625f

void CG_DrawCompassFriendlies(const rectDef_t *rect, const vec4_t color)
{
    clientInfo_t *localState = &bgs.clientinfo[cg_nextSnap->ps.psClientNum];
    int32_t localTeam;

    if (localState->infoValid == 0) {
        return;
    }
    localTeam = localState->team;
    if (localTeam == TEAM_FREE || localTeam == TEAM_SPECTATOR) {
        return;
    }

    for (int32_t i = 0; i < cg_nextSnap->numEntities; i++) {
        int32_t entityNum = cg_nextSnap->entities[i].number;
        centity_t *cent = &cg_entities[entityNum];

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (cent->nextState.eType != ET_PLAYER ||
            (cent->nextState.eFlags & EF_DEAD) != 0 ||
            entityNum < 0 ||
            entityNum >= CG_COMPASS_BLIP_COUNT ||
            bgs.clientinfo[entityNum].infoValid == 0 ||
            bgs.clientinfo[entityNum].team != localTeam) {
            continue;
        }

        cgCompassBlip_t *blip = &cg_compassFriendlies[entityNum];
        blip->updateTime = coduo_int32_from_bits(cg_time);
        blip->origin[0] = cent->lerpOrigin[0];
        blip->origin[1] = cent->lerpOrigin[1];
        blip->origin[2] = cent->lerpAngles[1];
        if ((cent->nextState.eFlags & COMPASS_FRIEND_ENTITY_CHAT_FLAG) != 0) {
            int32_t pulseTime = coduo_int32_from_bits(cg_time);
            if (blip->kindOrExpireTime <= pulseTime) {
                blip->kindOrExpireTime = coduo_int32_from_bits(
                    (uint32_t)pulseTime + COMPASS_CHAT_PULSE_MS);
            }
        }
    }

    if (cg_nextSnap->ps.compassFriendInfo != 0) {
        uint32_t packed = cg_nextSnap->ps.compassFriendInfo;
        int32_t index = (int32_t)(packed & 63u);
        cgCompassBlip_t *blip = &cg_compassFriendlies[index];
        vec2_t report = {
            (float)((int32_t)((packed >> 6) & 0x1ffu) * 4 - 1020),
            (float)((int32_t)((packed >> 15) & 0x1ffu) * 4 - 1020)
        };

        blip->updateTime = coduo_int32_from_bits(cg_time);
        if ((report[0] != COMPASS_PACKED_COORD_MAX &&
             report[0] != COMPASS_PACKED_COORD_MIN) &&
            (report[1] != COMPASS_PACKED_COORD_MAX &&
             report[1] != COMPASS_PACKED_COORD_MIN)) {
            vec3_t localOrigin = {
                cg_nextSnap->ps.psOrigin[0],
                cg_nextSnap->ps.psOrigin[1],
                cg_nextSnap->ps.psOrigin[2] + cg_nextSnap->ps.viewHeightCurrent
            };
            AddLeanToPosition(localOrigin, cg_nextSnap->ps.viewAngles[1],
                                  cg_nextSnap->ps.leanFraction,
                                  16.0f, 20.0f);
            blip->origin[0] = localOrigin[0] + report[0];
            blip->origin[1] = localOrigin[1] + report[1];
        } else {
            VectorNormalize2D(report);
            blip->origin[0] = report[0];
            blip->origin[1] = report[1];
        }
        blip->origin[2] = (float)(int8_t)(packed >> 24) * COMPASS_PACKED_YAW_SCALE;

        if ((cg_nextSnap->ps.entityStateFlags & COMPASS_LOCAL_CHAT_FLAG) != 0) {
            int32_t pulseTime = coduo_int32_from_bits(cg_time);
            if (blip->kindOrExpireTime <= pulseTime) {
                blip->kindOrExpireTime = coduo_int32_from_bits(
                    (uint32_t)pulseTime + COMPASS_CHAT_PULSE_MS);
            }
        }
    }

    CG_UpdateCompassOrientation();

    const float centerX = (float)(
        (long double)cg_hudCompassSize_vmCvar.value * (long double)rect->w *
            (long double)0.5f +
        (long double)rect->x);
    const float centerY = (float)(
        ((long double)cg_hudCompassSize_vmCvar.value * (long double)rect->h *
             (long double)0.5f +
         (long double)rect->y) -
        ((long double)cg_hudCompassSize_vmCvar.value - (long double)1.0f) *
            (long double)COMPASS_SLIDE_Y);
    /* 0x30016836..0x30016855 snapshots the input color once before the loop.
     * Every drawable entry replaces alpha before the renderer sees it. */
    vec4_t drawColor = { color[0], color[1], color[2], color[3] };

    for (int32_t i = 0; i < CG_COMPASS_BLIP_COUNT; i++) {
        cgCompassBlip_t *blip = &cg_compassFriendlies[i];
        float distance;
        float sizeDistance;
        uint32_t packedBearing;
        float bearingDeg;

        if (blip->updateTime > coduo_int32_from_bits(cg_time)) {
            blip->updateTime = 0;
        }
        if (blip->updateTime < coduo_int32_from_bits(
                cg_time - COMPASS_BLIP_LIFETIME_MS) ||
            i == cg_nextSnap->ps.psClientNum) {
            continue;
        }

        /* FABS; FCOMP double 1.0 [0x3007bcf8]; TEST AH,0x41 / JP takes the else
         * arm when |coord| > 1.0 (or unordered); <= 1.0 falls through here. */
        if ((double)fabsf(blip->origin[0]) <= COMPASS_AT_ORIGIN_MAX &&
            (double)fabsf(blip->origin[1]) <= COMPASS_AT_ORIGIN_MAX) {
            packedBearing = (uint32_t)coduo_fp_to_i32_extended(
                ((long double)vectoyaw(blip->origin) -
                 (long double)cg_compassRefYaw) *
                (long double)COMPASS_ANGLE2SHORT) & 0xffffu;
            bearingDeg = (float)(int32_t)packedBearing * COMPASS_SHORT2ANGLE;
            distance = cg_hudCompassMaxRange_vmCvar.value;
            /* The at-origin path jumps directly to the radius calculation at
             * 0x30016a42; it does not apply the ordinary min/max size clamp. */
            sizeDistance = distance;
            drawColor[3] = (float)(
                ((long double)1.0f -
                 (long double)cg_hudObjectiveMinAlpha_vmCvar.value) *
                    (long double)0.5f +
                (long double)cg_hudObjectiveMinAlpha_vmCvar.value);
        } else {
            vec3_t delta = {
                blip->origin[0] - cg_refdef.vieworg[0],
                blip->origin[1] - cg_refdef.vieworg[1],
                0.0f
            };
            packedBearing = (uint32_t)coduo_fp_to_i32_extended(
                ((long double)vectoyaw(delta) -
                 (long double)cg_compassRefYaw) *
                (long double)COMPASS_ANGLE2SHORT) & 0xffffu;
            bearingDeg = (float)(int32_t)packedBearing * COMPASS_SHORT2ANGLE;
            distance = (float)coduo_x87_sqrtl(
                (long double)delta[1] * (long double)delta[1] +
                (long double)delta[0] * (long double)delta[0]);
            float clamped = distance;
            if (clamped > cg_hudObjectiveMaxRange_vmCvar.value) {
                clamped = cg_hudObjectiveMaxRange_vmCvar.value;
            } else if (clamped < cg_hudCompassMaxRange_vmCvar.value) {
                clamped = cg_hudCompassMaxRange_vmCvar.value;
            }
            /* 0x300169d5..0x30016a0d: THREE roundings, not one -- the machine
             * spills the numerator (FSTP [ESP+0x68], 0x300169df) and then the
             * quotient (FSTP [ESP+0x68], 0x300169f3) to float slots and reloads
             * each, before the final result store (0x30016a0d). The explicit
             * temps reproduce those two intermediate roundings. */
            float rangeNumer = (float)(
                (long double)clamped -
                (long double)cg_hudCompassMaxRange_vmCvar.value);
            float rangeFrac = (float)(
                (long double)rangeNumer /
                ((long double)cg_hudObjectiveMaxRange_vmCvar.value -
                 (long double)cg_hudCompassMaxRange_vmCvar.value));
            drawColor[3] = (float)(
                ((long double)cg_hudObjectiveMinAlpha_vmCvar.value -
                 (long double)1.0f) * (long double)rangeFrac +
                (long double)1.0f);
            sizeDistance = distance;
            if (sizeDistance > cg_hudCompassMaxRange_vmCvar.value) {
                sizeDistance = cg_hudCompassMaxRange_vmCvar.value;
            } else if (sizeDistance < cg_hudCompassMinRange_vmCvar.value) {
                sizeDistance = cg_hudCompassMinRange_vmCvar.value;
            }
        }
        /* 0x30016a42..0x30016a8a is ONE unbroken 80-bit chain with a SINGLE
         * FSTP (0x30016a8a): the sizeScale sub-expression is never stored, the
         * slide (FMUL [0x3048c4a8], 0x30016a7e) and the ring scale (FMUL
         * [0x3007bf48], 0x30016a84) continue on the same unrounded st0. A
         * separate `float sizeScale` local would round once more than the DLL. */
        float ringRadius = (float)(
            (((long double)sizeDistance -
              (long double)cg_hudCompassMinRange_vmCvar.value) /
                 ((long double)cg_hudCompassMaxRange_vmCvar.value -
                  (long double)cg_hudCompassMinRange_vmCvar.value) *
                 ((long double)1.0f -
                  (long double)cg_hudCompassMinRadius_vmCvar.value) +
             (long double)cg_hudCompassMinRadius_vmCvar.value) *
            (long double)cg_hudCompassSize_vmCvar.value *
            (long double)COMPASS_RING_SCALE);
        /* 0x30016a8e..0x30016a9e: FMUL PI then FDIV 180.0f as two separate ops
         * (see COMPASS_PI above); one rounding, at the FSTP. */
        float bearingRad = (float)(
            (long double)bearingDeg * (long double)COMPASS_PI /
            (long double)COMPASS_DEG_PER_RAD);
        float sinBearing;
        float cosBearing;
        coduo_x87_sincosf(bearingRad, &sinBearing, &cosBearing);
        float quadSize = (float)(
            (long double)cg_hudCompassSize_vmCvar.value *
            (long double)COMPASS_FRIEND_SIZE);
        float x = (float)(
            ((long double)centerX -
             (long double)quadSize * (long double)0.5f) -
            (long double)sinBearing * (long double)ringRadius);
        float y = (float)(
            ((long double)centerY -
             (long double)quadSize * (long double)0.5f) -
            (long double)cosBearing * (long double)ringRadius);
        uint32_t packedRotation = (uint32_t)coduo_fp_to_i32_extended(
            ((long double)cg_refdefViewAngles[1] -
             (long double)blip->origin[2]) *
            (long double)COMPASS_ANGLE2SHORT) & 0xffffu;
        float rotation = (float)(int32_t)packedRotation * COMPASS_SHORT2ANGLE;

        int32_t pulse = 0;
        int32_t pulseTime = coduo_int32_from_bits(cg_time);
        if (blip->kindOrExpireTime > pulseTime) {
            pulse = (coduo_int32_from_bits(
                         (uint32_t)blip->kindOrExpireTime -
                         (uint32_t)pulseTime) %
                     COMPASS_CHAT_PERIOD_MS) >= COMPASS_CHAT_ON_MS;
        }
        cgame_syscall(CG_R_SETCOLOR, (intptr_t)drawColor);
        if (pulse != 0) {
            CG_DrawPic(x, y, quadSize, quadSize, cg_compassFriendlyShaders[1]);
        } else {
            CG_DrawRotatedPic(x, y, quadSize, quadSize, rotation,
                              cg_compassFriendlyShaders[0]);
        }
    }

    cgame_syscall(CG_R_SETCOLOR, 0);
}
