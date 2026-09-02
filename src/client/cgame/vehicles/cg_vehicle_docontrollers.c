// Source: uo_cgame_mp_x86.dll 0x30020540..0x3002138e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30020540_3002138e.mcode
//
// CG_Vehicle_DoControllers — per-frame DObj skeleton / bone-tag setup and
// ground-contact tread-dust emission for a vehicle render entity.
//
// The ghidra size-match name "BG_AnimParseAnimScript" is REJECTED: there is no
// text/script parsing anywhere in the body. The machine code issues DObj
// skeleton/bone traps through the cgame syscall pointer (cgame_syscall):
//   CG_DOBJ_GET_HANDLE (0xa5), CG_DOBJ_GET_BONE_INDEX (0xb2),
//   CG_DOBJ_SET_ROT_TRANS_INDEX (0xa2), CG_DOBJ_GET_ROT_TRANS_ARRAY (0xa1),
//   CG_DOBJ_NUM_BONES (0xb1), CG_XMODEL_GET_BASE_POSE (0xb0),
//   CG_PLAY_EFFECT_ORIENTED (0xe8), and the input-command pair
//   CG_GET_CURRENT_CMD_NUMBER (0x53) / CG_GET_USER_CMD (0x54).
// It interpolates the vehicle's turret / barrel / secondary-gun / wheel angles
// between the previous and new snapshot states by cg_frameInterpolation, writes
// them as DObj local tags, and — for each wheel bone — traces the ground and plays
// the matching tread/wheel dust effect keyed by the traced surface material.
//
// The Mac cgame symbol CG_Vehicle_DoControllers shares eight distinctive direct
// callees with this body. Combined with the DObj-tag trap vocabulary, the tag
// names "tag_turret"/"tag_barrel"/"tag_steeringwheel"/wheel tags, and the resolved
// cgs_media_vehicleTreadEffects table, this resolves the source name.
//
// The first-person follow transform and the six-wheel contact/effect loop are
// reconstructed from every x87 data dependency in 0x300207f0..0x30020a46 and
// 0x30020d4f..0x30021320. In particular, the two trace segments, local-tag update,
// persistent >32 and <=200-unit wheel-motion gate, surface-class switch, and three
// independent rand-scaled dust-vector components follow the instruction order.

#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"

#include <string.h>

/* Constants dumped via objdump -s -j .rdata and decoded little-endian:
 *   0x3007be60 = 127.0f, 0x3007becc = 30.0f, 0x3007bd94 = 0.001f,
 *   0x3007be88 = 1000.0f, 0x3007be38 = 0.75f, 0x3007be40 = 4.0f. */
#define VEH_ANGLE_BIAS 127.0f              /* 0x3007be60 */
#define VEH_ANGLE_SCALE 30.0f              /* 0x3007becc */
#define VEH_MILLI_SCALE 0.0010000000474974513f /* 0x3007bd94 */
#define VEH_SECONDS_TO_MILLISECONDS 1000.0f  /* 0x3007be88 */
#define VEH_STEERING_WHEEL_SCALE 0.75f   /* 0x3007be38 */

enum {
    VEHICLE_TYPE_JEEP = 1,
    VEHICLE_WHEEL_COUNT = 6,
    VEHICLE_FRONT_WHEEL_COUNT = 2,
    VEHICLE_WHEEL_TRACE_HANDLE = 0x211,
    VEHICLE_FOLLOW_MODE_DRIVER = 1,
    VEHICLE_FOLLOW_MODE_GUNNER = 2,
    VEH_DOBJ_PLACEMENT_DEFAULT = 0,
    VEH_TRACE_FLAGS_NONE = 0,
    VEH_TRACE_ARG_NONE = 0,
};

