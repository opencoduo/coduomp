// Source: uo_cgame_mp_x86.dll 0x30045550..0x30045c01
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30045550_30045c01.mcode
//
// CG_FakeTrajectoryEffects - synthesize up to eight spread trajectories from a
// weapon tag, trace them through both mark/geometry passes, and emit the first
// qualifying bullet impact or tracer. The former CG_DrawPlayerStance label was
// a size-only match; DObj tag matrices, weapon spread, traces and bullet effects
// prove this routine's identity.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    FAKE_TRAJECTORY_COUNT = 8,
    FAKE_TRAJECTORY_ENTITY_LIMIT = 1024,
    FAKE_TRAJECTORY_TRACE_MODEL = 0x2802031,
    FAKE_TRAJECTORY_REFINE_MODEL = 0x10000,
    FAKE_TRAJECTORY_WORLD_ENTITY = 1022,
    FAKE_TRAJECTORY_NO_ENTITY = 1023,
    FAKE_TRAJECTORY_VALID_NEARFAR = 64
};

#define FAKE_TRAJECTORY_RAND_SCALE (1.0f / 32768.0f)
#define FAKE_TRAJECTORY_PITCH_LIMIT 5.0f
#define FAKE_TRAJECTORY_YAW_LIMIT 2.0f
#define FAKE_TRAJECTORY_SURFACE_NO_IMPACT ((uint32_t)0x4u)
#define FAKE_TRAJECTORY_AIM_DISTANCE 10240.0f

