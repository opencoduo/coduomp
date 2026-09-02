#include "bg_animation.h"
#include "bg_animation_services.h"
#include "bg_pmove.h"
#include "bg_weapon.h"
#include "qcommon/vehicle_types.h"

#include <stdint.h>

/*
 * The Windows cgame/game functions have the same instruction graph after
 * absolute globals and their module-owned fatal-error edge are normalized:
 *
 *   BG_AnimUpdatePlayerStateConditions  0x300035f0 / 0x200035d0
 *   BG_AnimPlayerConditions             0x30004860 / 0x20004840
 *
 * Linux retains the corresponding bodies at RVA 0x0001c973 and 0x0001e194.
 * Supporting Mac cgame/game preserve the canonical names, equal sizes, and
 * matching calls to BG_UpdateConditionValue and BG_GetInfoForWeapon. Those
 * calls expose the original source structure that MSVC inlined into direct
 * condition-word stores in the Windows images.
 */

enum {
    BG_PLAYER_STATE_SUPPRESS_INPUT_CONDITION = 0x00800000u,
    BG_PMOVE_INPUT_CONDITION_BIT = 0x01,
    BG_VEHICLE_POSITION_FIRST = 1,
    BG_VEHICLE_POSITION_SECOND = 2,
    BG_VEHICLE_POSITION_FIRST_PASSENGER = 3,
    BG_VEHICLE_POSITION_LAST_PASSENGER = 6
};

void BG_AnimUpdatePlayerStateConditions(pmove_t *pmove)
{
    playerState_t *player = pmove->ps;
    const int32_t clientNum = player->psClientNum;
    int32_t vehiclePosition;
    int32_t vehicleType;
    qboolean inputCondition;

    /* NOT_FROM_ORIGINAL_SOURCE: validate psClientNum once at this cluster
     * entrance; each condition update also validates its row access. */
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "BG_AnimUpdatePlayerStateConditions: "
                  "invalid client number %i",
                  clientNum);
        return;
    }
    BG_UpdateConditionValue(clientNum, ANIM_COND_WEAPON, player->currentWeapon, qtrue);
    BG_UpdateConditionValue(clientNum, ANIM_COND_WEAPONCLASS, BG_GetInfoForWeapon(player->currentWeapon)->weaponClass, qtrue);
    BG_UpdateConditionValue(clientNum, ANIM_COND_WEAPON_POSITION,
                            (player->entityStateFlags & EF_ADS) != 0 ? ANIM_WEAPON_POSITION_ADS : ANIM_WEAPON_POSITION_HIP, qtrue);

    if ((player->entityStateFlags & EF_IN_VEHICLE) != 0) {
        vehiclePosition = player->vehiclePosition;
        if (vehiclePosition == BG_VEHICLE_POSITION_FIRST) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, ANIM_MOUNT_VEHICLE_DRIVER, qtrue);
        } else if (vehiclePosition == BG_VEHICLE_POSITION_SECOND) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, ANIM_MOUNT_VEHICLE_GUNNER, qtrue);
        } else if (vehiclePosition >= BG_VEHICLE_POSITION_FIRST_PASSENGER && vehiclePosition <= BG_VEHICLE_POSITION_LAST_PASSENGER) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, vehiclePosition + 1, qtrue);
        }

        vehicleType = player->vehicleType;
        if (vehicleType == VEHICLE_TYPE_TANK) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_TANK, qtrue);
        } else if (vehicleType == VEHICLE_TYPE_4_WHEEL) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_JEEP, qtrue);
        } else if (vehicleType == VEHICLE_TYPE_ARTILLERY) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_FLAK88, qtrue);
        } else {
            bg_compat_player_state_vehicle_type_error(vehicleType);
        }

        BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE_MOTION, player->vehicleMotion, qtrue);
    } else {
        BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED,
                                (player->entityStateFlags & EF_FORCED_STANCE_MASK) != 0 ? ANIM_MOUNT_MG42 : ANIM_MOUNT_UNUSED, qtrue);
        BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_UNUSED, qtrue);
        BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE_MOTION, ANIM_VEHICLE_MOTION_UNUSED, qtrue);
    }

    BG_UpdateConditionValue(clientNum, ANIM_COND_UNDERHAND, player->viewAngles[0] > 0.0f, qtrue);

    inputCondition = (pmove->command.buttons & BG_PMOVE_INPUT_CONDITION_BIT) != 0;
    if ((player->playerStateFlags & BG_PLAYER_STATE_SUPPRESS_INPUT_CONDITION) != 0) {
        inputCondition = qfalse;
    }
    BG_UpdateConditionValue(clientNum, ANIM_COND_FIRING, inputCondition, qtrue);
}