#define VEH_WHEEL_TRACE_HEIGHT 40.0f /* 0x3007c284 */
#define VEH_WHEEL_TRACE_EXTRA 16.0f /* 0x3007bf00 */
#define VEH_WHEEL_EFFECT_MIN_MOVE 32.0f /* 0x3007bdd0 */
#define VEH_WHEEL_EFFECT_MAX_MOVE 200.0f /* 0x3007bf44 */
#define VEH_WHEEL_EFFECT_Z_OFFSET 10.0f /* 0x3007bda4 */
#define VEH_WHEEL_EFFECT_Z_JITTER 8.0f /* 0x3007be08 */
#define VEH_WHEEL_EFFECT_RAND_BASE 0.25f /* 0x3007be58 */
#define VEH_WHEEL_EFFECT_RAND_RANGE 0.5f  /* 0x3007bce8 */
#define VEH_RAND_DENOMINATOR 32768.0f  /* 0x3007bd10 */
#define VEH_STEERING_WHEEL_PITCH 210.0f /* immediate 0x43520000 */

/* 0x3006be3c is the MSVC CRT `_ftol2` truncating float->int conversion, declared
 * canonically as Q_rint in the shared header (the former crt_ftol_round alias was
 * merged into it). This call site passes its argument in ST(0) and reads the
 * result from EAX; the value is truncated toward zero after converting the
 * interpolated seconds back to milliseconds. */

