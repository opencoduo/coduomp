// Source: uo_cgame_mp_x86.dll 0x30041550..0x30041a2d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30041550_30041a2d.mcode
//
// CG_CalcTurretViewValues — update the mounted-turret camera from the followed
// entity's tag_player transform.  The mechanical G_MoverPush name is rejected:
// this function contains no mover push/rollback loop; it is called at the turret
// view dispatch point in CG_CalcViewValues and operates entirely on cgame view,
// DObj-tag, weapon-sway, and refdef state.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../client_recovered.h"
#include "../globals.h"

enum {
    CG_TURRET_ENTITY_NONE = 1023
};

#define CG_TURRET_VIEW_FLAG_MASK ((uint32_t)(EF_FORCE_PRONE | EF_FORCE_CROUCH))
#define CG_TURRET_SWAY_FLAG ((uint32_t)0x00000200u)
#define CG_SHORT_TO_ANGLE_SCALE 0.0054931640625f
#define CG_TURRET_SWAY_SCALE 2.0f
#define CG_TURRET_OSCILLATION_PERIOD 100
#define CG_TURRET_OSCILLATION_HALF_PERIOD 50
#define CG_TURRET_FORWARD_KICK_SCALE (-0.5f)
#define CG_RANDOM_SIGNED_DENOMINATOR 32768.0f

