// Source: uo_cgame_mp_x86.dll 0x30016c00..0x3001720d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30016c00_3001720d.mcode
//
// CG_DrawCompassTanks - refresh and render the 64 allied vehicle compass blips.

#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"
#include "client/cgame/globals.h"

#include <math.h>
#include <stdint.h>

enum { TANK_COMPASS_BLIP_LIFETIME_MS = 800 };

#define TANK_COMPASS_ANGLE2SHORT (65536.0f / 360.0f)
#define TANK_COMPASS_SHORT2ANGLE (360.0f / 65536.0f)
/* NOT a folded deg->rad factor: the machine multiplies by PI (FMUL [0x3007bd88]
 * = 3.1415927f, 0x300170b5) and then DIVIDES by 180.0f (FDIV [0x3007bd50],
 * 0x300170bb), keeping the intermediate 80-bit between the two ops. A folded
 * `(3.1415927f / 180.0f)` constant would round to the single float 0.017453292f
 * and emit one FMUL -- a different result. (That folded constant does exist in
 * this DLL at 0x3007bd70 and other functions use it; this one deliberately
 * does not.) Same idiom as the friendlies compass sibling. */
#define TANK_COMPASS_PI          3.1415927f     /* 0x3007bd88 */
#define TANK_COMPASS_DEG_PER_RAD 180.0f         /* 0x3007bd50 */
#define TANK_COMPASS_RING_SCALE 43.75f
#define TANK_COMPASS_ICON_SIZE 16.0f
#define TANK_COMPASS_SLIDE_Y 160.0f
/* |origin[0|1]| <= 1.0 marks a blip parked at the world origin. The machine
 * compares FABS(float) against the DOUBLE 1.0 at .rdata 0x3007bcf8
 * (FCOMP double [0x3007bcf8], 0x3ff0000000000000); there is no 1e-6 epsilon
 * anywhere in the function's machine code. */
#define TANK_COMPASS_AT_ORIGIN_MAX 1.0
#define TANK_COMPASS_PACKED_COORD_MAX 1024.0f
#define TANK_COMPASS_PACKED_COORD_MIN (-1020.0f)
#define TANK_COMPASS_PACKED_YAW_SCALE 1.40625f

