#ifndef QCOMMON_VEHICLE_TYPES_H
#define QCOMMON_VEHICLE_TYPES_H

#include <stdint.h>

/* Shared player/vehicle type domain. The original game-module parser's
 * six-entry vehicleTypeNames table labels value zero "** unknown **"; cgame
 * consumes the same values from playerState_t and entityState_t. */
typedef enum vehicle_type_e {
    VEHICLE_TYPE_UNKNOWN = 0,
    VEHICLE_TYPE_4_WHEEL = 1,
    VEHICLE_TYPE_TANK = 2,
    VEHICLE_TYPE_PLANE = 3,
    VEHICLE_TYPE_BOAT = 4,
    VEHICLE_TYPE_ARTILLERY = 5,
    VEHICLE_TYPE_COUNT = 6
} vehicle_type_t;

typedef char vehicle_type_storage_size[sizeof(vehicle_type_t) == sizeof(int32_t) ? 1 : -1];

/* Packed entityState_t::vehicleAnimState lanes. Both Windows cgame export
 * bodies and the Windows/Linux game export bodies clear and replace these
 * same 3-bit position, 3-bit type, and 2-bit motion windows in this order. */
enum {
    VEHICLE_ANIM_STATE_POS_SHIFT = 0,
    VEHICLE_ANIM_STATE_POS_MASK = 0x00000007u,
    VEHICLE_ANIM_STATE_TYPE_SHIFT = 3,
    VEHICLE_ANIM_STATE_TYPE_MASK = 0x00000038u,
    VEHICLE_ANIM_STATE_MOTION_SHIFT = 6,
    VEHICLE_ANIM_STATE_MOTION_MASK = 0x000000c0u
};

#endif