void CG_FakeTrajectoryEffects(int32_t entityNum, int32_t weaponIndex,
                              const char *tagName)
{
    if (cg_fakeTrajectoryTime == (int32_t)cg_time &&
        cg_fakeTrajectoryEntity == entityNum) {
        return;
    }

    cg_fakeTrajectoryTime = cg_time;
    cg_fakeTrajectoryEntity = entityNum;
    if (entityNum < 0 || entityNum >= FAKE_TRAJECTORY_ENTITY_LIMIT) {
        return;
    }

    centity_t *cent = &cg_entities[entityNum];
    intptr_t dobjHandle = cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);
    weaponInfo_t *weapon = bg_weaponInfos[weaponIndex];
    float tracerChance = cg_tracerchance_vmCvar.value;

    if (weapon != NULL &&
        (weapon->ammoType == WEAPON_AMMO_TYPE_LMG ||
         weapon->ammoType == WEAPON_AMMO_TYPE_HMG ||
         weapon->ammoType == WEAPON_AMMO_TYPE_UMG)) {
        tracerChance = cg_tracerchancelmg_vmCvar.value;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    DObjSkelMat tagMatrix;
    if (!CG_DObjGetWorldTagMatrix((void *)dobjHandle, tagName, cent,
                                  &tagMatrix)) {
        return;
    }

    /* The alt-fire vehicle tag is steered toward the camera trace. The machine
     * code limits the correction to five pitch and two yaw degrees before
     * rebuilding the tag basis; the tag translation remains unchanged. */
    if (tagName != NULL &&
        Q_stricmpn(cg_muzzleTagNames[4], tagName, 99999) == 0) {
        axis_t viewAxis;
        trace_t aimTrace;
        vec3_t currentAngles;
        vec3_t desiredAngles;
        vec3_t toAim;
        vec3_t aimEnd;

        AnglesToAxisNegRight(viewAxis, cg_snap->ps.viewAngles);
        aimEnd[0] = (float)(
            (long double)viewAxis[0][0] * FAKE_TRAJECTORY_AIM_DISTANCE +
            (long double)cg_refdef.vieworg[0]);
        aimEnd[1] = (float)(
            (long double)viewAxis[0][1] * FAKE_TRAJECTORY_AIM_DISTANCE +
            (long double)cg_refdef.vieworg[1]);
        aimEnd[2] = (float)(
            (long double)viewAxis[0][2] * FAKE_TRAJECTORY_AIM_DISTANCE +
            (long double)cg_refdef.vieworg[2]);
        CG_Trace(FAKE_TRAJECTORY_TRACE_MODEL, aimEnd,
                           vec3_origin, &aimTrace,
                           cg_refdef.vieworg,
                           vec3_origin, entityNum);
        if (aimTrace.allsolid != 0 || aimTrace.startsolid != 0) {
            Com_PrintMessage(cg_fakeTrajectoryStartedInSolidMessage,
                             (int32_t)cg_time);
        }

        toAim[0] = aimTrace.endpos[0] - tagMatrix.origin[0];
        toAim[1] = aimTrace.endpos[1] - tagMatrix.origin[1];
        toAim[2] = aimTrace.endpos[2] - tagMatrix.origin[2];
        VectorNormalize(toAim);
        /* 0x30045722 call 0x3004a000 = vectoangles(&tagMatrix, currentAngles):
         * only the forward row (tagMatrix[0..2]) is consumed, so currentAngles[2]
         * (roll) is always 0. A prior pass replaced this with a tagAxis[3][3] copy
         * + AxisToAngles (0x3004c2a0), injecting the tag's real roll into
         * aimedAngles[2] -> AngleVectors and rotating the rebuilt muzzle basis on
         * every alt-fire shot. */
        vectoangles(tagMatrix.axis[0], currentAngles);
        vectoangles(toAim, desiredAngles);

        float pitch = AngleSubtract(currentAngles[0], desiredAngles[0]);
        float yaw = AngleSubtract(currentAngles[1], desiredAngles[1]);
        if (pitch < -FAKE_TRAJECTORY_PITCH_LIMIT) {
            pitch = -FAKE_TRAJECTORY_PITCH_LIMIT;
        } else if (pitch > FAKE_TRAJECTORY_PITCH_LIMIT) {
            pitch = FAKE_TRAJECTORY_PITCH_LIMIT;
        }
        if (yaw < -FAKE_TRAJECTORY_YAW_LIMIT) {
            yaw = -FAKE_TRAJECTORY_YAW_LIMIT;
        } else if (yaw > FAKE_TRAJECTORY_YAW_LIMIT) {
            yaw = FAKE_TRAJECTORY_YAW_LIMIT;
        }

        vec3_t aimedAngles = {
            AngleSubtract(currentAngles[0], pitch),
            AngleSubtract(currentAngles[1], yaw),
            /* 0x300457e7..ef: the DLL calls AngleSubtract(currentAngles[2], 0.0f)
             * here too (a third CALL 0x3004bd70 with PUSH 0). Inert -- roll is
             * always 0 so AngleSubtract(0,0)=0 -- but kept to match the bytes. */
            AngleSubtract(currentAngles[2], 0.0f)
        };
        axis_t aimedAxis;
        AngleVectors(aimedAngles, aimedAxis[0], aimedAxis[1], aimedAxis[2]);
        for (int32_t row = 0; row < 3; row++) {
            tagMatrix.axis[row][0] = aimedAxis[row][0];
            tagMatrix.axis[row][1] = aimedAxis[row][1];
            tagMatrix.axis[row][2] = aimedAxis[row][2];
        }
    }

    matrix43_t muzzleMatrix = {
        .axis = {
            {tagMatrix.axis[0][0], tagMatrix.axis[0][1], tagMatrix.axis[0][2]},
            {tagMatrix.axis[1][0], tagMatrix.axis[1][1], tagMatrix.axis[1][2]},
            {tagMatrix.axis[2][0], tagMatrix.axis[2][1], tagMatrix.axis[2][2]}
        },
        .origin = {tagMatrix.origin[0], tagMatrix.origin[1],
                   tagMatrix.origin[2]}
    };
    vec3_t muzzleOrigin = {
        muzzleMatrix.origin[0], muzzleMatrix.origin[1],
        muzzleMatrix.origin[2]
    };

    for (int32_t i = 0; i < FAKE_TRAJECTORY_COUNT; i++) {
        float spread = (float)coduo_crt_rand() * FAKE_TRAJECTORY_RAND_SCALE *
                       weapon->maxSpread + weapon->hipSpreadStandMin;
        vec3_t end;
        trace_t trace;
        trace_t refined;

        BG_Bullet_Endpos(spread, end, muzzleMatrix.axis[0]);
        CG_Trace(FAKE_TRAJECTORY_TRACE_MODEL, end,
                           vec3_origin, &trace,
                           muzzleOrigin,
                           vec3_origin, entityNum);

        if (tagName != NULL &&
            Q_stricmpn(cg_muzzleTagNames[4], tagName, 99999) == 0) {
            CG_Trace(FAKE_TRAJECTORY_REFINE_MODEL, end,
                               vec3_origin, &refined,
                               muzzleOrigin,
                               vec3_origin, entityNum);
        } else {
            CG_Trace(FAKE_TRAJECTORY_REFINE_MODEL, end,
                               vec3_origin, &refined,
                               muzzleOrigin,
                               vec3_origin,
                               FAKE_TRAJECTORY_WORLD_ENTITY);
        }

        if (refined.fraction < trace.fraction &&
            refined.entityNum != FAKE_TRAJECTORY_NO_ENTITY) {
            int32_t hitEntity = refined.entityNum;
            centity_t *hitCent = &cg_entities[hitEntity];
            if (hitCent->currentValid != 0 && hitCent->currentState.eType == ET_VEHICLE) {
                trace = refined;
            }
        }

        if (trace.entityNum < FAKE_TRAJECTORY_VALID_NEARFAR) {
            continue;
        }

        if (trace.fraction < 1.0f &&
            (trace.surfaceFlags & FAKE_TRAJECTORY_SURFACE_NO_IMPACT) == 0) {
            vec3_t effectDir2 = {
                trace.endpos[0] - muzzleOrigin[0],
                trace.endpos[1] - muzzleOrigin[1],
                trace.endpos[2] - muzzleOrigin[2]
            };
            VectorNormalize(effectDir2);
            /* 0x30045ada..0x30045b6e: reflect effectDir2 about the surface plane
             * before the hit event -- the DLL passes the REFLECTED direction as
             * CG_BulletHitEvent's effectDir2 (arg via ESI -> the effectDir2 slot,
             * modified in place), not the raw one. effectDir2 = effectDir2
             * - 2*(trace.normal . effectDir2)*trace.normal. The scalar -2*(N.D) is
             * FSTP'd to a float slot (0x30045b23) and reloaded per component, so
             * reflectScale is a float temp (Class 1). effectDir1 stays trace.normal. */
            float reflectScale = (float)(
                ((((long double)trace.normal[2] * (long double)effectDir2[2]) +
                  ((long double)trace.normal[1] * (long double)effectDir2[1])) +
                 ((long double)trace.normal[0] * (long double)effectDir2[0])) *
                (long double)-2.0f);
            effectDir2[0] = (float)(
                (long double)trace.normal[0] * (long double)reflectScale +
                (long double)effectDir2[0]);
            effectDir2[1] = (float)(
                (long double)trace.normal[1] * (long double)reflectScale +
                (long double)effectDir2[1]);
            effectDir2[2] = (float)(
                (long double)trace.normal[2] * (long double)reflectScale +
                (long double)effectDir2[2]);
            int32_t surfaceType = (int32_t)((trace.surfaceFlags >> 20) & 31u);
            int32_t mountPos = 1 + (tagName == cg_muzzleTagNames[5]);
            CG_BulletHitEvent(entityNum, trace.endpos, trace.normal,
                              effectDir2, weaponIndex, surfaceType,
                              coduo_int32_from_bits((uint32_t)entityNum + 1u),
                              mountPos);
            return;
        }

        if ((float)coduo_crt_rand() * FAKE_TRAJECTORY_RAND_SCALE < tracerChance) {
            CG_SpawnTracerLine(trace.endpos, muzzleOrigin, weaponIndex);
        }
        return;
    }
}