void CG_CalcTurretViewValues(void)
{
    if ((cg_predictedPlayerState.entityStateFlags & CG_TURRET_VIEW_FLAG_MASK) == 0) {
        return;
    }

    int32_t entityNum = cg_predictedPlayerState.viewLockedEntityNum;
    if (entityNum == CG_TURRET_ENTITY_NONE) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_CalcTurretViewValues: invalid view-lock "
                  "entity %i",
                  entityNum);
        return;
    }
    centity_t *cent = &cg_entities[entityNum];
    centity_t *turret = cent;
    void *dobj = (void *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number);
    if (dobj == NULL) {
        return;
    }

    DObjSkelMat tagPlayerMatrix;
    if (!CG_DObjGetWorldTagMatrix(dobj, "tag_player", cent,
                                         &tagPlayerMatrix)) {
        Com_Error(ERR_FATAL, cg_turretMissingTagPlayerError);
    }

    CG_CalcEntityLerpPositions(cent);

    /* 0x300415e6..0x3004163f: interpolate the four paired current/next turret
     * angle scalars with the snapshot interpolation fraction. */
    float baseYaw = LerpAngle(cent->currentState.iconBaseYaw,
                              cent->nextState.leanf,
                              cg_frameInterpolation);
    float currentBodyPitch = CG_FloatFromBits(
        (uint32_t)turret->currentState.vehicleBodyPitchPacked);
    float nextBodyPitch = CG_FloatFromBits(
        (uint32_t)turret->nextState.vehicleBodyPitchPacked);
    float bodyPitch = LerpAngle(currentBodyPitch, nextBodyPitch,
                                cg_frameInterpolation);
    float secondaryYaw = LerpAngle(turret->currentState.vehicleSecondaryBaseYaw,
                                   turret->nextState.vehicleSecondaryBaseYaw,
                                   cg_frameInterpolation);
    float currentBodyRoll = CG_FloatFromBits(
        (uint32_t)turret->currentState.vehicleBodyRollPacked);
    float nextBodyRoll = CG_FloatFromBits(
        (uint32_t)turret->nextState.vehicleBodyRollPacked);
    float bodyRoll = LerpAngle(currentBodyRoll, nextBodyRoll,
                               cg_frameInterpolation);

    /* The machine updates pitch and yaw only. Each component is a symmetric clamp
     * of the signed refdef/entity delta into [lowerAdds[axis], limits[axis]]:
     * delta > limits -> weap+limits (0x3004167b test ah,0x41/jne); else delta <
     * lowerAdds -> weap+lowerAdds (0x30041689 fcomp delta,lowerAdds / 0x3004168f
     * test ah,0x5/jp: store only on delta < lowerAdds); else unchanged. A prior pass
     * read the else-arm as `lowerAdds[axis] > 0.0f`, but the bytes reload the SAME
     * delta (0x30041685 fld [esp+0x10]) and compare it against lowerAdds -- no 0.0f
     * constant is loaded. */
    float limits[2] = { baseYaw, bodyPitch };
    float lowerAdds[2] = { secondaryYaw, bodyRoll };
    for (int axis = 0; axis < 2; ++axis) {
        float delta = AngleSubtract(cg_refdefViewAngles[axis],
                                    cent->lerpAngles[axis]);
        if (delta > limits[axis]) {
            cg_refdefViewAngles[axis] =
                cent->lerpAngles[axis] + limits[axis];
        } else if (delta < lowerAdds[axis]) {
            cg_refdefViewAngles[axis] =
                cent->lerpAngles[axis] + lowerAdds[axis];
        }
    }

    vec3_t adjustedAngles;
    /* Stage 1 (0x300416fa..0x30041725): adjustedAngles[i] = cg_refdefViewAngles[i]
     * - deltaScale[i], a PLAIN componentwise subtraction. The DLL emits three
     * inline FSUB here (0x30041700/0x30041713/0x30041721), NOT calls -- only the
     * SECOND stage below (0x30041729 CALL AngleSubtract) normalizes. Using
     * AngleSubtract for stage 1 too would wrongly wrap the raw view-minus-delta
     * difference in AngleNormalize180Accurate (they diverge once |view - delta| > 180).
     * deltaScale is (float)deltaAngles[i] * SCALE stored to a float slot (the
     * product rounds to float, FSTP DWORD) before the FSUB, so it is a float
     * temp -- the (float) int cast is faithful (FILD; FSTP DWORD; FLD DWORD). */
    for (int axis = 0; axis < 3; ++axis) {
        float deltaInteger = (float)coduo_int32_from_bits(
            (uint32_t)cg_predictedPlayerState.deltaAngles[axis]);
        float deltaScale = (float)((long double)deltaInteger *
                                   (long double)CG_SHORT_TO_ANGLE_SCALE);
        adjustedAngles[axis] = (float)(
            (long double)cg_refdefViewAngles[axis] - (long double)deltaScale);
    }
    /* Stage 2 (0x30041729/0x3004173d/0x30041752): here the DLL DOES call
     * AngleSubtract (= a - b then AngleNormalize180Accurate). */
    adjustedAngles[0] = AngleSubtract(adjustedAngles[0], cg_adsViewErrorAngles[0]);
    adjustedAngles[1] = AngleSubtract(adjustedAngles[1], cg_adsViewErrorAngles[1]);
    adjustedAngles[2] = AngleSubtract(adjustedAngles[2], cg_adsViewErrorAngles[2]);
    cgame_syscall(CG_VEH_VIEW_ANGLE_DELTA, (intptr_t)adjustedAngles);

    playerState_t swayState;
    memcpy(&swayState, &cg_predictedPlayerState, sizeof(swayState));
    swayState.currentWeapon = BG_GetWeaponIndexForName("thompson_MP");
    BG_CalculateWeaponPosition_Sway(&swayState,
                                    cg_turretViewSwayPreviousViewAngles,
                                    cg_turretViewSwayOffset,
                                    cg_turretViewSwayViewAngles,
                                    CG_TURRET_SWAY_SCALE, cg_frametime);

    int32_t phase = coduo_int32_from_bits(cg_time) % CG_TURRET_OSCILLATION_PERIOD;
    if (phase > CG_TURRET_OSCILLATION_HALF_PERIOD) {
        phase = CG_TURRET_OSCILLATION_PERIOD - phase;
    }
    float phaseInteger = (float)phase;
    float phaseScale = (float)((long double)phaseInteger /
                               (long double)(float)CG_TURRET_OSCILLATION_HALF_PERIOD);

    if (!CG_DObjGetWorldTagMatrix(dobj, "tag_player", cent,
                                         &tagPlayerMatrix)) {
        return;
    }

    cg_refdef.vieworg[0] = tagPlayerMatrix.origin[0];
    cg_refdef.vieworg[1] = tagPlayerMatrix.origin[1];
    cg_refdef.vieworg[2] =
        tagPlayerMatrix.origin[2] - cg_predictedPlayerState.viewHeightCurrent;

    vec3_t displacement = { 0.0f, 0.0f, 0.0f };
    if ((cg_snap->ps.entityStateFlags & CG_TURRET_SWAY_FLAG) != 0 &&
        cent->currentState.hudTagMask == 0) {
        float kick = phaseScale * CG_TURRET_FORWARD_KICK_SCALE;
        displacement[0] = kick * tagPlayerMatrix.axis[0][0];
        displacement[1] = kick * tagPlayerMatrix.axis[0][1];
        displacement[2] = kick * tagPlayerMatrix.axis[0][2];
    }

    displacement[0] += tagPlayerMatrix.axis[0][0] * cg_turretViewSwayOffset[0];
    displacement[1] += tagPlayerMatrix.axis[0][1] * cg_turretViewSwayOffset[0];
    displacement[2] += tagPlayerMatrix.axis[0][2] * cg_turretViewSwayOffset[0];
    displacement[0] += tagPlayerMatrix.axis[1][0] * cg_turretViewSwayOffset[1];
    displacement[1] += tagPlayerMatrix.axis[1][1] * cg_turretViewSwayOffset[1];
    displacement[2] += tagPlayerMatrix.axis[1][2] * cg_turretViewSwayOffset[1];
    displacement[0] += tagPlayerMatrix.axis[2][0] * -cg_turretViewSwayOffset[2];
    displacement[1] += tagPlayerMatrix.axis[2][1] * -cg_turretViewSwayOffset[2];
    displacement[2] += tagPlayerMatrix.axis[2][2] * -cg_turretViewSwayOffset[2];

    cg_refdef.vieworg[0] += displacement[0];
    cg_refdef.vieworg[1] += displacement[1];
    cg_refdef.vieworg[2] += displacement[2];

    if ((cg_snap->ps.entityStateFlags & CG_TURRET_SWAY_FLAG) != 0 &&
        cent->currentState.hudTagMask == 0) {
        const weaponInfo_t *weapon = bg_weaponInfos[cent->currentState.weapon];
        /* The signed-random ((float)rand/32768)*2-1 is kept in st(0) and multiplied by
         * the jitter directly (0x300419c2..0x300419d0 pitch, 0x300419fa..0x30041a08 yaw):
         * the binary never rounds it to a float slot, so it must not be a float local.
         * The (float)rand cast is real (FILD;FSTP DWORD;FLD DWORD @0x300419aa..be). */
        float pitchRandomInteger = (float)coduo_crt_rand();
        long double pitchRandom =
            ((long double)pitchRandomInteger /
             (long double)CG_RANDOM_SIGNED_DENOMINATOR) * 2.0L - 1.0L;
        cg_refdefViewAngles[0] = (float)(
            pitchRandom * (long double)weapon->vertViewJitter +
            (long double)cg_refdefViewAngles[0]);

        float yawRandomInteger = (float)coduo_crt_rand();
        long double yawRandom =
            ((long double)yawRandomInteger /
             (long double)CG_RANDOM_SIGNED_DENOMINATOR) * 2.0L - 1.0L;
        cg_refdefViewAngles[1] = (float)(
            yawRandom * (long double)weapon->horizViewJitter +
            (long double)cg_refdefViewAngles[1]);
    }

}