void CG_Vehicle_DoControllers(centity_t *veh, uint32_t *partBits)
{
    struct DObj_s *self;      /* [EBP+0x44] = ESI: DObj from trap 0xa5 */
    /* The four angle triples the tag setters pass, at their exact frame offsets.
     * Slots not written by a proven statement are zeroed at entry (0x300205a6.. as
     * MOV [EBP+off],EAX with EAX=0), matching the machine code. */
    float bodyAngles[3] = {0.0f, 0.0f, 0.0f}; /* [EBP+0x14/0x18/0x1c] */
    float auxAngles[3] = {0.0f, 0.0f, 0.0f}; /* [EBP+0x20/0x24/0x28] */
    float wheelAngles[3] = {0.0f, 0.0f, 0.0f}; /* [EBP+0x38/0x3c/0x40] */
    float followAngles[3] = {0.0f, 0.0f, 0.0f}; /* [EBP+0xffffff44/48/4c]: tank 2nd-gun angles */
    float secondaryAngles[3] = {0.0f, 0.0f, 0.0f}; /* [EBP+0x54/0x58/0x5c] */
    float wheelSpinRate;     /* [EBP-0x78]: blended wheel-spin rate */
    float interpolatedTimeSeconds; /* [EBP+0xffffff50]: time lerp after *0.001 */
    const vec3_t zeroOrigin = {0.0f, 0.0f, 0.0f}; /* origin arg for the tag setters */

    self = (struct DObj_s *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, veh->currentState.number);

    /* --- Interpolate the vehicle-part angles (LerpAngle calls) ------------- *
     * cg_frameInterpolation (0x304831a8) is the [0,1) snapshot lerp weight.
     * The packed-int fields are rounded to float and then unpacked; the float
     * fields are passed straight through. Result-slot mapping is proven from the
     * FSTP targets across 0x30020574..0x3002069a. */
    {
        /* Each FILD is rounded through a DWORD stack slot before the
         * FSUB/FMUL/FDIV chain (0x30020574..0x30020653). */
        float nextPitchPacked = (float)veh->nextState.vehicleBodyPitchPacked;
        float initialLerpFraction = cg_frameInterpolation;
        float currentPitchPacked = (float)veh->currentState.vehicleBodyPitchPacked;
        float nextPitch = (float)((((long double)nextPitchPacked - (long double)VEH_ANGLE_BIAS) * (long double)VEH_ANGLE_SCALE) /
                                  (long double)VEH_ANGLE_BIAS);
        float currentPitch = (float)((((long double)currentPitchPacked - (long double)VEH_ANGLE_BIAS) * (long double)VEH_ANGLE_SCALE) /
                                     (long double)VEH_ANGLE_BIAS);
        bodyAngles[0] = LerpAngle(currentPitch, nextPitch, initialLerpFraction);               /* [EBP+0x14] */

        float nextRollPacked = (float)veh->nextState.vehicleBodyRollPacked;
        float currentRollPacked = (float)veh->currentState.vehicleBodyRollPacked;
        float nextRoll = (float)((((long double)nextRollPacked - (long double)VEH_ANGLE_BIAS) * (long double)VEH_ANGLE_SCALE) /
                                 (long double)VEH_ANGLE_BIAS);
        float currentRoll = (float)((((long double)currentRollPacked - (long double)VEH_ANGLE_BIAS) * (long double)VEH_ANGLE_SCALE) /
                                    (long double)VEH_ANGLE_BIAS);
        bodyAngles[2] = LerpAngle(currentRoll, nextRoll, initialLerpFraction);               /* [EBP+0x1c] */

        secondaryAngles[0] =
            LerpAngle(veh->currentState.vehicleBarrelPitch, veh->nextState.vehicleBarrelPitch, initialLerpFraction); /* [EBP+0x54] */
        auxAngles[1] = LerpAngle(veh->currentState.vehicleTurretYaw, veh->nextState.vehicleTurretYaw, initialLerpFraction); /* [EBP+0x24] */
        wheelSpinRate =
            LerpAngle(veh->currentState.vehicleWheelAngle, veh->nextState.vehicleWheelAngle, initialLerpFraction); /* [EBP-0x78] */
    }

    /* Interpolate currentState.time (+0x54) to nextState.time (+0x148) in seconds:
     * each integer millisecond value is multiplied by 0.001f before the ordinary
     * `(next-current)*frac + current` lerp. */
    {
        float nextTimeInteger = (float)veh->nextState.time;
        float currentTimeInteger = (float)veh->currentState.time;
        float nextTime = (float)((long double)nextTimeInteger * (long double)VEH_MILLI_SCALE);
        float currentTime = (float)((long double)currentTimeInteger * (long double)VEH_MILLI_SCALE);
        interpolatedTimeSeconds =
            (float)(((long double)nextTime - (long double)currentTime) * (long double)cg_frameInterpolation + (long double)currentTime);
    }

    /* --- tag_body: set the vehicle body bone from bodyAngles --------------- */
    {
        int bone = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleBodyTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, bodyAngles, zeroOrigin);
        }
    }
    /* --- tag_turret using auxAngles --------------------------------------- */
    {
        int bone =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleTurretTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, auxAngles, zeroOrigin);
        }
    }
    /* --- tag_barrel using secondaryAngles --------------------------------- */
    {
        int bone =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleBarrelTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, secondaryAngles, zeroOrigin);
        }
    }

    /* --- First-person follow blend (0x300207c0..0x30020a46) --------------- *
     * Gate: (cg_snap->psEFlags & EF_IN_VEHICLE) &&
     *       cg_snap->ps.vehiclePosition == 2 && cg_snap->ps.viewLockedEntityNum == entityNum.
     * When following THIS vehicle in first person, the body/secondary angles are
     * recomputed from veh->viewAngles + the locally predicted view instead of the
     * raw snapshot; otherwise the two remaining body angles are plain LerpAngles. */
    if ((cg_snap->ps.entityStateFlags & EF_IN_VEHICLE) != 0 && cg_snap->ps.vehiclePosition == VEHICLE_FOLLOW_MODE_GUNNER &&
        cg_snap->ps.viewLockedEntityNum == veh->currentState.number) {
        playerState_t localPs;
        vec3_t vehicleViewAngles;
        memcpy(vehicleViewAngles, veh->lerpAngles, sizeof(vehicleViewAngles));
        vehicleViewAngles[0] = (float)((long double)vehicleViewAngles[0] + (long double)bodyAngles[0]);
        memcpy(&localPs, &cg_predictedPlayerState, sizeof(localPs)); /* REP MOVSD, 0x1141 dwords */
        vehicleViewAngles[1] = (float)((long double)vehicleViewAngles[1] + 0.0L);
        vehicleViewAngles[2] = (float)((long double)vehicleViewAngles[2] + (long double)bodyAngles[2]);
        axis_t localViewAxis;
        axis_t vehicleAxis;
        axis_t vehicleAxisTranspose;
        axis_t relativeAxis;

        /* Live input is folded into the local player-state copy only outside demo
         * playback and when the snapshot's 0x40000 follow bit is clear. */
        if ((cg_snap->ps.playerStateFlags & PSF_FOLLOWING) == 0 && cg_demoPlayback == qfalse) {
            usercmd_t cmd;
            int32_t cmdNumber = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_CURRENT_CMD_NUMBER));
            cgame_syscall(CG_GET_USER_CMD, cmdNumber, (intptr_t)&cmd);
            PM_UpdateViewAngles(&localPs, &cmd, CG_TraceCapsule);
        }

        AnglesToAxisNegRight(localViewAxis, localPs.viewAngles);
        AnglesToAxisNegRight(vehicleAxis, vehicleViewAngles);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                vehicleAxisTranspose[row][column] = vehicleAxis[column][row];
            }
        }
        MatrixMultiply(localViewAxis, vehicleAxisTranspose, relativeAxis);
        AxisToAngles(relativeAxis, secondaryAngles);

        /* Normalize the relative pitch, clamp it to the vehicle's asymmetric
         * +/- envelope, then turn the clamp delta into a view correction. */
        {
            float normalizedPitch = AngleNormalize180(secondaryAngles[0]);
            float clampedPitch;

            if (normalizedPitch < -veh->currentState.vehicleFollowPitchDownLimit) {
                clampedPitch = -veh->currentState.vehicleFollowPitchDownLimit;
            } else if (normalizedPitch > veh->currentState.vehicleFollowPitchUpLimit) {
                clampedPitch = veh->currentState.vehicleFollowPitchUpLimit;
            } else {
                clampedPitch = normalizedPitch;
            }

            bodyAngles[0] = (float)((long double)clampedPitch - (long double)secondaryAngles[0]);
            bodyAngles[1] = (float)((long double)secondaryAngles[1] - (long double)secondaryAngles[1]);
            bodyAngles[2] = 0.0f;
        }

        localPs.viewAngles[0] = (float)((long double)localPs.viewAngles[0] + (long double)bodyAngles[0]);
        localPs.viewAngles[1] = (float)((long double)localPs.viewAngles[1] + (long double)bodyAngles[1]);
        localPs.viewAngles[2] = (float)((long double)localPs.viewAngles[2] + (long double)bodyAngles[2]);
        AnglesToAxisNegRight(localViewAxis, localPs.viewAngles);
        MatrixMultiply(localViewAxis, vehicleAxisTranspose, relativeAxis);
        AxisToAngles(relativeAxis, secondaryAngles);

        wheelAngles[1] = (float)((long double)secondaryAngles[1] - (long double)auxAngles[1]);
        followAngles[0] = secondaryAngles[0];
    } else {
        /* Non-follow path (0x30020a48): the two remaining body angles are plain
         * LerpAngles of the (+0x60,+0x154) and (+0x70,+0x164) float pairs. */
        float nonFollowFraction = cg_frameInterpolation;
        wheelAngles[1] = LerpAngle(veh->currentState.vehicleSecondaryBaseYaw, veh->nextState.vehicleSecondaryBaseYaw,
                                   nonFollowFraction); /* [EBP+0x3c] */
        followAngles[0] = LerpAngle(veh->currentState.vehicleSecondaryGunPitch, veh->nextState.vehicleSecondaryGunPitch,
                                    nonFollowFraction); /* [EBP+0xffffff44] */
    }

    /* --- Secondary-crew bone tags (vehicleType selects the layout) --------- *
     * Both layouts set their first tag's local orientation from wheelAngles
     * ([EBP+0x38]) with a zero origin (&0x30071f58 == {0,0,0}), then set the
     * secondary gun from followAngles ([EBP+0xffffff44]): the jeep arm's
     * `JMP 0x30020b61` (0x30020af4) lands on the SAME shared
     * SET_ROT_TRANS_INDEX + CG_DObjSetLocalTagInternal tail as the tank arm
     * (LEA EBX,[EBP-0xbc] at 0x30020b78). */
    if (veh->currentState.vehicleType == VEHICLE_TYPE_JEEP) {
        int bone = coduo_int32_from_bits(
            (uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleSecondaryBaseTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, wheelAngles, zeroOrigin);
        }
        bone =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleSecondaryGunTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, followAngles, zeroOrigin);
        }
    } else {
        int bone =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)bg_secondaryPlayerTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, wheelAngles, zeroOrigin);
        }
        bone =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleSecondaryGunTagName));
        if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
            CG_DObjSetLocalTagInternal(self, bone, followAngles, zeroOrigin);
        }
    }

    /* --- Steering-wheel local tag with a ring-buffered seed (jeep only) ---- *
     * The steering-wheel local tag (jeep layout, vehicleType==1) is seeded by
     * tv(2, 0, 0). The compiler inlined the exact q_shared eight-slot ring at
     * 0x30020bf8..0x30020c29 rather than calling the retained tv body at
     * 0x3004e930. The wheel-spin component is scaled by 0.75f (0x3007be38)
     * normally, or by 4.0f
     * (0x3007be40) when following in first person and the snapshot's +0xec is
     * exactly 0.0f (0x30020bbb..0x30020be2 FLD/FUCOMPP gate). */
    if (veh->currentState.vehicleType == VEHICLE_TYPE_JEEP) {
        float *steeringOrigin;
        float spinScale = VEH_STEERING_WHEEL_SCALE;

        auxAngles[0] = VEH_STEERING_WHEEL_PITCH;
        auxAngles[1] = 0.0f;

        if ((cg_snap->ps.entityStateFlags & EF_IN_VEHICLE) != 0 && cg_snap->ps.vehiclePosition == VEHICLE_FOLLOW_MODE_DRIVER &&
            cg_snap->ps.viewLockedEntityNum == veh->currentState.number && cg_snap->ps.adsFraction == 0.0f) {
            spinScale = 4.0f;
        }
        auxAngles[2] = (float)((long double)wheelSpinRate * (long double)spinScale); /* [EBP+0x28] */

        steeringOrigin = tv(2.0f, 0.0f, 0.0f);

        {
            int bone = coduo_int32_from_bits(
                (uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)cg_vehicleSteeringWheelTagName));
            if (bone >= 0 && cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
                CG_DObjSetLocalTagInternal(self, bone, auxAngles, steeringOrigin);
            }
        }
    }

    /* --- Wheel loop: place each wheel bone and emit tread dust ------------- */
    {
        float wheelTracePacked = (float)veh->currentState.vehicleWheelTracePacked;
        float wheelTraceDistance = (float)((long double)wheelTracePacked * (long double)VEH_MILLI_SCALE);
        /* partCount = CG_DOBJ_NUM_BONES(self); alloca ((partCount<<6)+3)&~3. */
        int32_t partCount = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_NUM_BONES, (intptr_t)self));
        size_t matBytes = (size_t)((((uint32_t)partCount << 6) + 3u) & ~3u);
        DObjSkelMat *boneMatrices = __builtin_alloca(matBytes);
        matrix43_t placement;
        vec3_t right;

        /* 0x30020ca3..0x30020d0d lays out a 4x3 transform matrix as
         * {forward, -right, up, origin}. AngleVectors writes forward->ESI
         * (rows[0], [ebp-0x28]), right->EDI (the scratch `right`, [ebp+0x20]) and
         * up->EBX (rows[2], [ebp-0x10]); the three `0.0f - right[i]` stores
         * materialize row 1 = -right. A prior pass swapped the forward/up
         * destinations (forward->axis[2], up->axis[0]); the DLL is the reverse. */
        AngleVectors(veh->lerpAngles, placement.axis[0], right, placement.axis[2]);
        placement.axis[1][0] = 0.0f - right[0];
        placement.axis[1][1] = 0.0f - right[1];
        placement.axis[1][2] = 0.0f - right[2];
        memcpy(&placement.origin, &veh->lerpOrigin, sizeof(placement.origin));

        cgame_syscall(CG_XMODEL_GET_BASE_POSE, (intptr_t)self, VEH_DOBJ_PLACEMENT_DEFAULT, (intptr_t)boneMatrices);

        for (int w = 0; w < VEHICLE_WHEEL_COUNT; ++w) {
            const char *tagName = cg_vehicleWheelTags[w];
            int bone = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));
            if (bone < 0) {
                continue;
            }

            DObjSkelMat *boneMatrix = &boneMatrices[bone];
            vec3_t wheelWorld;
            vec3_t traceStart;
            vec3_t traceEnd;
            vec3_t contactOrigin;
            trace_t trace;

            /* Transform the skeleton's translation triple. Each component is
             * `(row2*origin2 + row0*origin0) + row1*origin1`, followed by the
             * placement origin, with one float store at the end. */
            for (int component = 0; component < 3; ++component) {
                long double wheelComponent = ((long double)placement.axis[2][component] * (long double)boneMatrix->origin[2] +
                                              (long double)placement.axis[0][component] * (long double)boneMatrix->origin[0]) +
                                             (long double)placement.axis[1][component] * (long double)boneMatrix->origin[1];
                wheelWorld[component] = (float)(wheelComponent + (long double)placement.origin[component]);
                traceStart[component] = (float)((long double)placement.axis[2][component] * (long double)VEH_WHEEL_TRACE_HEIGHT +
                                                (long double)wheelWorld[component]);
            }
            /* The target negates wheelTraceDistance once, stores that negated
             * value as float, and reloads it for all three products. */
            float negWheelTraceDistance = -wheelTraceDistance;
            for (int component = 0; component < 3; ++component) {
                traceEnd[component] = (float)((long double)negWheelTraceDistance * (long double)placement.axis[2][component] +
                                              (long double)wheelWorld[component]);
            }

            if (cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, bone) != 0) {
                vec3_t localOffset;
                float tracedDistance;
                float maxDistance;

                CG_Trace(VEHICLE_WHEEL_TRACE_HANDLE, traceEnd, NULL, &trace, traceStart, NULL, veh->currentState.number);

                tracedDistance =
                    (float)(((long double)wheelTraceDistance + (long double)VEH_WHEEL_TRACE_HEIGHT) * (long double)trace.fraction);
                maxDistance = (float)((long double)VEH_WHEEL_TRACE_HEIGHT - (long double)wheelTraceDistance);
                /* 0x30020e66 FLD tracedDistance; FCOMP maxDistance; TEST AH,0x5;
                 * JP 0x30020e7f skips the store, so `tracedDistance = maxDistance`
                 * (MOV [ebp+0x7c],[ebp+0x80] at 0x30020e76) runs only on the
                 * fall-through -- i.e. when tracedDistance < maxDistance. A prior
                 * pass negated the test (`!(tracedDistance < maxDistance)`). */
                if (tracedDistance < maxDistance) {
                    tracedDistance = maxDistance;
                }

                float negTracedDistance = -tracedDistance;
                for (int component = 0; component < 3; ++component) {
                    contactOrigin[component] = (float)((long double)negTracedDistance * (long double)placement.axis[2][component] +
                                                       (long double)traceStart[component]);
                }

                MatrixTransposeTransformVector43(contactOrigin, &placement, localOffset);
                localOffset[0] -= boneMatrix->origin[0];
                localOffset[1] -= boneMatrix->origin[1];
                localOffset[2] -= boneMatrix->origin[2];

                /* 0x30020ed4: FUCOMPP vs 0.0 with a TEST AH,0x44 / JNP equality
                 * gate — any NONZERO spin (negative included; NaN too) takes the
                 * spinning local-tag path; only exactly 0.0 falls through. */
                if (wheelSpinRate != 0.0f && w < VEHICLE_FRONT_WHEEL_COUNT) {
                    vec3_t wheelAnglesLocal = {0.0f, wheelSpinRate, 0.0f};
                    CG_DObjSetLocalTagInternal(self, bone, wheelAnglesLocal, localOffset);
                } else {
                    DObjAnimMat *rotTrans = (DObjAnimMat *)(intptr_t)cgame_syscall(CG_DOBJ_GET_ROT_TRANS_ARRAY, (intptr_t)self);
                    DObjAnimMat *mat = &rotTrans[bone];

                    mat->quat[0] = 0.0f;
                    mat->quat[1] = 0.0f;
                    mat->quat[2] = 0.0f;
                    mat->quat[3] = 1.0f;
                    mat->accumulatedWeight = 0.0f;
                    memcpy(&mat->translation[0], &localOffset[0], sizeof(mat->translation[0]));
                    memcpy(&mat->translation[1], &localOffset[1], sizeof(mat->translation[1]));
                    memcpy(&mat->translation[2], &localOffset[2], sizeof(mat->translation[2]));
                }
            } else {
                memcpy(contactOrigin, wheelWorld, sizeof(contactOrigin));
            }

            /* The second trace end is anchored on wheelWorld ([EBP+0x6c..0x74],
             * FADD at 0x30020f8c/0x30020fa2/0x30020fae), NOT the clamped
             * contactOrigin: traceEnd = wheelWorld - rows[2]*(dist+16). */
            /* The DLL forms -(wheelTraceDistance + 16.0f) ONCE, FSTP's it to a float
             * slot and reloads it for all three components (0x30020f5e FADD 16.0f;
             * 0x30020f76 FCHS; 0x30020f78 FSTP; 0x30020f7d FLD): the SUM is rounded
             * to float before the multiply. Keeping it 80-bit inline would drop that
             * rounding (Class 1). (Contrast the first traceEnd above, whose negated
             * operand is the already-float wheelTraceDistance, so its FCHS is exact.) */
            float negTraceReach = (float)(-((long double)wheelTraceDistance + (long double)VEH_WHEEL_TRACE_EXTRA));
            for (int component = 0; component < 3; ++component) {
                traceEnd[component] =
                    (float)((long double)placement.axis[2][component] * (long double)negTraceReach + (long double)wheelWorld[component]);
            }
            CG_Trace(VEHICLE_WHEEL_TRACE_HANDLE, traceEnd, NULL, &trace, traceStart, NULL, veh->currentState.number);

            if (cg_vehicletrails_vmCvar.integer == 0 || !(trace.fraction < 1.0f)) {
                continue;
            }

            {
                vec3_t *lastOrigin = &veh->vehicleWheelLastOrigin[w];
                vec3_t delta;
                float distance;

                if ((*lastOrigin)[0] == 0.0f && (*lastOrigin)[1] == 0.0f && (*lastOrigin)[2] == 0.0f) {
                    memcpy(*lastOrigin, contactOrigin, sizeof(*lastOrigin));
                    continue;
                }

                delta[0] = (*lastOrigin)[0] - contactOrigin[0];
                delta[1] = (*lastOrigin)[1] - contactOrigin[1];
                delta[2] = (*lastOrigin)[2] - contactOrigin[2];
                /* The DLL passes the raw 80-bit sum-of-squares straight into _CIsqrt
                 * (0x300210c2 FADDP; 0x300210c4 CALL 0x3006bee0) with no float store
                 * of the argument. Its stack order is specifically
                 * `(x*x + z*z) + y*y`: FLD/FMUL delta[0], FLD/FMUL delta[2],
                 * FADDP, FLD/FMUL delta[1], FADDP. */
                {
                    long double x2 = (long double)delta[0] * (long double)delta[0];
                    long double z2 = (long double)delta[2] * (long double)delta[2];
                    long double y2 = (long double)delta[1] * (long double)delta[1];
                    distance = (float)coduo_x87_sqrtl((x2 + z2) + y2);
                }

                if (distance > VEH_WHEEL_EFFECT_MAX_MOVE) {
                    memcpy(*lastOrigin, contactOrigin, sizeof(*lastOrigin));
                    continue;
                }
                /* This second _CIsqrt deliberately has a different instruction
                 * order: `(x*x + y*y) + z*z` at 0x300210fb..0x30021117. */
                {
                    long double x2 = (long double)delta[0] * (long double)delta[0];
                    long double y2 = (long double)delta[1] * (long double)delta[1];
                    long double z2 = (long double)delta[2] * (long double)delta[2];
                    distance = (float)coduo_x87_sqrtl((x2 + y2) + z2);
                }
                if (!(distance > VEH_WHEEL_EFFECT_MIN_MOVE)) {
                    continue;
                }

                delta[0] = contactOrigin[0] - (*lastOrigin)[0];
                delta[1] = contactOrigin[1] - (*lastOrigin)[1];
                delta[2] = contactOrigin[2] - (*lastOrigin)[2];
                (void)VectorNormalize(delta);

                memcpy(*lastOrigin, contactOrigin, sizeof(*lastOrigin));

                {
                    int32_t material = VEH_TREAD_EFFECT_NONE;
                    uint32_t selector = (trace.surfaceFlags >> SURFACE_TYPE_SHIFT) & SURFACE_TYPE_MASK;
                    vec3_t effectOrigin;
                    memcpy(effectOrigin, contactOrigin, sizeof(effectOrigin));

                    /* Source-level switch reconstructed from the compiler's
                     * two-level tables at 0x30021390 and 0x300213ac. The byte
                     * table is a case selector, not an original material array. */
                    switch (selector) {
                    case 6:
                        material = VEH_TREAD_EFFECT_TANK_DIRT;
                        break;
                    case 10:
                        material = VEH_TREAD_EFFECT_TANK_GRASS;
                        break;
                    case 11:
                    case 17:
                        material = VEH_TREAD_EFFECT_TANK_ROCK;
                        break;
                    case 12:
                        material = VEH_TREAD_EFFECT_TANK_SNOW;
                        break;
                    case 18:
                        material = VEH_TREAD_EFFECT_TANK_SAND;
                        break;
                    case 19:
                        material = VEH_TREAD_EFFECT_TANK_SNOW_ALT;
                        break;
                    default:
                        break;
                    }

                    if (material != VEH_TREAD_EFFECT_NONE) {
                        /* The `ADD EBX,6` at 0x300211c9 is reached only from the
                         * six value cases (via 0x300211ba); the default case
                         * (material NONE) jumps straight to 0x300211cc past it. */
                        if (veh->currentState.vehicleType == VEHICLE_TYPE_JEEP) {
                            material += VEH_TREAD_EFFECT_JEEP_OFFSET;
                        }
                    }

                    /* 0x300211db..0x300211ef forms the dot product in z,y,x
                     * order without an intervening float store. */
                    long double travelDot = ((long double)placement.axis[0][2] * (long double)delta[2] +
                                             (long double)placement.axis[0][1] * (long double)delta[1]) +
                                            (long double)placement.axis[0][0] * (long double)delta[0];
                    if (veh->currentState.vehicleType == VEHICLE_TYPE_JEEP && (travelDot < 0.0L || w < VEHICLE_FRONT_WHEEL_COUNT)) {
                        effectOrigin[2] = (float)((long double)effectOrigin[2] - (long double)VEH_WHEEL_EFFECT_Z_OFFSET);
                    } else {
                        float randomSample = (float)coduo_crt_rand();
                        effectOrigin[2] = (float)((((long double)randomSample / (long double)VEH_RAND_DENOMINATOR) *
                                                   (long double)VEH_WHEEL_EFFECT_Z_JITTER) +
                                                  (long double)effectOrigin[2]);
                    }

                    if (material != VEH_TREAD_EFFECT_NONE) {
                        if (cgs_media_vehicleTreadEffects[material] != 0) {
                            float randomX = (float)coduo_crt_rand();
                            delta[0] = (float)(-((((long double)randomX / (long double)VEH_RAND_DENOMINATOR) *
                                                  (long double)VEH_WHEEL_EFFECT_RAND_RANGE) +
                                                 (long double)VEH_WHEEL_EFFECT_RAND_BASE) *
                                               (long double)delta[0]);
                            float randomY = (float)coduo_crt_rand();
                            delta[1] = (float)(-((((long double)randomY / (long double)VEH_RAND_DENOMINATOR) *
                                                  (long double)VEH_WHEEL_EFFECT_RAND_RANGE) +
                                                 (long double)VEH_WHEEL_EFFECT_RAND_BASE) *
                                               (long double)delta[1]);
                            float randomZ = (float)coduo_crt_rand();
                            delta[2] = (float)(-((((long double)randomZ / (long double)VEH_RAND_DENOMINATOR) *
                                                  (long double)VEH_WHEEL_EFFECT_RAND_RANGE) +
                                                 (long double)VEH_WHEEL_EFFECT_RAND_BASE) *
                                               (long double)delta[2]);
                            /* 0x300212df reloads the table entry after all three
                             * rand calls instead of carrying the pre-test value. */
                            cgame_syscall(CG_PLAY_EFFECT_ORIENTED, cgs_media_vehicleTreadEffects[material], (intptr_t)effectOrigin,
                                          (intptr_t)delta);
                        }
                    }
                }
            }
        }
    }

    /* --- Interpolated misc time (0x30021340..0x3002136a) ----------------- *
     * currentTime < 0 keeps the -1 sentinel; otherwise convert the interpolated
     * seconds back to integer milliseconds with `_ftol2`. */
    if (veh->currentState.time < 0) {
        veh->miscTime = -1;
    } else {
        veh->miscTime = coduo_fp_to_i32_extended(interpolatedTimeSeconds * VEH_SECONDS_TO_MILLISECONDS);
    }
}