void CG_DrawCompassTanks(const rectDef_t *rect, const vec4_t color)
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
        int32_t occupant = cent->currentState.vehicleEntityNum;

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (cent->nextState.eType != ET_VEHICLE || occupant == 1023 ||
            occupant == cg_nextSnap->ps.psClientNum || cent->currentValid == 0 ||
            occupant < 0 || occupant >= CG_COMPASS_BLIP_COUNT ||
            bgs.clientinfo[occupant].team != localTeam ||
            cent->currentState.compassBlipIndex < 0 ||
            cent->currentState.compassBlipIndex >= CG_COMPASS_BLIP_COUNT) {
            continue;
        }

        cgCompassBlip_t *blip = &cg_compassTanks[cent->currentState.compassBlipIndex];
        blip->updateTime = coduo_int32_from_bits(cg_time);
        blip->origin[0] = cent->lerpOrigin[0];
        blip->origin[1] = cent->lerpOrigin[1];
        blip->kindOrExpireTime = cent->currentState.stateFilter;
        blip->origin[2] = cent->currentState.stateFilter == 5
                            ? cent->currentState.loopedFxInterval
                            : cent->lerpAngles[1];
    }

    if (cg_nextSnap->ps.compassTankInfo != 0) {
        uint32_t packed = cg_nextSnap->ps.compassTankInfo;
        int32_t index = (int32_t)(packed & 63u);
        cgCompassBlip_t *blip = &cg_compassTanks[index];
        vec2_t report = {
            (float)((int32_t)((packed >> 6) & 0x1ffu) * 4 - 1020),
            (float)((int32_t)((packed >> 15) & 0x1ffu) * 4 - 1020)
        };

        blip->updateTime = coduo_int32_from_bits(cg_time);
        if ((report[0] != TANK_COMPASS_PACKED_COORD_MAX &&
             report[0] != TANK_COMPASS_PACKED_COORD_MIN) &&
            (report[1] != TANK_COMPASS_PACKED_COORD_MAX &&
             report[1] != TANK_COMPASS_PACKED_COORD_MIN)) {
            blip->origin[0] = cg_nextSnap->ps.psOrigin[0] + report[0];
            blip->origin[1] = cg_nextSnap->ps.psOrigin[1] + report[1];
        } else {
            VectorNormalize2D(report);
            blip->origin[0] = report[0];
            blip->origin[1] = report[1];
        }
        blip->origin[2] = (float)(int8_t)(packed >> 24) *
                          TANK_COMPASS_PACKED_YAW_SCALE;
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
            (long double)TANK_COMPASS_SLIDE_Y);
    /* The machine snapshots the whole input color once before entering the
     * loop. RGB is replaced with hardcoded green immediately before each draw. */
    vec4_t drawColor = { color[0], color[1], color[2], color[3] };

    for (int32_t i = 0; i < CG_COMPASS_BLIP_COUNT; i++) {
        cgCompassBlip_t *blip = &cg_compassTanks[i];
        float distance;
        float sizeDistance;
        uint32_t packedBearing;
        float bearingDeg;

        if (blip->updateTime > coduo_int32_from_bits(cg_time)) {
            blip->updateTime = 0;
        }
        if (blip->updateTime < coduo_int32_from_bits(
                cg_time - TANK_COMPASS_BLIP_LIFETIME_MS)) {
            continue;
        }

        /* FABS; FCOMP double 1.0 [0x3007bcf8]; TEST AH,0x41 / JP takes the else
         * arm when |coord| > 1.0 (or unordered); <= 1.0 falls through here. */
        if ((double)fabsf(blip->origin[0]) <= TANK_COMPASS_AT_ORIGIN_MAX &&
            (double)fabsf(blip->origin[1]) <= TANK_COMPASS_AT_ORIGIN_MAX) {
            packedBearing = (uint32_t)coduo_fp_to_i32_extended(
                ((long double)vectoyaw(blip->origin) -
                 (long double)cg_compassRefYaw) *
                (long double)TANK_COMPASS_ANGLE2SHORT) & 0xffffu;
            bearingDeg = (float)(int32_t)packedBearing * TANK_COMPASS_SHORT2ANGLE;
            distance = cg_hudCompassMaxRange_vmCvar.value;
            /* 0x30016f3e jumps directly to 0x30017065; this path deliberately
             * skips the ordinary min/max size-distance clamp. */
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
                (long double)TANK_COMPASS_ANGLE2SHORT) & 0xffffu;
            bearingDeg = (float)(int32_t)packedBearing * TANK_COMPASS_SHORT2ANGLE;
            distance = (float)coduo_x87_sqrtl(
                (long double)delta[1] * (long double)delta[1] +
                (long double)delta[0] * (long double)delta[0]);
            float clamped = distance;
            if (clamped > cg_hudObjectiveMaxRange_vmCvar.value) {
                clamped = cg_hudObjectiveMaxRange_vmCvar.value;
            } else if (clamped < cg_hudCompassMaxRange_vmCvar.value) {
                clamped = cg_hudCompassMaxRange_vmCvar.value;
            }
            /* 0x30016ff8..0x30017030: THREE roundings, not one -- the machine
             * spills the numerator (FSTP [ESP+0x54], 0x30017002) and then the
             * quotient (FSTP [ESP+0x54], 0x30017016) to float slots and reloads
             * each, before the final result store (0x30017030). The explicit
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
        /* 0x30017065..0x300170ad is ONE unbroken 80-bit chain with a SINGLE
         * FSTP (0x300170ad): the sizeScale sub-expression is never stored, the
         * slide (FMUL [0x3048c4a8], 0x300170a1) and the ring scale (FMUL
         * [0x3007bf48], 0x300170a7) continue on the same unrounded st0. A
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
            (long double)TANK_COMPASS_RING_SCALE);

        /* The degrees value is ROUNDED to float and spilled (FSTP [ESP+0x14],
         * 0x30016f91) then reloaded (FLD [ESP+0x14], 0x300170b1) for the PI/180
         * conversion -- two roundings, so it needs its own float temp (the
         * friendlies sibling already has one). */
        float bearingRad = (float)(
            (long double)bearingDeg * (long double)TANK_COMPASS_PI /
            (long double)TANK_COMPASS_DEG_PER_RAD);
        float sinBearing;
        float cosBearing;
        coduo_x87_sincosf(bearingRad, &sinBearing, &cosBearing);
        float quadSize = (float)(
            (long double)cg_hudCompassSize_vmCvar.value *
            (long double)TANK_COMPASS_ICON_SIZE);
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
            (long double)TANK_COMPASS_ANGLE2SHORT) & 0xffffu;
        float rotation = (float)(int32_t)packedRotation * TANK_COMPASS_SHORT2ANGLE;
        qhandle_t shader = cg_compassTankShaders[0];
        if (blip->kindOrExpireTime == 1) {
            shader = cg_compassTankShaders[1];
        } else if (blip->kindOrExpireTime == 5) {
            shader = cg_compassTankShaders[2];
        }

        /* 0x30017156/0x3001715e/0x30017173 hardcode green only after the
         * position/rotation work, immediately before CG_R_SETCOLOR. */
        drawColor[0] = 0.0f;
        drawColor[2] = 0.0f;
        drawColor[1] = 1.0f;
        cgame_syscall(CG_R_SETCOLOR, (intptr_t)drawColor);
        CG_DrawRotatedPic(x, y, quadSize, quadSize, rotation, shader);
    }

    cgame_syscall(CG_R_SETCOLOR, 0);
}