void BG_AnimPlayerConditions(const entityState_t *entity, clientInfo_t *clientInfo)
{
    const int32_t clientNum = entity->clientNum;
    const int32_t weapon = entity->weapon;
    const uint32_t entityFlags = entity->eFlags;
    const uint32_t vehicleAnimState = (uint32_t)entity->vehicleAnimState;
    const int32_t vehiclePosition = (int32_t)((vehicleAnimState & VEHICLE_ANIM_STATE_POS_MASK) >> VEHICLE_ANIM_STATE_POS_SHIFT);
    const int32_t vehicleType = (int32_t)((vehicleAnimState & VEHICLE_ANIM_STATE_TYPE_MASK) >> VEHICLE_ANIM_STATE_TYPE_SHIFT);
    const int32_t vehicleMotion = (int32_t)((vehicleAnimState & VEHICLE_ANIM_STATE_MOTION_MASK) >> VEHICLE_ANIM_STATE_MOTION_SHIFT);
    const uint32_t animationIndex = (uint32_t)entity->legsAnim & ~ANIM_TOGGLEBIT;
    const bg_static_animation_t *animation = &bgs.animationTable.entries[animationIndex];

    /* NOT_FROM_ORIGINAL_SOURCE: validate clientNum before condition updates;
     * each update also validates its row access. animationIndex is implicitly
     * bounded: legsAnim is a 10-bit
     * netfield and clearing ANIM_TOGGLEBIT leaves the table's 0..511 domain. */
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "BG_AnimPlayerConditions: invalid client number %i",
                  clientNum);
        return;
    }
    BG_UpdateConditionValue(clientNum, ANIM_COND_WEAPON, weapon, qtrue);
    BG_UpdateConditionValue(clientNum, ANIM_COND_WEAPONCLASS, BG_GetInfoForWeapon(weapon)->weaponClass, qtrue);
    BG_UpdateConditionValue(clientNum, ANIM_COND_WEAPON_POSITION,
                            (entityFlags & EF_ADS) != 0 ? ANIM_WEAPON_POSITION_ADS : ANIM_WEAPON_POSITION_HIP, qtrue);

    if ((entityFlags & EF_IN_VEHICLE) != 0) {
        if (vehiclePosition == BG_VEHICLE_POSITION_FIRST) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, ANIM_MOUNT_VEHICLE_DRIVER, qtrue);
        } else if (vehiclePosition == BG_VEHICLE_POSITION_SECOND) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, ANIM_MOUNT_VEHICLE_GUNNER, qtrue);
        } else if (vehiclePosition >= BG_VEHICLE_POSITION_FIRST_PASSENGER && vehiclePosition <= BG_VEHICLE_POSITION_LAST_PASSENGER) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, vehiclePosition + 1, qtrue);
        }

        if (vehicleType == VEHICLE_TYPE_TANK) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_TANK, qtrue);
        } else if (vehicleType == VEHICLE_TYPE_4_WHEEL) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_JEEP, qtrue);
        } else if (vehicleType == VEHICLE_TYPE_ARTILLERY) {
            BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_FLAK88, qtrue);
        } else {
            bg_compat_player_entity_vehicle_type_error(vehicleType);
        }

        BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE_MOTION, vehicleMotion, qtrue);
    } else {
        BG_UpdateConditionValue(clientNum, ANIM_COND_MOUNTED, ANIM_MOUNT_UNUSED, qtrue);
        BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE, ANIM_VEHICLE_UNUSED, qtrue);
        BG_UpdateConditionValue(clientNum, ANIM_COND_VEHICLE_MOTION, ANIM_VEHICLE_MOTION_UNUSED, qtrue);
    }

    BG_UpdateConditionValue(clientNum, ANIM_COND_UNDERHAND, clientInfo->viewPitch > 0.0f, qtrue);
    BG_UpdateConditionValue(clientNum, ANIM_COND_CROUCHING, (entityFlags & EF_CROUCHING) != 0, qtrue);
    BG_UpdateConditionValue(clientNum, ANIM_COND_FIRING, (entityFlags & EF_FIRING) != 0, qtrue);

    if (animation->stateFlags != 0) {
        BG_UpdateConditionValue(clientNum, ANIM_COND_MOVETYPE, (int32_t)animation->stateFlags, qfalse);
    }

    if ((animation->flags & BG_ANIM_ENTRY_STRAFE_LEFT) != 0) {
        BG_UpdateConditionValue(clientNum, ANIM_COND_STRAFING, ANIM_STRAFE_LEFT, qtrue);
    } else if ((animation->flags & BG_ANIM_ENTRY_STRAFE_RIGHT) != 0) {
        BG_UpdateConditionValue(clientNum, ANIM_COND_STRAFING, ANIM_STRAFE_RIGHT, qtrue);
    } else {
        BG_UpdateConditionValue(clientNum, ANIM_COND_STRAFING, ANIM_STRAFE_NOT, qtrue);
    }
}
