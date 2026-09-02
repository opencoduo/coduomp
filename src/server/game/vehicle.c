/*
 * Source reconstruction for vehicle functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "qcommon/info.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"
#include "scr_vm.h"
#include "level_locals.h"
#include "compat/libm/coduo_libm.h"

/* Private descriptors used only by the vehicle parser and turret state. */
typedef enum vehicle_parse_field_type_e {
    VEH_SCRIPT_FIELD_STRING = 1,
    VEH_SCRIPT_FIELD_FLOAT = 4,
    VEH_SCRIPT_FIELD_INT = 5,
    VEH_PARSE_FIELD_TYPE_VEHICLE = 8
} vehicle_parse_field_type_t;

typedef struct vehicle_node_spawn_field_s {
    const char *name;
    size_t offset;
    int32_t type;
} vehicle_node_spawn_field_t;

typedef enum vehicle_turret_activity_slot_e {
    VEH_TURRET_ACTIVITY_PRIMARY,
    VEH_TURRET_ACTIVITY_GUNNER
} vehicle_turret_activity_slot_t;

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

/* Vehicle collision parameters */
#define VEH_COLLISION_SPEED_THRESHOLD 200.0f
#define VEH_COLLISION_MAX_SPEED 400.0f
#define VEH_COLLISION_KNOCKBACK_BASE 150.0f
#define VEH_COLLISION_KNOCKBACK_MULT 1.3f
#define VEH_COLLISION_VERTICAL_BASE 200.0f
#define VEH_COLLISION_VERTICAL_SCALE 100.0f
#define VEH_COLLISION_SOUND_Z_SCALE 1500.0f
#define VEH_COLLISION_DAMAGE_COOLDOWN 500
#define VEH_PARSE_ERROR_UNKNOWN_TYPE COM_ERROR_MARKER "Unknown vehicle type [s]\n"
#define VEH_PARSE_ERROR_BAD_FIELD_TYPE COM_ERROR_MARKER "Bad vehicle field type %i\n"
#define VEH_SCRIPT_ENTITY_ERROR_TYPE "entity %i is not a script_vehicle\n"
#define VEH_SCRIPT_ENTITY_ERROR_STATE "entity %i doesn't have a script_vehicle\n"
#define VEH_SCRIPT_ENTITY_ERROR_RANGE "i is not a valid entity number\n"
#define VEH_SCRIPT_ENTITY_ERROR_TAG COM_ERROR_MARKER "Script vehicle [%s] needs [%s]\n"
#define VEH_SCRIPT_ENTITY_ERROR_NOT_PRECACHED "vehicle '%s' not precached"
#define VEH_PHYSICS_CLIENT_COUNT 64
#define VEH_PHYSICS_NEARBY_RADIUS_SQ 1048576.0f
#define VEH_GROUND_SIDE_CLAMP_SPEED 20.0f
#define VEH_GROUND_FRICTION_SCALE 0.05f
#define VEH_GROUND_STEER_MIN_FORWARD 100.0f
#define VEH_GROUND_STEER_MIN_SIDE 1.0f
#define VEH_GROUND_STEER_MAX_STEP 5.0f
#define VEH_APPROACH_FRAME_SECONDS 0.05f
#define VEH_APPROACH_EPSILON 0.001f /* original float32 0x3a83126f */
#define VEH_ENTITY_SVFLAGS 4u
#define VEH_ENTITY_CONTENTS 0x2080
#define VEH_ENTITY_SPAWNFLAG_TRIGGER 1u
#define VEH_ENTITY_SPAWNFLAG_CONTENTS 0x200000u
#define VEH_ENTITY_CLIPMASK 0x201
#define VEH_ENTITY_FLAG_LINKED 0x20000u
#define VEH_ENTITY_THINK_INTERVAL_MS 50
#define VEH_ENTITY_DEFAULT_ANIM_VALUE 127
#define VEH_COLLMAP_SCRIPT_CONTENTS 0xa00000u
#define VEH_COLLMAP_BASE_CONTENTS 0x800000u
#define VEH_SCRIPTED_INPUT_CLAMP 30.0f
#define VEH_INPUT_SCALE_MAX 1.0f
#define VEH_THROTTLE_SCALE_MULTIPLIER 2.0f
#define VEH_PHYSICS_FRAME_SECONDS 0.05f
#define VEH_DEFAULT_SPEED_SCALE 0.8f
#define VEH_REVERSE_SPEED_SCALE 0.5f
#define VEH_IDLE_BRAKE_STEP 10.0f
#define VEH_4WHEEL_LATERAL_SCALE 0.75f
#define VEH_4WHEEL_LATERAL_DIVISOR 80.0f
#define VEH_4WHEEL_LATERAL_SPEED 400.0f
#define VEH_4WHEEL_STEER_ANGLE 60.0f
#define VEH_4WHEEL_STEER_CENTER 10.0f
#define VEH_4WHEEL_STEER_DAMP_RANGE 60.0f
#define VEH_4WHEEL_MIN_SPEED 10.0f
#define VEH_4WHEEL_RATE_STEP_SCALE 4.0f
#define VEH_ROLL_ACCEL_SCALE 4.0f
#define VEH_ROLL_ACCEL_MAX_STEP 3.0f
#define VEH_ROLL_ACCEL_CLAMP 16.0f
#define VEH_ROLL_ACCEL_SNAP 0.5f
#define VEH_STEERING_ROLL_SPEED 40.0f
#define VEH_DRIVER_BUTTON_STRAFE (1u << 5)
#define VEH_UNSTICK_VECTOR_COUNT 26
#define VEH_GROUND_PROBE_STEP 0.25f
#define VEH_GROUND_PROBE_FALLBACK 0.25f
#define VEH_GROUND_TRACE_DOWN_STEP 1.0f
#define VEH_GROUND_NORMAL_MIN_Z 0.7f
#define VEH_GROUND_MAX_IMPACT_DOT 10.0f
#define VEH_SLIDE_GRAVITY_STEP 40.0f
#define VEH_SLIDE_MAX_BUMPS 4
#define VEH_SLIDE_MAX_CLIP_PLANES 5
#define VEH_SLIDE_OVERCLIP 1.01f
#define VEH_SLIDE_CLIP_EPSILON 0.1f
#define VEH_SLIDE_RECOLLIDE_DOT 0.99f
#define VEH_STEP_TOUCH_COUNT 4
#define VEH_SUSPENSION_WHEEL_COUNT_4WHEEL 4
#define VEH_SUSPENSION_WHEEL_COUNT_TANK 6
#define VEH_SUSPENSION_TRACE_UP 100.0f
#define VEH_SUSPENSION_TRACE_DOWN 256.0f
#define VEH_SUSPENSION_TRACE_MASK 0x211
#define VEH_SUSPENSION_TRACE_MASK_ALT 0x10211
#define VEH_SUSPENSION_ANGLE_RATE 6.0f
#define VEH_SUSPENSION_ANGLE_CLAMP 60.0f
#define VEH_SUSPENSION_ANGULAR_SCALE 20.0f
#define VEH_COLLISION_MODE_ALT_TRACE 2u
#define VEH_TURRET_STATE_INACTIVE 0
#define VEH_TURRET_STATE_WINDDOWN 1
#define VEH_TURRET_STATE_ACTIVE 2
#define VEH_CLIENT_FIRE_BUTTON 1u
#define VEH_CLIENT_ALT_FIRE_BUTTON (1u << 5)
#define VEH_CLIENT_GUNNER_OVERHEAT_FLAG 0x00800000u
#define VEH_FIRE_TIME_THINK_STEP_MS 50
#define VEH_ALT_TRACE_TARGET_DISTANCE 10240.0f
#define VEH_ALT_TRACE_CONTENTS_CLEAR 0x02000000u
#define VEH_ALT_TRACE_LOCATIONAL_MASK 0x2091
#define VEH_TEXTURE_SCROLL_SPEED_SCALE 176.0f
#define VEH_TURRET_STALL_DELTA 2.0f
#define VEH_TURRET_NOTIFY_EPSILON 1.0f
#define VEH_MOTION_ANIM_FRAME_SECONDS 0.05f
#define VEH_MOTION_ANIM_PHASE_STEP 36.0f
#define VEH_MOTION_ANIM_CENTER 127.0f
#define VEH_MOTION_ANIM_SCALE (127.0f / 30.0f)
#define VEH_PI 3.1415927f /* original float32 0x40490fdb */
#define VEH_PATH_DEBUG_SEGMENT_DOT 0.9999f /* original float32 0x3f7ff972 */
#define VEH_PATH_DEBUG_MAX_ITERS 50000
#define VEH_PATH_DEBUG_BOX_HALF_SIZE 4.0f
#define VEH_MAX_PATH_NODES 3000
#define VEH_PATH_NODE_MAX_COUNT_ERROR COM_ERROR_MARKER "Hit max vehicle path node count [%d]\n"
#define VEH_PATH_NODE_NO_NAME_ERROR COM_ERROR_MARKER "Vehicle path node( %f, %f, %f ) found with no name\n"
#define VEH_PATH_NODE_NEGATIVE_SPEED_ERROR COM_ERROR_MARKER "Vehicle path node at( %f, %f, %f ) has negative speed\n"
#define VEH_NOT_VEHICLE_NODE_ERROR "Not a vehicle node"
#define VEH_NODE_KEY_NOT_STRING_ERROR "key is not internally a string"
#define VEH_NODE_MULTIPLE_MATCH_ERROR "GetVehicleNode used with more than one node"
#define VEH_PASSENGER_SLOT_COUNT 7
#define VEH_PASSENGER_SLOT_DRIVER 1
#define VEH_PASSENGER_SLOT_GUNNER 2
#define VEH_PASSENGER_EXTRA_SLOT_FIRST 3
#define VEH_PASSENGER_SLOT_LAST (VEH_PASSENGER_SLOT_COUNT - 1)
#define VEH_PASSENGER_SLOT_WRAP VEH_PASSENGER_SLOT_COUNT
#define VEH_PATH_ANGLE_SMOOTH_PITCH 6.0f
#define VEH_PATH_ANGLE_SMOOTH_YAW 4.0f
#define VEH_PATH_SPEED_MODE_NONE 0
#define VEH_PATH_SPEED_MODE_SCRIPT 1
#define VEH_PATH_SPEED_MODE_PATH 2
#define VEH_PATH_INVALID_POSITION_ERROR COM_ERROR_MARKER "VEH_AttachBoneIndexForPosition: unknown position\n"
#define VEH_MAX_SCR_VEHICLES 64
#define VEH_VEHICLE_FILE_MAGIC "VEHICLEFILE"
#define VEH_VEHICLE_FILE_FOLDER "vehicles"
#define VEH_VEHICLE_FILE_EXTENSION ""
#define VEH_VEHICLE_FILE_LIST_SIZE 4096
#define VEH_VEHICLE_FILE_BUFFER_SIZE 8192
#define VEH_VEHICLE_PARSE_CUSTOM_MAX 9
#define VEH_VEHICLE_MAX_FILES_ERROR COM_ERROR_MARKER "Max vehicle files allowed is %i, found %i.\n"
#define VEH_VEHICLE_LOAD_ERROR COM_ERROR_MARKER "Could not load vehicle file [%s]\n"
#define VEH_VEHICLE_MAGIC_ERROR COM_ERROR_MARKER "File [%s] is not a vehicle file\n"
#define VEH_VEHICLE_SIZE_ERROR COM_ERROR_MARKER "Vehicle file [%s] is to big\n"
#define VEH_VEHICLE_VALID_ERROR COM_ERROR_MARKER "Vehicle file [%s] is not valid\n"
#define VEH_VEHICLE_PATH_FORMAT "%s/%s"
#define VEH_VEHICLE_INLINE_STRING_SIZE 64
#define VEH_VEHICLE_HIT_PERSON_SOUND "vehicle_hit_person"
#define VEH_VEHICLE_MAX_COUNT_ERROR COM_ERROR_MARKER "Hit max vehicle count [%d]\n"
#define VEH_VEHICLE_INFO_LOOKUP_ERROR COM_ERROR_MARKER "Can't find info for script vehicle [%s]\n"
#define VEH_LINK_ALREADY_USING_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Player is already using a vehicle\n"
#define VEH_LINK_ALREADY_OWNER_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Player already has an owner\n"
#define VEH_LINK_NOT_OWNER_COMMAND "e \"GMI_GAME_VEHICLE_NOT_OWNER\""
#define VEH_LINK_FULL_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Vehicle is full, should not be usable\n"
#define VEH_LINK_ATTACH_BONE_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Attach bone not found in vehicle (%s)\n"
#define VEH_LINK_DRIVER_BONE_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Trying to use vehicle without a bone [tag_player]\n"
#define VEH_LINK_GUNNER_BONE_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Trying to use vehicle without a bone [tag_secondary_gun]\n"
#define VEH_LINK_BODY_BONE_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Trying to use vehicle without a bone [tag_body]\n"
#define VEH_LINK_CANNOT_LINK_ERROR COM_ERROR_MARKER "VEH_LinkPlayer: Cannot link to vehicle bone [%s]\n"
#define VEH_UNLINK_NOT_USING_ERROR COM_ERROR_MARKER "VEH_UnlinkPlayer: Player is not using a vehicle\n"
#define VEH_UNLINK_NO_OWNER_WARNING COM_ERROR_MARKER "VEH_UnlinkPlayer: Player doesn't have an owner\n"
#define VEH_UNLINK_EXIT_DELAY_COMMAND "e \"GMI_GAME_VEHICLE_EXIT_DELAY\""
#define VEH_UNLINK_BLOCKED_COMMAND "e \"GMI_GAME_VEHICLE_UNLINK_BLOCKED\""
#define VEH_CLIENT_COMMAND_RUN "v cl_run 1"
#define VEH_CLIENT_COMMAND_STANCE "v cl_stance 0"
#define VEH_CLIENT_STANCE_FLAG_GUNNER 0x00800000u
#define VEH_VEHICLE_S_FLAG_DRIVER 0x00100000u
#define VEH_LINK_RETRY_DELAY_MS 250
#define VEH_LINK_TANK_RETRY_DELAY_MS 1500
#define VEH_EXIT_DELAY_MS 1500
#define VEH_EXIT_ROLL_DELAY_MS 3000
#define VEH_EXIT_ANIM_GRACE_MS 750
#define VEH_PLAYER_REENTER_DELAY_MS 2000
#define VEH_EXIT_VERTICAL_BOOST 200.0f
#define VEH_USABLE_SCRIPT_CONTENTS 0x00200000u
#define VEH_USABLE_MAX_SPEED 100.0f
#define VEH_INFO_SPEED_SCALE 17.6f
#define VEH_INFO_HALF_SCALE 0.5f
#define VEH_INFO_SOUND_ALIAS_COUNT 7
#define VEH_PLAYER_BLEND_TAG_FAIL_WARNING "WARNING: aborting player positioning on turret since '%s' does not exist\n"
#define VEH_PLAYER_BLEND_ANIM_CHILD_ERROR COM_ERROR_MARKER "Player anim '%s' has no children"
#define VEH_PLAYER_BLEND_LEAN_YAW_LIMIT 45.0f
#define VEH_PLAYER_BLEND_RATE_SCALE 10.0f
#define VEH_OWNER_ICON_SVFLAG_LINKED 0x00000800u
#define VEH_OWNER_ICON_DELAY_MS 100
#define VEH_OWNER_ICON_TIME_SCALE 1000.0f
#define VEH_SOUND_THINK_INTERVAL_MS 50
#define VEH_RUN_SOUND_MIN_ANGULAR_SPEED 0.1 /* double: fabs compare loads QWORD 0.1 @0x876fa */
#define VEH_TAG_FLASH_COUNT 4
#define VEH_WHEEL_TAG_COUNT 6
#define VEH_PASSENGER_TAG_COUNT 4
#define VEH_POPOUT_DISABLED_ERROR COM_ERROR_MARKER "G_VehiclePopOut: Popout disabled\n"
#define VEH_POPOUT_BONE_ERROR COM_ERROR_MARKER "G_VehiclePopOut: Trying to use vehicle without a bone [%s]\n"
#define VEH_POPOUT_LINK_ERROR COM_ERROR_MARKER "G_VehiclePopOut: Cannot link to vehicle bone [%s]\n"
#define VEH_TAG_POPOUT "tag_popout"
#define VEH_TAG_PLAYER "tag_player"
#define VEH_NONPVS_TANK_ENTITY_NONE ENTITYNUM_NONE
#define VEH_NONPVS_TANK_ENCODE_MIN -1022
#define VEH_NONPVS_TANK_ENCODE_MAX 1024
#define VEH_NONPVS_TANK_ORIGIN_LIMIT_POS 1024.0f
#define VEH_NONPVS_TANK_ORIGIN_LIMIT_NEG -1022.0f
#define VEH_NONPVS_TANK_YAW_SCALE 0x1.6c16c2p-1f /* float32 0x3f360b61 */
#define VEH_NONPVS_TANK_COORD_BIAS 255u
#define VEH_NONPVS_TANK_COORD_MASK 0x1ffu
#define VEH_NONPVS_TANK_INDEX_MASK 0x3fu
#define VEH_DAMAGE_SCALE_PERCENT 0.01f
#define VEH_DAMAGE_SCALE_RADIUS_BLEND 0.7f
#define VEH_DAMAGE_SCALE_GRENADE_HEIGHT 10.0f
#define VEH_DAMAGE_SCALE_GRENADE_RADIUS_MULT 0.8f
#define VEH_DAMAGE_SCALE_ANGLE_RADIANS 0.3490658503988659 /* 20 degrees; original double64 0x3fd657184ae74487 */
#define VEH_PUSH_AWAY_STEP 4.0f
#define VEH_PUSH_AWAY_PLAYER_RADIUS 32.0f
#define VEH_BOUNDS_EXPANSION_SCALE 1.2f
#define VEH_BOUNDS_MINS_PAD_XY -50.0f
#define VEH_BOUNDS_MINS_PAD_Z 0.0f
#define VEH_BOUNDS_MAXS_PAD_XY 50.0f
#define VEH_BOUNDS_MAXS_PAD_Z 20.0f
#define VEH_CHECK_PUSH_MAX_CLIENTS 64
#define VEH_CYCLE_SLOT_RETRY_MS 250
#define VEH_CYCLE_SLOT_COMMAND_REPEAT_MS 500
#define VEH_CYCLE_SLOT_RELINK_DELAY_MS 1500
#define VEH_CLIENT_COMMAND_DELAY_IGNORE "e \"GMI_GAME_VEHICLE_DELAY_IGNORE\""
#define VEH_DIE_PASSENGER_DAMAGE_BOOST 10
#define VEH_DIE_PASSENGER_DAMAGE 5
#define VEH_DIE_PASSENGER_Z_RAISE 32.0f
#define VEH_COLLMAP_ENTITY_TYPE 14
#define VEH_PATH_ALREADY_USED_ERROR "Vehicle is invalid on path after it's been used"
#define VEH_PATH_NOT_ATTACHED_ERROR "Can't start path on a vehicle that hasn't been attached"
#define VEH_WAIT_SPEED_NEGATIVE_ERROR "Cannot have a negative wait speed on a vehicle"
#define VEH_SET_SPEED_NEGATIVE_ERROR "Cannot set negative speed on vehicle"
#define VEH_SET_ACCELERATION_NEGATIVE_ERROR "Cannot set negative acceleration on vehicle"
#define VEH_JOLT_SPEED_FRACTION_ERROR "Speed fraction must be between [0,1]"
#define VEH_JOLT_DECELERATION_NEGATIVE_ERROR "Deceleration can't be negative"
#define VEH_WAIT_SPEED_SCALE 17.6f
#define VEH_COLLISION_MODE_USED 2u
#define VEH_PATH_ACTIVE_MODE 1u
#define VEH_FREE_ENTITY_TYPE 13
#define VEH_WHEEL_SURFACE_DEFAULT "none"
#define VEH_WHEEL_SURFACE_NO_WHEELS_ERROR "Vehicle type [%s] has no wheels\n"
#define VEH_WHEEL_SURFACE_VALID_NAMES_ERROR \
    "Valid wheel names are: [front_left, front_right, back_left, back_right, middle_left, middle_right]\n"
#define VEH_WHEEL_SURFACE_NO_MIDDLE_ERROR "Vehicle has no middle wheels\n"
#define VEH_ALREADY_IN_USE_ERROR "Vehicle is already in use"
#define VEH_TURRET_HEALTH_ERROR "Vehicle must have health to control the turret"
#define VEH_TURRET_NO_WEAPON_ERROR "No weapon specified for [%s]\n"
#define VEH_TURRET_WEAPON_TYPE_ERROR "Vehicles only support bullet and projectile weapons\n"
#define VEH_TURRET_NO_BARREL_ERROR "No tag_barrel for [%s]\n"
#define VEH_GUNNER_NO_BARREL_ERROR "No tag_secondary_gun for [%s]\n"
#define VEH_TURRET_NO_FLASH_WARNING "WARNING: No %s for [%s]\n"
#define VEH_PLAYER_VEHICLE_NO_TAG_ERROR COM_ERROR_MARKER "Player vehicle has no %s\n"
#define VEH_PLAYER_VEHICLE_NO_BARREL_ERROR COM_ERROR_MARKER "Player vehicle has no tag_barrel\n"
#define VEH_PLAYER_CONTROLLED_VEHICLE_ERROR "Must be called on a player controlled vehicle"
#define VEH_TURRET_AIM_PITCH_CLAMP 5.0f
#define VEH_TURRET_AIM_YAW_CLAMP 2.0f
#define VEH_TURRET_SCRIPTED_INPUT_SCALE 0.75f
#define VEH_EVENT_MISS_FLAG 0x80u
#define VEH_CLIENT_SESSION_STATE_SPECTATOR 3
#define VEH_DISMOUNT_TRACE_MASK_CLEAR 0x02000000u
#define VEH_DISMOUNT_TRACE_MASK_GROUND 0x00810011u
#define VEH_DISMOUNT_TRACE_MASK_DETACH 0x02810011u
#define VEH_DISMOUNT_LOS_Z_RAISE 30.0f
#define VEH_DISMOUNT_BASE_Z_RAISE 4.0f
#define VEH_DISMOUNT_SEARCH_MIN -80.0f
#define VEH_DISMOUNT_SEARCH_MAX 80.0f
#define VEH_DISMOUNT_SEARCH_STEP 20.0f
#define VEH_DISMOUNT_Z_RETRY_STEP 32.0f
#define VEH_DISMOUNT_FALLBACK_STEP 4.0f
#define VEH_DISMOUNT_FALLBACK_RETRIES 8
#define VEH_DISMOUNT_DYNAMIC_RETRIES 4
#define VEH_DISMOUNT_GROUND_DROP 256.0f
#define VEH_DISMOUNT_MIN_DIR_LENGTH 0.00001f /* original float32 0x3727c5ac */
#define VEH_DISMOUNT_SIDE_PAD 60.0f
#define VEH_DISMOUNT_FORWARD_PAD 100.0f
#define VEH_DISMOUNT_FALLBACK_Z_PAD 48.0f
#define VEH_SOLID_SWEEP_COUNT 6
#define VEH_SOLID_TRACE_MINS_XY -24.0f
#define VEH_SOLID_TRACE_MIN_Z -1.0f
#define VEH_SOLID_TRACE_MAX_XY 24.0f
#define VEH_SOLID_TRACE_MAX_Z 10.0f
#define VEH_SOLID_STANDOFF 24.0f
#define VEH_SOLID_INITIAL_PAD 48.0f
#define VEH_SOLID_INITIAL_Z_PAD 16.0f
#define VEH_SOLID_MAX_SLIDE_BUMPS 4
#define VEH_SOLID_GROUND_MIN_Z 0.7f
#define VEH_SOLID_MIN_FALL_SPEED -20.0f
#define VEH_SOLID_SCRIPT_PUSH_TIME_MS 150
#define VEH_SOLID_SCRIPT_PUSH_SPEED 700.0f
#define VEH_SOLID_MIN_SCRIPT_SCALE 0.2f
#define VEH_SOLID_MAX_SCRIPT_SCALE 0.6f
#define VEH_SOLID_MAX_OTHER_SCALE 0.5f
#define VEH_SOLID_INPUT_SPEED_BASE 400.0f
#define VEH_SOLID_INPUT_SPEED_AXIS 300.0f
#define VEH_SOLID_SPEED_SCALE_DENOM 0.75f
#define VEH_SOLID_IMPULSE_BASE 100.0f
#define VEH_SOLID_IMPULSE_MIN_SPEED 0.5f
#define VEH_SOLID_DEFLECT_MIN_SPEED 10.0f
#define VEH_SOLID_BOUNCE_MIN_SPEED 120.0f
#define VEH_SOLID_BOUNCE_MAX_SPEED 160.0f
#define VEH_SOLID_BOUNCE_MIN_PUSH 40.0f
#define VEH_SOLID_BOUNCE_SCALE 0.85f
#define VEH_SOLID_BOUNCE_SPEED 150.0f
#define VEH_SOLID_SECOND_PASS_SPEED_DIVISOR 4.0f
#define VEH_SOLID_NOTIFY_RECENT_MS 200
#define VEH_SOLID_NOTIFY_COOLDOWN_MS 1000
#define VEH_SOLID_STUCK_DAMAGE_DELAY_MS 7000
#define VEH_SOLID_STUCK_DAMAGE_GRACE_MS 8000
#define VEH_SOLID_STUCK_DAMAGE 999999
#define VEH_COLLISION_CACHE_DISTANCE_RESET 99999.0f
#define VEH_SOLID_SOUND_Z_SCALE 2500.0f
#define VEH_SOLID_TRACE_CONTENTS 0x00800000u
#define VEH_SOLID_BODY_CONTENTS CONTENTS_BODY
#define VEH_SOLID_INITIAL_CONTENTS 0x02800000u
#define VEH_SOLID_IGNORED_BODY_MASK CONTENTS_BODY
#define VEH_STRESS_FORWARD_TIME_SCALE 0x1.eb3ca4p-13f /* float32 0x39759e52 */
#define VEH_STRESS_RIGHT_TIME_SCALE 0x1.364a14p-9f  /* float32 0x3b1b250a */
#define VEH_STRESS_INPUT_SCALE 127.0f
#define VEH_SPEED_BLEND_RATE 4.0f

/* Vehicle info table */
vehicleInfo_t g_vehicleInfoTable[VEH_MAX_SCR_VEHICLES];
int16_t g_vehicleInfoCount;
vehicle_state_t vehClientPhysicsRecords[VEH_PHYSICS_CLIENT_COUNT];
const float vehUnstickOffsets[VEH_UNSTICK_VECTOR_COUNT * 3] = {
    0.0f, 0.0f,  1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,  -1.0f, 1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,  -1.0f,
    0.0f, 1.0f,  0.0f,  0.0f,  0.0f, 1.0f, 0.0f,  0.0f,  0.0f,  -1.0f, -1.0f, 0.0f,  -1.0f, 0.0f, -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f,
    1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
    1.0f, 1.0f,  0.0f,  -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, 1.0f, -1.0f};
const float vehCheckSolidOffsets[VEH_SOLID_SWEEP_COUNT][3] = {{1.2f, -1.0f, 0.0f},  {1.3f, 0.0f, 0.0f},   {1.2f, 1.0f, 0.0f},
                                                              {-1.25f, 1.0f, 0.0f}, {-1.35f, 0.0f, 0.0f}, {-1.25f, -1.0f, 0.0f}};

int16_t vehPathNodeCount;
vehicle_path_node_t vehPathNodes[VEH_MAX_PATH_NODES];
int vehPathDebugDrawing;

#define VEHICLE_PARSE_FIELD(key, member, fieldType) {key, (int32_t)offsetof(vehicleInfo_t, member), fieldType}

static const parseField_t vehicleParseFields[] = {
    VEHICLE_PARSE_FIELD("type", type, VEH_PARSE_FIELD_TYPE_VEHICLE),
    VEHICLE_PARSE_FIELD("steerWheels", steerWheels, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("texureScroll", textureScroll, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("quadBarrel", primaryDualFlash, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("bulletDamage", bulletDamageEnabled, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("grenadeDamage", grenadeDamageEnabled, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("projectileDamage", explosiveDamageEnabled, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("texureScrollScale", textureScrollScale, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("maxSpeed", maxSpeed, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("accel", acceleration, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("rotRate", steeringLimit, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("rotAccel", steeringRate, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("collisionDamage", collisionDamageScale, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("collisionSpeed", pathSpeed, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("suspensionTravel", suspensionTravel, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("maxBodyPitch", forwardInputScale, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("maxBodyRoll", verticalInputScale, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("hasGunner", gunnerSeatEnabled, PARSE_FIELD_BOOL),
    VEHICLE_PARSE_FIELD("numPassengers", extraPassengerCount, PARSE_FIELD_INT),
    VEHICLE_PARSE_FIELD("turretWeapon", turretWeapon, PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("turretAltWeapon", turretAltWeapon, PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("turretHorizSpanLeft", primaryYawLimitPos, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretHorizSpanRight", primaryYawLimitNeg, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretVertSpanUp", primaryPitchLimitNeg, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretVertSpanDown", primaryPitchLimitPos, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretRotRate", primaryTurnRate, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretGunnerWeapon", turretGunnerWeapon, PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("turretGunnerHorizSpanLeft", gunnerYawLimitPos, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretGunnerHorizSpanRight", gunnerYawLimitNeg, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretGunnerVertSpanUp", gunnerPitchLimitNeg, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretGunnerVertSpanDown", gunnerPitchLimitPos, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("turretGunnerRotRate", gunnerTurnRate, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("lowIdleSnd", soundNames[0], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("highIdleSnd", soundNames[1], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("lowEngineSnd", soundNames[2], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("highEngineSnd", soundNames[3], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("turretSpinSnd", soundNames[4], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("turretStopSnd", soundNames[5], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("impactSnd", soundNames[6], PARSE_FIELD_STRING_ALLOC),
    VEHICLE_PARSE_FIELD("engineSndSpeed", pathSpeedDenom, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("dmgScaleFront", damageScaleFront, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("dmgScaleSide", damageScaleSide, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("dmgScaleBehind", damageScaleRear, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("dmgScaleUnder", damageScaleTop, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("dmgScaleBullet", damageScaleBullet, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("minsX", collisionBoundsSource[0], PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("minsY", collisionBoundsSource[1], PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("minsZ", collisionBoundsSource[2], PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("maxsX", collisionBoundsSource[3], PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("maxsY", collisionBoundsSource[4], PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("maxsZ", collisionBoundsSource[5], PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("accelFwdRollDegrees", rollInputScale, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("accelSideRollDegrees", steeringRollScale, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("accelSpringTension", rollLimit, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("stepsize", stepSize, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("collisionFrontDist", dismountForwardOffset, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("collisionBackDist", dismountBackOffset, PARSE_FIELD_FLOAT),
    VEHICLE_PARSE_FIELD("useHintString", hintString, PARSE_FIELD_STRING_ALLOC),
};

typedef char vehicle_parse_field_count_check[(sizeof(vehicleParseFields) / sizeof(vehicleParseFields[0]) == 58) ? 1 : -1];

const uint16_t s_numVehicleFields = 58;
const float kVehSafetyBuffer = 25.0f;

#undef VEHICLE_PARSE_FIELD
void G_VehFreePathPos(vehicle_path_position_t *pathPosition);
void G_VehSetUpPathPos(vehicle_path_position_t *pathPosition, int16_t nodeIndex);
void G_VehSetSwitchNode(vehicle_path_position_t *pathPosition, int16_t fromNodeIndex, int16_t toNodeIndex);
qboolean VEH_FindValidDismountSpot(gentity_t *vehicleEnt, const float *origin, const float *mins, const float *maxs, float *outOrigin,
                                   int passEntityNum);
int vehClientThinkRecursing;

void VEH_Strcpy(char *dest, const char *src);
qboolean VEH_ParseSpecificField(void *vehicleInfoBase, const char *value, int fieldType);
gentity_t *VEH_GetEntity(int entNum);
void VEH_SetupCollmap(gentity_t *ent);
int VEH_FindNextPassengerSlot(const vehicle_state_t *vehicleState, int currentSlot, qboolean reverse);
int VEH_GetPassengerTagBoneIndex(const vehicle_state_t *vehicleState, uint32_t passengerSlot);
int VEH_FindVehicleInfoIndex(const char *name);
void VEH_InitEntity(gentity_t *ent, vehicle_state_t *vehicleState, int16_t vehicleInfoIndex);
void VEH_InitVehicle(gentity_t *ent, vehicle_state_t *vehicleState, int16_t vehicleInfoIndex);
void Scr_Vehicle_Init(gentity_t *ent);
void Scr_Vehicle_Think(gentity_t *ent);
void Scr_Vehicle_Touch(gentity_t *self, gentity_t *other, int traceMode);
void Scr_Vehicle_Use(gentity_t *self, gentity_t *other, gentity_t *activator);
int G_GetTankIndex(int entNum);
void VEH_UpdateScriptedInput(gentity_t *ent, const vec3_t inputVector, float scale, float speedScale, float acceleration);
void VEH_UpdateSuspension(gentity_t *ent, qboolean livePhysicsPass);
void VEH_Backup(gentity_t *ent);
void VEH_UpdatePath(gentity_t *ent);
void VEH_UpdateAim(gentity_t *ent);
void VEH_UpdateGunnerAim(gentity_t *ent);
void VEH_UpdateBody(gentity_t *ent);
void VEH_CalcAccel(gentity_t *ent, const int8_t input[3], vec3_t linearAccel, vec3_t angularAccel);
void VEH_UpdateClient(gentity_t *ent);
#if !EMULATE_X87 && !defined(__x86_64__)
static int game_compat_veh_round_to_int(float value);
#endif
qboolean VEH_PhysicsNotRequired(gentity_t *ent, qboolean checkNearbyClients);
qboolean VEH_VerifyPosition(gentity_t *ent, int passNumber);
void VEH_GroundTrace(gentity_t *ent);
void VEH_GroundFriction(gentity_t *ent);
void VEH_GroundMove(gentity_t *ent);
void VEH_AirMove(gentity_t *ent);
void VEH_UpdateWeapon(gentity_t *ent);
void VEH_UpdateGunnerWeapon(gentity_t *ent);
void VEH_UpdateSteering(gentity_t *ent);
void VEH_UpdateShaderTime(gentity_t *ent);
void CMD_VEH_AttachPath(uint32_t scriptObject);
void CMD_VEH_StartPath(uint32_t scriptObject);
void CMD_VEH_SetSwitchNode(uint32_t scriptObject);
void CMD_VEH_SetWaitNode(uint32_t scriptObject);
void CMD_VEH_SetWaitSpeed(uint32_t scriptObject);
void CMD_VEH_SetSpeed(uint32_t scriptObject);
void CMD_VEH_ResumeSpeed(uint32_t scriptObject);
void CMD_VEH_JoltBody(uint32_t scriptObject);
void CMD_VEH_FreeVehicle(uint32_t scriptObject);
void CMD_VEH_GetWheelSurface(uint32_t scriptObject);
void CMD_VEH_GetSpeedMPH(uint32_t scriptObject);
void CMD_VEH_GetVehicleOwner(uint32_t scriptObject);
void CMD_VEH_StartEngineSound(uint32_t scriptObject);
void CMD_VEH_StopEngineSound(uint32_t scriptObject);
void CMD_VEH_MakeVehicleUsable(uint32_t scriptObject);
void CMD_VEH_MakeVehicleUnusable(uint32_t scriptObject);
void CMD_VEH_AddVehicleToCompass(uint32_t scriptObject);
void CMD_VEH_RemoveVehicleFromCompass(uint32_t scriptObject);
void CMD_VEH_SetTurretTargetVec(uint32_t scriptObject);
void CMD_VEH_SetTurretTargetEnt(uint32_t scriptObject);
void CMD_VEH_ClearTurretTarget(uint32_t scriptObject);
void CMD_VEH_FireTurret(uint32_t scriptObject);
void CMD_VEH_FireAltTurret(uint32_t scriptObject);
void G_VEH_FireGunner(uint32_t scriptObject, qboolean ignoreReady);
void CMD_VEH_FireGunner(uint32_t scriptObject);
void CMD_VEH_IsTurretReady(uint32_t scriptObject);
void CMD_VEH_GetFireTime(uint32_t scriptObject);
void CMD_VEH_GetAltHeat(uint32_t scriptObject);
void CMD_VEH_GetGunnerHeat(uint32_t scriptObject);
void CMD_VEH_GetAltOverheating(uint32_t scriptObject);
void CMD_VEH_GetGunnerOverheating(uint32_t scriptObject);
void CMD_VEH_GetDismountSpot(uint32_t scriptObject);

static const char *const vehicleTypeNames[VEHICLE_TYPE_COUNT] = {"** unknown **", "4 wheel", "tank", "plane", "boat", "artillery"};

static const char *const vehicleWheelTagNames[VEH_WHEEL_TAG_COUNT] = {"tag_wheel_front_left",  "tag_wheel_front_right",
                                                                      "tag_wheel_back_left",   "tag_wheel_back_right",
                                                                      "tag_wheel_middle_left", "tag_wheel_middle_right"};

static const script_method_t scriptVehicleMethods[] = {{"attachpath", CMD_VEH_AttachPath},
                                                       {"startpath", CMD_VEH_StartPath},
                                                       {"setswitchnode", CMD_VEH_SetSwitchNode},
                                                       {"setwaitnode", CMD_VEH_SetWaitNode},
                                                       {"setwaitspeed", CMD_VEH_SetWaitSpeed},
                                                       {"setspeed", CMD_VEH_SetSpeed},
                                                       {"resumespeed", CMD_VEH_ResumeSpeed},
                                                       {"joltbody", CMD_VEH_JoltBody},
                                                       {"freevehicle", CMD_VEH_FreeVehicle},
                                                       {"getwheelsurface", CMD_VEH_GetWheelSurface},
                                                       {"getspeedmph", CMD_VEH_GetSpeedMPH},
                                                       {"getvehicleowner", CMD_VEH_GetVehicleOwner},
                                                       {"startenginesound", CMD_VEH_StartEngineSound},
                                                       {"stopenginesound", CMD_VEH_StopEngineSound},
                                                       {"makevehicleusable", CMD_VEH_MakeVehicleUsable},
                                                       {"makevehicleunusable", CMD_VEH_MakeVehicleUnusable},
                                                       {"addvehicletocompass", CMD_VEH_AddVehicleToCompass},
                                                       {"removevehiclefromcompass", CMD_VEH_RemoveVehicleFromCompass},
                                                       {"setturrettargetvec", CMD_VEH_SetTurretTargetVec},
                                                       {"setturrettargetent", CMD_VEH_SetTurretTargetEnt},
                                                       {"clearturrettarget", CMD_VEH_ClearTurretTarget},
                                                       {"fireturret", CMD_VEH_FireTurret},
                                                       {"firealtturret", CMD_VEH_FireAltTurret},
                                                       {"firegunner", CMD_VEH_FireGunner},
                                                       {"isturretready", CMD_VEH_IsTurretReady},
                                                       {"get_fire_time", CMD_VEH_GetFireTime},
                                                       {"getaltheat", CMD_VEH_GetAltHeat},
                                                       {"getgunnerheat", CMD_VEH_GetGunnerHeat},
                                                       {"getaltoverheating", CMD_VEH_GetAltOverheating},
                                                       {"getgunneroverheating", CMD_VEH_GetGunnerOverheating},
                                                       {"getdismountspot", CMD_VEH_GetDismountSpot}};

static const char *const vehiclePrimaryFlashTagNames[VEH_TAG_FLASH_COUNT] = {"tag_flash", "tag_flash_11", "tag_flash_2", "tag_flash_22"};

static const char *const vehicleAltFireTagNames[VEH_TAG_FLASH_COUNT] = {"tag_altfire", "tag_altfire_11", "tag_altfire_2", "tag_altfire_22"};

static const char *const vehicleSecondaryFlashTagNames[VEH_TAG_FLASH_COUNT] = {"tag_secondary_flash", "tag_secondary_flash_11",
                                                                               "tag_secondary_flash_2", "tag_secondary_flash_22"};

static trace_t vehLastGroundTrace;  /* DAT_001035e0 */
static qboolean vehGroundTraceHit;          /* DAT_00103610 */
static qboolean vehGroundTraceWalkable;     /* DAT_00103614 */
static vehicle_path_position_t vehPhysicsSnapshotPrefix; /* DAT_00103620 */
static vehicle_physics_state_t vehPhysicsSnapshotState;  /* DAT_001036d8 */
static qboolean vehDebugPathSegmentNeedsInit = qtrue; /* DAT_000c3388 */
static vec3_t vehDebugPathSegmentStart;             /* DAT_00146624 */
static vec3_t vehDebugPathSegmentEnd;               /* DAT_00146630 */
static vec3_t vehDebugPathSegmentDir;               /* DAT_0014663c */

static const vehicle_node_spawn_field_t vehicleNodeSpawnFields[] = {{"targetname", offsetof(vehicle_path_node_t, targetname), 5},
                                                                    {"target", offsetof(vehicle_path_node_t, target), 5},
                                                                    {"origin", offsetof(vehicle_path_node_t, origin), 6},
                                                                    {"angles", offsetof(vehicle_path_node_t, angles), 6},
                                                                    {"speed", offsetof(vehicle_path_node_t, speed), 3},
                                                                    {"lookahead", offsetof(vehicle_path_node_t, lookAhead), 3},
                                                                    {0, 0, 0}};

GAME_STATIC_ASSERT(vehicle_path_node_size, sizeof(vehicle_path_node_t) == 0x40u);
GAME_STATIC_ASSERT(vehicle_path_node_origin_offset, offsetof(vehicle_path_node_t, origin) == 0x14);
GAME_STATIC_ASSERT(vehicle_path_node_angles_offset, offsetof(vehicle_path_node_t, angles) == 0x2c);
GAME_STATIC_ASSERT(vehicle_path_node_use_node_angles_offset, offsetof(vehicle_path_node_t, useNodeAngles) == 0x08);
GAME_STATIC_ASSERT(vehicle_path_node_speed_offset, offsetof(vehicle_path_node_t, speed) == 0x0c);
GAME_STATIC_ASSERT(vehicle_path_node_look_ahead_offset, offsetof(vehicle_path_node_t, lookAhead) == 0x10);
GAME_STATIC_ASSERT(vehicle_path_node_next_offset, offsetof(vehicle_path_node_t, nextNodeIndex) == 0x3c);
GAME_STATIC_ASSERT(vehicle_path_node_previous_offset, offsetof(vehicle_path_node_t, previousNodeIndex) == 0x3e);
GAME_STATIC_ASSERT(vehicle_path_position_current_angles_offset, offsetof(vehicle_path_position_t, currentAngles) == 0x20);
GAME_STATIC_ASSERT(vehicle_path_position_look_ahead_origin_offset, offsetof(vehicle_path_position_t, lookAheadOrigin) == 0x2c);
GAME_STATIC_ASSERT(vehicle_path_position_target_node_offset, offsetof(vehicle_path_position_t, targetNode) == 0x38);
GAME_STATIC_ASSERT(vehicle_path_position_cached_node_offset, offsetof(vehicle_path_position_t, cachedNode) == 0x78);
GAME_STATIC_ASSERT(vehicle_path_position_size, sizeof(vehicle_path_position_t) == 0xb8);
/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static const vehicleInfo_t *game_compat_veh_get_vehicle_info(int index)
{
    return &g_vehicleInfoTable[index];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint16_t game_compat_veh_entity_model_classname(const gentity_t *ent)
{
    return ent->scriptClassname;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint16_t *game_compat_veh_entity_model_classname_slot(gentity_t *ent)
{
    return &ent->scriptClassname;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float *game_compat_veh_physics_client_origin(vehicle_state_t *clientPhysics)
{
    return clientPhysics->origin;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_vector3_is_non_zero(const vec3_t value)
{
    return value[0] != 0.0f || value[1] != 0.0f || value[2] != 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_project_velocity_to_local_axis(const vehicle_state_t *vehicleState, axis_t axis, vec3_t localVelocity)
{
    for (int axisIndex = 0; axisIndex < 3; axisIndex++) {
#if EMULATE_X87
        localVelocity[axisIndex] =
            x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(axis[axisIndex][0]), x87f_load_f32(vehicleState->velocity[0])),
                                             x87f_mul(x87f_load_f32(axis[axisIndex][1]), x87f_load_f32(vehicleState->velocity[1]))),
                                    x87f_mul(x87f_load_f32(axis[axisIndex][2]), x87f_load_f32(vehicleState->velocity[2]))));
#else
        localVelocity[axisIndex] = axis[axisIndex][0] * vehicleState->velocity[0] + axis[axisIndex][1] * vehicleState->velocity[1] +
                                   axis[axisIndex][2] * vehicleState->velocity[2];
#endif
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_add_scaled_axis(vec3_t velocity, axis_t axis, int axisIndex, float scale)
{
#if EMULATE_X87
    for (int i = 0; i < 3; i++) {
        velocity[i] =
            x87f_store_f32(x87f_add(x87f_load_f32(velocity[i]), x87f_mul(x87f_load_f32(axis[axisIndex][i]), x87f_load_f32(scale))));
    }
#else
    velocity[0] += axis[axisIndex][0] * scale;
    velocity[1] += axis[axisIndex][1] * scale;
    velocity[2] += axis[axisIndex][2] * scale;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_float_changed(float current, float desired)
{
    return current != desired || isnan(current) || isnan(desired);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_vector_changed(const vec3_t current, const vec3_t desired)
{
    return game_compat_veh_float_changed(current[0], desired[0]) || game_compat_veh_float_changed(current[1], desired[1]) ||
           game_compat_veh_float_changed(current[2], desired[2]);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_entity_current_speed(const gentity_t *ent)
{
    float value;

    memcpy(&value, &ent->maxSpeed, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_entity_current_speed(gentity_t *ent, float value)
{
    memcpy(&ent->maxSpeed, &value, sizeof(value));
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_entity_primary_yaw(const gentity_t *ent)
{
    return ent->s.vehicleTurret.primaryYaw;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_entity_primary_yaw(gentity_t *ent, float value)
{
    ent->s.vehicleTurret.primaryYaw = value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_entity_gunner_yaw_base(const gentity_t *ent)
{
    return ent->s.loopedFxForward[1];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_entity_gunner_yaw_base(gentity_t *ent, float value)
{
    ent->s.loopedFxForward[1] = value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint32_t game_compat_veh_entity_passenger_bits(const gentity_t *ent)
{
    return (uint32_t)ent->s.turretOverheatState;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_entity_passenger_bits(gentity_t *ent, uint32_t value)
{
    ent->s.turretOverheatState = (int32_t)value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void __attribute__((unused)) game_compat_veh_register_weapon_if_present(const char *vehicleName, const char weaponName[0x040])
{
    int weaponIndex;

    if (weaponName[0] == '\0') {
        return;
    }

    weaponIndex = BG_GetWeaponIndexForName(weaponName) & 0xff;
    if (!IsItemRegistered(weaponIndex)) {
        Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_NOT_PRECACHED, vehicleName));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_precache_weapon_if_present(const char weaponName[0x040])
{
    int weaponIndex;

    if (weaponName[0] != '\0') {
        weaponIndex = BG_GetWeaponIndexForName(weaponName) & 0xff;
        if (weaponIndex != 0) {
            RegisterItem(weaponIndex, qtrue);
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_clamp_float(float value, float minValue, float maxValue)
{
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_clamp_abs(float value, float maxAbs)
{
    return game_compat_veh_clamp_float(value, -maxAbs, maxAbs);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
/* Every inline original rounds the squared sum to DOUBLE and calls CoduoLibm_Sqrt()
 * (e.g. 0x7da9b/0x7dbad: fstp QWORD; call sqrt), never sqrtf. */
static float game_compat_veh_length3(const vec3_t value)
{
#if EMULATE_X87
    return (float)CoduoLibm_Sqrt(x87f_store_f64(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(value[0]), x87f_load_f32(value[0])), x87f_mul(x87f_load_f32(value[1]), x87f_load_f32(value[1]))),
        x87f_mul(x87f_load_f32(value[2]), x87f_load_f32(value[2])))));
#else
    return (float)CoduoLibm_Sqrt((double)((long double)value[0] * (long double)value[0] + (long double)value[1] * (long double)value[1] +
                                          (long double)value[2] * (long double)value[2]));
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_float_is_non_zero_or_nan(float value)
{
    return value != 0.0f || isnan(value);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int game_compat_veh_abs_int(int value)
{
    return value < 0 ? -value : value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_approach_float_step(float current, float target, float step)
{
    if (current < target) {
        current += step;
        if (current > target) {
            current = target;
        }
    } else if (target < current) {
        current -= step;
        if (current < target) {
            current = target;
        }
    }

    return current;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
/* Retired from the VEH_CalcAccel sites: the inline originals compare the
 * unrounded 80-bit factor and round the pass-through arm stepwise (see the
 * cascades at 0x7dca9/0x7dfa8/0x7e20d/0x7e9a4), which this helper cannot
 * reproduce. Kept for reference. */
static float __attribute__((unused)) VEH_SpeedScaledFactor(float speed, float maxSpeed, float baseFactor, float rangeFactor)
{
    if (speed < maxSpeed) {
        return game_compat_veh_clamp_float((1.0f - speed / maxSpeed) * rangeFactor + baseFactor, 0.0f, 1.0f);
    }

    return baseFactor;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int32_t game_compat_veh_float_slot_int_bits(const float *slot)
{
    int32_t value;

    memcpy(&value, slot, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_float_slot_int_bits(float *slot, int32_t value)
{
    memcpy(slot, &value, sizeof(value));
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_compass_visible(vehicle_state_t *vehicleState, int32_t value)
{
    game_compat_veh_set_float_slot_int_bits(&vehicleState->turretRoll, value);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_engine_sound_active(vehicle_state_t *vehicleState, int32_t value)
{
    vehicleState->suspensionEnabled = value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_turret_target_active(vehicle_state_t *vehicleState, int32_t value)
{
    game_compat_veh_set_float_slot_int_bits(&vehicleState->viewState[6], value);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int32_t game_compat_veh_turret_muzzle_back_active(const vehicle_state_t *vehicleState)
{
    return game_compat_veh_float_slot_int_bits(&vehicleState->viewState[1]);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_turret_muzzle_back_active(vehicle_state_t *vehicleState, int32_t value)
{
    game_compat_veh_set_float_slot_int_bits(&vehicleState->viewState[1], value);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint16_t game_compat_veh_script_notify_string(const char *value)
{
    return SL_GetString(value, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_dismount_side_offset(const vehicleInfo_t *vehicleInfo)
{
    return vehicleInfo->collisionBoundsSource[3];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int32_t game_compat_veh_turret_activity_state(const vehicle_state_t *vehicleState, vehicle_turret_activity_slot_t slot)
{
    if (slot == VEH_TURRET_ACTIVITY_PRIMARY) {
        return game_compat_veh_float_slot_int_bits(&vehicleState->turretYaw);
    }

    return vehicleState->gunnerTurretState;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_turret_activity_state(vehicle_state_t *vehicleState, vehicle_turret_activity_slot_t slot, int32_t value)
{
    if (slot == VEH_TURRET_ACTIVITY_PRIMARY) {
        game_compat_veh_set_float_slot_int_bits(&vehicleState->turretYaw, value);
    } else {
        vehicleState->gunnerTurretState = value;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int32_t game_compat_veh_path_speed_mode(const vehicle_state_t *vehicleState)
{
    return game_compat_veh_float_slot_int_bits(&vehicleState->viewState[2]);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_path_speed_mode(vehicle_state_t *vehicleState, int32_t mode)
{
    game_compat_veh_set_float_slot_int_bits(&vehicleState->viewState[2], mode);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float *game_compat_veh_path_cursor_fraction_slot(vehicle_state_t *vehicleState)
{
    return &vehicleState->viewState[5];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_clear_vehicle_velocity(vehicle_state_t *vehicleState)
{
    vehicleState->velocity[0] = 0.0f;
    vehicleState->velocity[1] = 0.0f;
    vehicleState->velocity[2] = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint8_t game_compat_veh_entity_collision_mode(const gentity_t *ent)
{
    return ent->activeState;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_entity_collision_mode(gentity_t *ent, uint8_t collisionMode)
{
    ent->activeState = collisionMode;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_entity_usable_byte(gentity_t *ent, uint8_t usable)
{
    ent->takeDamage = usable;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int32_t game_compat_veh_wheel_surface_at(const vehicle_state_t *vehicleState, uint32_t wheelIndex)
{
    return game_compat_veh_float_slot_int_bits(&vehicleState->wheelMaterial[wheelIndex]);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_set_wheel_surface_at(vehicle_state_t *vehicleState, uint32_t wheelIndex, int32_t surfaceType)
{
    game_compat_veh_set_float_slot_int_bits(&vehicleState->wheelMaterial[wheelIndex], surfaceType);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
#if !EMULATE_X87 && !defined(__x86_64__)
static int game_compat_veh_round_to_int(float value)
{
    /* The original converts an already-rounded binary32 value. */
    return (int)value;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_non_pvs_tank_delta_scale(int delta)
{
    if (delta > VEH_NONPVS_TANK_ENCODE_MAX) {
        /* single float/float divide: x87 forms the 80-bit quotient and rounds
         * once at the float return store, where SSE divides in 32 bits -> shim */
#if EMULATE_X87
        return x87f_store_f32(x87f_div(x87f_load_f32(VEH_NONPVS_TANK_ORIGIN_LIMIT_POS), x87f_load_f32((float)delta)));
#else
        return VEH_NONPVS_TANK_ORIGIN_LIMIT_POS / (float)delta;
#endif
    }
    if (delta < VEH_NONPVS_TANK_ENCODE_MIN) {
#if EMULATE_X87
        return x87f_store_f32(x87f_div(x87f_load_f32(VEH_NONPVS_TANK_ORIGIN_LIMIT_NEG), x87f_load_f32((float)delta)));
#else
        return VEH_NONPVS_TANK_ORIGIN_LIMIT_NEG / (float)delta;
#endif
    }

    return 1.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int game_compat_veh_clamp_non_pvs_tank_delta(int delta)
{
    if (delta > VEH_NONPVS_TANK_ENCODE_MAX) {
        return VEH_NONPVS_TANK_ENCODE_MAX;
    }
    if (delta < VEH_NONPVS_TANK_ENCODE_MIN) {
        return VEH_NONPVS_TANK_ENCODE_MIN;
    }

    return delta;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint32_t game_compat_veh_pack_non_pvs_tank_coord(int value)
{
    int biased = value + 2;

    if (biased < 0) {
        biased = value + 5;
    }

    return (uint32_t)(((biased >> 2) + (int)VEH_NONPVS_TANK_COORD_BIAS) & (int)VEH_NONPVS_TANK_COORD_MASK);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static vehicle_state_t *game_compat_veh_scr_vehicle_state_at(int index)
{
    return &vehClientPhysicsRecords[index];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_scr_vehicle_slot_in_use(int index)
{
    return game_compat_veh_scr_vehicle_state_at(index)->entityNum != ENTITYNUM_NONE;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static vehicle_path_position_t *game_compat_veh_path_cursor(vehicle_state_t *vehicleState)
{
    return &vehicleState->pathPosition;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static vehicle_path_node_t *game_compat_veh_path_node_at(int index)
{
    return &vehPathNodes[index];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_path_vector_is_zero(const vec3_t value)
{
    return value[0] == 0.0f && value[1] == 0.0f && value[2] == 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_path_node_has_angles(const vehicle_path_node_t *node)
{
    return !game_compat_veh_path_vector_is_zero(node->angles);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint32_t game_compat_veh_player_blend_anim(const clientInfo_t *clientInfo)
{
    const uint32_t animTreeIndex = (uint32_t)Scr_GetAnimsIndex(bgAnimStaticTable->animTreeHandle);

    /* The four original branches all clear the animation restart toggle from
     * this packed tree/index value: Linux 0x90602/0x90b8e/0x90f0a/0x9129c,
     * Windows 0x2004c9b5/0x2004cc70/0x2004ce29/0x2004cfd1, and Mac
     * 0x59248/0x594d8/0x59688/0x5983c. */
    return ((animTreeIndex << SCR_ANIM_TREE_INDEX_SHIFT) |
            (game_compat_bg_anim_slot_animation_word_from_slot(&clientInfo->legsYawAngle) & 0xffffu)) &
           ~ANIM_TOGGLEBIT;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_player_blend_anim_slot_enabled(const clientInfo_t *clientInfo)
{
    const uint32_t animSlotWord = game_compat_bg_anim_slot_animation_word_from_slot(&clientInfo->legsYawAngle);
    const uint32_t animSlotAnimationReference = game_compat_bg_anim_slot_animation_reference_from_slot(&clientInfo->legsYawAngle);

    if (animSlotWord == 0 || animSlotAnimationReference == 0) {
        return qfalse;
    }

    return (game_compat_bg_static_animation_flags_from_reference(bgAnimStaticTable, animSlotAnimationReference) & BG_ANIM_ENTRY_TURRET) !=
           0;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
/* targetWeight is long double so caller expressions (e.g. 1.0f - leanWeight)
 * reach the weight-delta subtraction unrounded, as in the inline original
 * (0x908b5: fsub feeds fsubp with no intermediate float store). */
static float game_compat_veh_player_blend_goal_time(XAnimTree *animTree, uint32_t anim, long double targetWeight)
{
    /* 0x908c3/0x908c5: fabs result is stored to a float slot BEFORE the
     * rate-scale multiply, which is a second rounded store. */
    const float weightDelta = fabsf((float)(trap_XAnimGetWeight(animTree, anim) - targetWeight));
    const float delta = weightDelta * VEH_PLAYER_BLEND_RATE_SCALE;

    if (delta > 0.0f) {
        return 1.0f / delta;
    }

    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_player_blend_set_weight(XAnimTree *animTree, uint32_t anim, long double targetWeight)
{
    trap_XAnimSetGoalWeight(animTree, anim, (float)targetWeight, game_compat_veh_player_blend_goal_time(animTree, anim, targetWeight), 1.0f,
                            0, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_player_blend_get_middle_child(uint32_t parentAnim, uint32_t *outChild)
{
    const int childCount = trap_XAnimGetNumChildren(parentAnim);

    if (childCount == 0) {
        Com_Error(1, VEH_PLAYER_BLEND_ANIM_CHILD_ERROR, trap_XAnimGetAnimName(parentAnim));
    }

    trap_XAnimGetChildAt(outChild, parentAnim, childCount / 2);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint32_t game_compat_veh_player_blend_first_child(uint32_t parentAnim)
{
    uint32_t childAnim;

    trap_XAnimGetChildAt(&childAnim, parentAnim, 0);
    return childAnim;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint32_t game_compat_veh_player_blend_last_child(uint32_t parentAnim)
{
    uint32_t childAnim;
    const int childCount = trap_XAnimGetNumChildren(parentAnim);

    trap_XAnimGetChildAt(&childAnim, parentAnim, childCount - 1);
    return childAnim;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_player_blend_resolve_anims(uint32_t rootAnim, uint32_t *topAnim, uint32_t *centerAnim)
{
    game_compat_veh_player_blend_get_middle_child(rootAnim, topAnim);
    game_compat_veh_player_blend_get_middle_child(*topAnim, centerAnim);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_player_blend_set_centered(XAnimTree *animTree, uint32_t rootAnim, uint32_t topAnim, uint32_t centerAnim)
{
    trap_XAnimSetGoalWeight(animTree, centerAnim, 1.0f, 1.0f, 1.0f, 0, 0);
    trap_XAnimClearTreeGoalWeightsStrict(animTree, rootAnim, 0.0f);
    game_compat_veh_player_blend_set_weight(animTree, centerAnim, 1.0f);
    trap_XAnimSetGoalWeight(animTree, topAnim, 1.0f, 1.0f, 1.0f, 0, 0);
    game_compat_veh_player_blend_set_weight(animTree, topAnim, 1.0f);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_player_blend_update_anims(gentity_t *player, gentity_t *vehicleEnt, clientInfo_t *clientInfo)
{
    const int vehiclePosition = player->client->ps.vehiclePosition;
    const int vehicleAnimType = vehicleEnt->s.vehicleType;
    XAnimTree *animTree = clientInfo->animTree;
    const uint32_t rootAnim = game_compat_veh_player_blend_anim(clientInfo);
    uint32_t topAnim;
    uint32_t centerAnim;

    trap_XAnimClearTreeGoalWeightsStrict(animTree, rootAnim, 0.0f);
    game_compat_veh_player_blend_resolve_anims(rootAnim, &topAnim, &centerAnim);

    if ((vehiclePosition == 1 || vehiclePosition == 3) && vehicleAnimType == 1) {
        uint32_t leanAnim;
        float leanWeight;
        const float vehicleLeanYaw = vehicleEnt->s.clientInfoLeanFraction;

        if (vehicleLeanYaw > 0.0f) {
            /* single float/float divide -> shim; fabsf is an exact sign clear */
#if EMULATE_X87
            leanWeight = game_compat_veh_clamp_float(
                x87f_store_f32(x87f_div(x87f_load_f32(vehicleLeanYaw), x87f_load_f32(VEH_PLAYER_BLEND_LEAN_YAW_LIMIT))), 0.0f, 1.0f);
#else
            leanWeight = game_compat_veh_clamp_float(vehicleLeanYaw / VEH_PLAYER_BLEND_LEAN_YAW_LIMIT, 0.0f, 1.0f);
#endif
            leanAnim = game_compat_veh_player_blend_last_child(topAnim);
        } else if (vehicleLeanYaw < 0.0f) {
#if EMULATE_X87
            leanWeight = game_compat_veh_clamp_float(
                x87f_store_f32(x87f_div(x87f_load_f32(fabsf(vehicleLeanYaw)), x87f_load_f32(VEH_PLAYER_BLEND_LEAN_YAW_LIMIT))), 0.0f, 1.0f);
#else
            leanWeight = game_compat_veh_clamp_float(fabsf(vehicleLeanYaw) / VEH_PLAYER_BLEND_LEAN_YAW_LIMIT, 0.0f, 1.0f);
#endif
            leanAnim = game_compat_veh_player_blend_first_child(topAnim);
        } else {
            leanWeight = 0.0f;
            leanAnim = game_compat_veh_player_blend_first_child(topAnim);
        }

        trap_XAnimClearTreeGoalWeightsStrict(animTree, rootAnim, 0.0f);
        game_compat_veh_player_blend_set_weight(animTree, centerAnim, 1.0f - leanWeight);
        if (leanWeight != 0.0f || isnan(leanWeight)) {
            game_compat_veh_player_blend_set_weight(animTree, leanAnim, leanWeight);
        }
        trap_XAnimSetGoalWeight(animTree, topAnim, 1.0f, 1.0f, 1.0f, 0, 0);
        game_compat_veh_player_blend_set_weight(animTree, topAnim, 1.0f);
    } else if ((vehiclePosition == 2 && vehicleAnimType == 1) || vehicleAnimType == 5 || (vehicleAnimType == 2 && vehiclePosition == 2)) {
        game_compat_veh_player_blend_set_centered(animTree, rootAnim, topAnim, centerAnim);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_path_node_copy_angles(const vehicle_path_node_t *node, vec3_t outAngles)
{
    outAngles[0] = node->angles[0];
    outAngles[1] = node->angles[1];
    outAngles[2] = node->angles[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_path_clear_vector(vec3_t value)
{
    value[0] = 0.0f;
    value[1] = 0.0f;
    value[2] = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_path_position_speed_at_node(const vehicle_path_position_t *position)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(position->nodeIndex);

    if (node->nextNodeIndex < 0) {
        return node->speed;
    }

    return node->speed + (game_compat_veh_path_node_at(node->nextNodeIndex)->speed - node->speed) * position->fraction;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_path_position_look_ahead_at_node(const vehicle_path_position_t *position)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(position->nodeIndex);

    if (node->nextNodeIndex < 0) {
        return node->lookAhead;
    }

    return node->lookAhead + (game_compat_veh_path_node_at(node->nextNodeIndex)->lookAhead - node->lookAhead) * position->fraction;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_path_position_curve_fraction(const vehicle_path_position_t *position)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(position->nodeIndex);

    if (node->nextNodeIndex < 0) {
        return node->useNodeAngles != 0 ? 1.0f : 0.0f;
    }

    if (node->useNodeAngles != 0 && game_compat_veh_path_node_at(node->nextNodeIndex)->useNodeAngles != 0) {
        return 1.0f;
    }
    if (node->useNodeAngles == 0 && game_compat_veh_path_node_at(node->nextNodeIndex)->useNodeAngles != 0) {
        return position->fraction;
    }
    if (node->useNodeAngles != 0 && game_compat_veh_path_node_at(node->nextNodeIndex)->useNodeAngles == 0) {
        return 1.0f - position->fraction;
    }

    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_path_lerp_angles(const vec3_t from, const vec3_t to, float fraction, vec3_t outAngles)
{
    for (int axis = 0; axis < 3; axis++) {
        outAngles[axis] = LerpAngle(from[axis], to[axis], fraction);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_advance_turret_state(vehicle_state_t *vehicleState, vehicle_turret_activity_slot_t slot)
{
    const int state = game_compat_veh_turret_activity_state(vehicleState, slot);

    if (state == VEH_TURRET_STATE_ACTIVE) {
        game_compat_veh_set_turret_activity_state(vehicleState, slot, VEH_TURRET_STATE_WINDDOWN);
    } else if (state == VEH_TURRET_STATE_WINDDOWN) {
        game_compat_veh_set_turret_activity_state(vehicleState, slot, VEH_TURRET_STATE_INACTIVE);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float game_compat_veh_clamp_float_symmetric(float value, float negativeLimit, float positiveLimit)
{
    if (value < -negativeLimit) {
        return -negativeLimit;
    }
    if (positiveLimit < value) {
        return positiveLimit;
    }
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_build_passenger_aim_angles(const gentity_t *passenger, vec3_t outAngles)
{
    outAngles[0] = passenger->client->ps.viewAngles[0];
    outAngles[1] = passenger->client->ps.viewAngles[1];
    outAngles[2] = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_build_vehicle_local_angles(const vehicle_state_t *vehicleState, vec3_t outAngles)
{
    outAngles[0] = vehicleState->viewClampTargetAngles[0] + vehicleState->externalVelocity[0];
    outAngles[1] = vehicleState->viewClampTargetAngles[1] + vehicleState->externalVelocity[1];
    outAngles[2] = vehicleState->viewClampTargetAngles[2] + vehicleState->externalVelocity[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_aim_angles_in_vehicle_space(const vec3_t passengerAngles, const vec3_t vehicleAngles, vec3_t outAngles)
{
    axis_t passengerAxis;
    axis_t vehicleAxis;
    axis_t inverseVehicleAxis;
    axis_t localAxis;

    AnglesToAxis(passengerAngles, passengerAxis);
    AnglesToAxis(vehicleAngles, vehicleAxis);
    /* C99 multidimensional-array qualifier bridges for read-only math input. */
    MatrixTranspose((const vec_t(*)[3])vehicleAxis, inverseVehicleAxis);
    MatrixMultiply(passengerAxis, inverseVehicleAxis, localAxis);
    AxisToAngles((const vec_t(*)[3])localAxis, outAngles);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_update_turret_activity(vehicle_state_t *vehicleState, vehicle_turret_activity_slot_t slot, float pitchDelta,
                                                   float yawDelta, float desiredPitch, float clampedPitch, float desiredYaw)
{
    vec3_t desiredAngles = {desiredPitch, 0.0f, 0.0f};
    const vec3_t clampedAngles = {clampedPitch, 0.0f, 0.0f};

    AnglesSubtract(desiredAngles, clampedAngles, desiredAngles);

    if ((pitchDelta >= VEH_TURRET_STALL_DELTA && desiredAngles[0] == 0.0f && !isnan(desiredAngles[0])) ||
        (yawDelta >= VEH_TURRET_STALL_DELTA && desiredYaw == 0.0f && !isnan(desiredYaw))) {
        game_compat_veh_set_turret_activity_state(vehicleState, slot, VEH_TURRET_STATE_ACTIVE);
    } else {
        game_compat_veh_advance_turret_state(vehicleState, slot);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static
#if !EMULATE_X87 && defined(__x86_64__)
    __attribute__((always_inline)) inline
#endif
    int
    game_compat_veh_motion_byte_from_axis(float value)
{
    /* 0x86864..0x8693c (both VEH_UpdateBody expansions): the original
     * multiplies by 127 and divides by 30 as separate operations (never the
     * folded 127/30 constant), keeps the chain in 80-bit registers through
     * the float 0/255 compares, rounds it to DOUBLE for floor(), and the
     * clamp arms are the doubles 0.0 / 255.0. */
    double scaled;

#if EMULATE_X87
    /* value*127/30 + 127 stays in x87 width across the 0/255 compares (per
     * 0x86864..0x8693c) and is rounded to double only for floor(). */
    x87f x = x87f_add(x87f_div(x87f_mul(x87f_load_f32(value), x87f_load_f32(VEH_MOTION_ANIM_CENTER)), x87f_load_f32(30.0f)),
                      x87f_load_f32(VEH_MOTION_ANIM_CENTER));
    if (x87f_lt(x87f_load_f32(0.0f), x)) {
        if (x87f_lt(x, x87f_load_f32(255.0f))) {
            scaled = x87f_store_f64(x);
        } else {
            scaled = 255.0;
        }
    } else {
        scaled = 0.0;
    }
#else
    if (value * VEH_MOTION_ANIM_CENTER / 30.0f + VEH_MOTION_ANIM_CENTER > 0.0f) {
        if (255.0f > value * VEH_MOTION_ANIM_CENTER / 30.0f + VEH_MOTION_ANIM_CENTER) {
            scaled = value * VEH_MOTION_ANIM_CENTER / 30.0f + VEH_MOTION_ANIM_CENTER;
        } else {
            scaled = 255.0;
        }
    } else {
        scaled = 0.0;
    }
#endif

#if EMULATE_X87
    return x87f_store_i32_trunc(x87f_load_f64(floor(scaled)));
#elif defined(__x86_64__)
    {
        const double rounded = floor(scaled);
        return CODUO_X87_TRUNCATE_I32((long double)rounded);
    }
#else
    return (int)floor(scaled);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static gentity_t *game_compat_veh_spawn_linked_sound_blend(gentity_t *vehicleEnt)
{
    gentity_t *soundBlend = G_SpawnSoundBlend();

    soundBlend->s.vehicleEntityNum = vehicleEnt->s.number;
    trap_LinkEntity(soundBlend);
    return soundBlend;
}

/* VERIFIED_DECOMPILER(0x87480, 97480_FUN_00097480.c, VERIFY-VEHICLE-SOUNDS-RETRACE-2026-06-18): DATAFLOW_VERIFIED - vehicle sound blend entity gates, primary turret clientSound/stop sound, alt/gunner weapon blend timers, stop aliases, and sound time decrements checked against current typed Ghidra retrace output. */
static void VEH_UpdateSounds(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const qboolean notAlive = ent->health < 1;
    gentity_t *soundBlend;

    ent->s.clientSound = 0;

    if (vehicleState->soundBlendEntityNums[0] != ENTITYNUM_NONE) {
        soundBlend = &g_entities[vehicleState->soundBlendEntityNums[0]];
        if (notAlive || vehicleState->suspensionEnabled == 0 ||
            vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] == ENTITYNUM_NONE || ent->vehiclePrimaryDisabled != 0) {
            G_SetSoundBlend(soundBlend, 0, 0, 0);
        } else {
            G_SetSoundBlend(soundBlend, vehicleInfo->idleBlendSound0, vehicleInfo->idleBlendSound1,
                            vehicleState->idleSoundBlendRepeatDelay);
        }
    }

    if (vehicleState->soundBlendEntityNums[1] != ENTITYNUM_NONE) {
        soundBlend = &g_entities[vehicleState->soundBlendEntityNums[1]];
        if (notAlive || vehicleState->suspensionEnabled == 0 ||
            (vehicleState->soundBlendEntityNums[0] != ENTITYNUM_NONE && vehicleState->runSoundBlendRepeatDelay == 0.0f) ||
            (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] == ENTITYNUM_NONE &&
             fabsf(vehicleState->angularVelocity[0]) < VEH_RUN_SOUND_MIN_ANGULAR_SPEED)) {
            G_SetSoundBlend(soundBlend, 0, 0, 0);
        } else {
            G_SetSoundBlend(soundBlend, vehicleInfo->runBlendSound0, vehicleInfo->runBlendSound1, vehicleState->runSoundBlendRepeatDelay);
        }
    }

    if (notAlive) {
        ent->s.clientSound = 0;
    } else if (game_compat_veh_turret_activity_state(vehicleState, VEH_TURRET_ACTIVITY_PRIMARY) == VEH_TURRET_STATE_ACTIVE &&
               vehicleInfo->primaryActiveSound != 0) {
        ent->s.clientSound = vehicleInfo->primaryActiveSound;
    } else if (game_compat_veh_turret_activity_state(vehicleState, VEH_TURRET_ACTIVITY_PRIMARY) == VEH_TURRET_STATE_WINDDOWN &&
               vehicleInfo->primaryStopSound != 0) {
        G_PlaySoundAlias(ent, vehicleInfo->primaryStopSound);
    }

    if (vehicleState->soundBlendEntityNums[2] != ENTITYNUM_NONE && vehicleInfo->turretAltWeapon[0] != '\0') {
        (void)BG_GetInfoForWeapon(BG_GetWeaponIndexForName(vehicleInfo->turretAltWeapon) & 0xff);
        soundBlend = &g_entities[vehicleState->soundBlendEntityNums[2]];
        G_SetSoundBlend(soundBlend, 0, 0, 0);
        if (!notAlive && vehicleState->altWeaponSoundTime > 0) {
            G_SetSoundBlend(soundBlend, vehicleState->altWeaponFireSound, vehicleState->altWeaponFireSound, 0.0f);
            vehicleState->altWeaponSoundTime -= VEH_SOUND_THINK_INTERVAL_MS;
            if (vehicleState->altWeaponSoundTime < 1 && vehicleState->altWeaponStopSound != 0) {
                G_PlaySoundAlias(ent, vehicleState->altWeaponStopSound);
            }
        }
    }

    if (vehicleState->soundBlendEntityNums[3] != ENTITYNUM_NONE && vehicleInfo->turretGunnerWeapon[0] != '\0') {
        (void)BG_GetInfoForWeapon(BG_GetWeaponIndexForName(vehicleInfo->turretGunnerWeapon) & 0xff);
        soundBlend = &g_entities[vehicleState->soundBlendEntityNums[3]];
        G_SetSoundBlend(soundBlend, 0, 0, 0);
        if (!notAlive && vehicleState->gunnerWeaponSoundTime > 0) {
            G_SetSoundBlend(soundBlend, vehicleState->gunnerWeaponFireSound, vehicleState->gunnerWeaponFireSound, 0.0f);
            vehicleState->gunnerWeaponSoundTime -= VEH_SOUND_THINK_INTERVAL_MS;
            if (vehicleState->gunnerWeaponSoundTime < 1 && vehicleState->gunnerWeaponStopSound != 0) {
                G_PlaySoundAlias(ent, vehicleState->gunnerWeaponStopSound);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8998d  VEH_LinkPlayer                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8998d, 9998d_FUN_0009998d.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_LinkPlayer(gentity_t *vehicleEnt, gentity_t *player, int requestedSlot, qboolean cycleSlotPass)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)vehicleEnt->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    gclient_t *client = player->client;
    gentity_t *vehicleOwner = vehicleEnt->vehicleOwner;
    int passengerSlot;
    int attachBoneIndex;
    DObjSkelMat attachMatrix;
    DObjSkelMat viewMatrix;
    vec3_t viewAngles;

    /*
     * Stock callers pass false from normal use and true while cycling seats.
     * The Linux body does not read this fourth argument.
     */
    (void)cycleSlotPass;

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        Com_Error(1, VEH_LINK_ALREADY_USING_ERROR);
    }

    if (player->passEntityNum != ENTITYNUM_NONE) {
        Com_Error(1, VEH_LINK_ALREADY_OWNER_ERROR);
    }

    if (vehicleOwner != NULL && vehicleOwner->client != NULL &&
        vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] == ENTITYNUM_NONE && vehicleOwner != player) {
        trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0, va(VEH_LINK_NOT_OWNER_COMMAND));
        return;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((player->s.groundEntityNum == ENTITYNUM_NONE && client->ps.velocity[2] < -80.0f) ||
        client->vehicleControlTime > level.time - VEH_LINK_RETRY_DELAY_MS) {
        return;
    }

    if (vehicleInfo->type == VEHICLE_TYPE_TANK && client->vehicleExitState == 0) {
        client->vehicleControlTime = level.time;
    } else {
        client->vehicleControlTime = level.time + VEH_LINK_TANK_RETRY_DELAY_MS;
    }

    if (requestedSlot != 0 && vehicleState->passengerEntityNums[requestedSlot] != ENTITYNUM_NONE) {
        requestedSlot = 0;
    }

    passengerSlot = requestedSlot != 0 ? requestedSlot : VEH_FindNextPassengerSlot(vehicleState, 0, qfalse);
    if (passengerSlot == 0) {
        Com_Error(1, VEH_LINK_FULL_ERROR);
    }

    attachBoneIndex = VEH_GetPassengerTagBoneIndex(vehicleState, (uint32_t)passengerSlot);
    if (attachBoneIndex < 0) {
        Com_Error(1, VEH_LINK_ATTACH_BONE_ERROR, BG_GetVehiclePosTag(passengerSlot));
    }

    G_DObjGetWorldBoneIndexMatrix(vehicleEnt, attachBoneIndex, &attachMatrix);

    if (passengerSlot == VEH_PASSENGER_SLOT_DRIVER) {
        if (vehicleState->driverTagIndex < 0) {
            Com_Error(1, VEH_LINK_DRIVER_BONE_ERROR);
        }
        G_DObjGetWorldBoneIndexMatrix(vehicleEnt, vehicleState->driverTagIndex, &viewMatrix);
    } else if (passengerSlot == VEH_PASSENGER_SLOT_GUNNER) {
        if (vehicleState->gunnerTurretTagIndex < 0) {
            Com_Error(1, VEH_LINK_GUNNER_BONE_ERROR);
        }
        G_DObjGetWorldBoneIndexMatrix(vehicleEnt, vehicleState->gunnerTurretTagIndex, &viewMatrix);
    } else {
        if (vehicleState->bodyTagIndex < 0) {
            Com_Error(1, VEH_LINK_BODY_BONE_ERROR);
        }
        G_DObjGetWorldBoneIndexMatrix(vehicleEnt, vehicleState->bodyTagIndex, &viewMatrix);
    }

    Axis4ToAngles(&viewMatrix, viewAngles);
    viewAngles[2] = 0.0f;
    SetClientOrigin(player, attachMatrix.origin);
    SetClientViewAngle(player, viewAngles);

    if (!G_EntLinkToWithOffset(player, vehicleEnt, BG_GetVehiclePosTag(passengerSlot),
                               BG_GetVehiclePosOffset(vehicleInfo->type, requestedSlot), vec3_origin)) {
        Com_Error(1, VEH_LINK_CANNOT_LINK_ERROR, BG_GetVehiclePosTag(passengerSlot));
    }

    if (vehicleState->gunnerEntityNum == player->s.number) {
        game_compat_veh_set_turret_target_active(vehicleState, 0);
        vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
    }

    vehicleState->passengerEntityNums[passengerSlot] = player->s.number;
    game_compat_veh_set_entity_passenger_bits(vehicleEnt,
                                              game_compat_veh_entity_passenger_bits(vehicleEnt) | (1u << (passengerSlot & 0x1f)));

    if (passengerSlot == VEH_PASSENGER_SLOT_DRIVER) {
        game_compat_veh_set_entity_collision_mode(vehicleEnt, VEH_COLLISION_MODE_ALT_TRACE);
        vehicleEnt->s.eFlags |= VEH_VEHICLE_S_FLAG_DRIVER;
        vehicleEnt->passEntityNum = player->s.number;
        vehicleEnt->s.vehicleEntityNum = player->s.number;
    }

    player->passEntityNum = vehicleEnt->s.number;
    client->ps.entityStateFlags |= EF_IN_VEHICLE | EF_VEHICLE_ACTIVE;
    client->ps.entityStateFlags &= ~EF_VEHICLE_POPOUT;
    client->ps.viewLockedEntityNum = vehicleEnt->s.number;
    client->ps.playerStateFlags &= ~PMF_ADS;
    trap_SendServerCommand(client->ps.psClientNum, 1, VEH_CLIENT_COMMAND_RUN);
    trap_SendServerCommand(client->ps.psClientNum, 1, VEH_CLIENT_COMMAND_STANCE);
    client->ps.vehiclePosition = passengerSlot;
    client->ps.vehicleType = vehicleInfo->type;

    Scr_AddEntity(player);
    Scr_AddInt(client->ps.vehiclePosition);
    Scr_Notify(vehicleEnt, scr_const_activated, 2);
    Scr_AddEntity(vehicleEnt);
    Scr_AddInt(client->ps.vehiclePosition);
    Scr_Notify(player, scr_const_vehicle_activated, 2);

    vehicleEnt->s.entityAngles[2] = vehicleInfo->gunnerPitchLimitPos;
    vehicleEnt->s.entityAngles[1] = vehicleInfo->gunnerPitchLimitNeg;
}

/* ------------------------------------------------------------------ */
/*  0x89fb2  VEH_UnlinkPlayer                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x89fb2, 99fb2_VEH_UnlinkPlayer.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_UnlinkPlayer(gentity_t *player, int keepVehicle)
{
    gclient_t *client = player->client;
    gentity_t *vehicleEnt;
    vehicle_state_t *vehicleState;
    const vehicleInfo_t *vehicleInfo;
    vec3_t dismountOrigin;

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) == 0) {
        Com_Error(1, VEH_UNLINK_NOT_USING_ERROR);
    }

    if (player->passEntityNum == ENTITYNUM_NONE) {
        client->ps.entityStateFlags &= ~EF_VEHICLE_STATE_MASK;
        Com_Printf(VEH_UNLINK_NO_OWNER_WARNING);
        return qtrue;
    }

    vehicleEnt = &g_entities[player->passEntityNum];
    vehicleState = (vehicle_state_t *)vehicleEnt->vehicle;
    vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (vehicleEnt->health >= 1 && keepVehicle != 0 && client->vehicleControlTime > level.time - VEH_EXIT_DELAY_MS &&
        client->vehicleExitState <= 1 && vehicleInfo->type == VEHICLE_TYPE_TANK) {
        trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0, va(VEH_UNLINK_EXIT_DELAY_COMMAND));
        return qfalse;
    }

    if (keepVehicle != 0 &&
        !VEH_FindValidDismountSpot(vehicleEnt, client->ps.psOrigin, player->mins, player->maxs, dismountOrigin, player->s.number)) {
        trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0, va(VEH_UNLINK_BLOCKED_COMMAND));
        return qfalse;
    }

    G_EntUnlink(player);
    vehicleState->passengerEntityNums[client->ps.vehiclePosition] = ENTITYNUM_NONE;
    game_compat_veh_set_entity_passenger_bits(vehicleEnt, game_compat_veh_entity_passenger_bits(vehicleEnt) &
                                                              ~(1u << (((uint8_t)client->ps.vehiclePosition) & 0x1f)));

    if (client->ps.vehiclePosition == VEH_PASSENGER_SLOT_DRIVER) {
        vehicleEnt->s.eFlags &= ~VEH_VEHICLE_S_FLAG_DRIVER;
        vehicleEnt->passEntityNum = ENTITYNUM_NONE;
        vehicleEnt->s.vehicleEntityNum = ENTITYNUM_NONE;
        vehicleEnt->s.headIcon = 0;

        if (vehicleEnt->vehiclePrimaryDisabled == 0 && vehicleInfo->type == VEHICLE_TYPE_TANK) {
            vehicleState->scriptedDriverEndTime = level.time + VEH_EXIT_ROLL_DELAY_MS;
            if (vehicleState->animLeftSource == 0 || level.time - VEH_EXIT_ANIM_GRACE_MS <= vehicleState->animLeftTime) {
                vehicleState->animLeftTarget = 0;
            } else {
                vehicleState->animLeftTarget = vehicleState->animLeftSource;
            }

            if (vehicleState->animRightSource == 0 || level.time - VEH_EXIT_ANIM_GRACE_MS <= vehicleState->animRightTime) {
                vehicleState->animRightTarget = 0;
            } else {
                vehicleState->animRightTarget = vehicleState->animRightSource;
            }
        }
    } else if (client->ps.vehiclePosition == VEH_PASSENGER_SLOT_GUNNER) {
        if (vehicleState->soundBlendEntityNums[2] != ENTITYNUM_NONE && vehicleState->altWeaponSoundTime > 0) {
            vehicleState->altWeaponSoundTime = 10;
        }
        client->ps.playerStateFlags &= ~VEH_CLIENT_STANCE_FLAG_GUNNER;
    }

    if (vehicleEnt->s.vehicleEntityNum == player->s.number) {
        vehicleEnt->s.vehicleEntityNum = ENTITYNUM_NONE;
    }

    player->passEntityNum = ENTITYNUM_NONE;
    client->ps.entityStateFlags &= ~EF_VEHICLE_STATE_MASK;
    client->ps.viewLockedEntityNum = ENTITYNUM_NONE;
    client->ps.vehiclePosition = 0;
    client->ps.playerStateFlags &= ~PMF_ADS;
    client->ps.adsFraction = 0.0f;
    trap_SendServerCommand(client->ps.psClientNum, 1, VEH_CLIENT_COMMAND_RUN);
    trap_SendServerCommand(client->ps.psClientNum, 1, VEH_CLIENT_COMMAND_STANCE);

    player->vehicleReenterTime = level.time + VEH_PLAYER_REENTER_DELAY_MS;
    player->lastVehicleEntityNum = vehicleEnt->s.number;

    if (keepVehicle != 0) {
        SetClientOrigin(player, dismountOrigin);
        if (level.time < vehicleState->scriptedDriverEndTime) {
            client->ps.velocity[2] = VEH_EXIT_VERTICAL_BOOST;
            client->ps.velocity[0] = 0.0f;
            client->ps.velocity[1] = 0.0f;
        }
    }

    Scr_AddEntity(player);
    Scr_Notify(vehicleEnt, scr_const_deactivated, 1);
    Scr_AddEntity(vehicleEnt);
    Scr_Notify(player, scr_const_vehicle_deactivated, 1);
    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x8a51f  G_ParseScrVehicleInfo                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8a51f, 9a51f_G_ParseScrVehicleInfo.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED; file-list local and trap_FS_GetFileList capacity are both 4096 bytes. */
void G_ParseScrVehicleInfo(void)
{
    char fileList[VEH_VEHICLE_FILE_LIST_SIZE];
    char vehicleText[VEH_VEHICLE_FILE_BUFFER_SIZE];
    char path[MAX_QPATH];
    const char *vehicleFiles[VEH_MAX_SCR_VEHICLES];
    const char *cursor;
    const size_t magicLength = strlen(VEH_VEHICLE_FILE_MAGIC);
    int fileCount;

    g_vehicleInfoCount = 0;
    fileCount = trap_FS_GetFileList(VEH_VEHICLE_FILE_FOLDER, VEH_VEHICLE_FILE_EXTENSION, fileList, VEH_VEHICLE_FILE_LIST_SIZE);
    if (fileCount > VEH_MAX_SCR_VEHICLES) {
        Com_Error(1, VEH_VEHICLE_MAX_FILES_ERROR, VEH_MAX_SCR_VEHICLES, fileCount);
    }

    cursor = fileList;
    for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
        vehicleFiles[fileIndex] = cursor;
        cursor = &cursor[strlen(cursor) + 1];
    }

    for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
        int handle;
        int fileLength;

        const size_t folderLength = strlen(VEH_VEHICLE_FILE_FOLDER);
        const size_t fileNameLength = strlen(vehicleFiles[fileIndex]);
        /* NOT_FROM_ORIGINAL_SOURCE: require the complete mounted-file path and
         * NUL to fit; the same proof covers the persistent vehicle name. */
        if (folderLength >= sizeof(path) || fileNameLength > sizeof(path) - folderLength - 2) {
            Com_Error(ERR_DROP, COM_ERROR_MARKER "Vehicle file path is too long");
            continue;
        }
        Com_sprintf(path, sizeof(path), VEH_VEHICLE_PATH_FORMAT, VEH_VEHICLE_FILE_FOLDER, vehicleFiles[fileIndex]);
        fileLength = trap_FS_FOpenFile(path, &handle, FS_READ);
        if (fileLength < 1) {
            Com_Error(1, VEH_VEHICLE_LOAD_ERROR, path);
            continue;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: require the complete file magic before
         * reading it or deriving the remaining payload length. */
        if (fileLength < (int)magicLength) {
            trap_FS_FCloseFile(handle);
            Com_Error(ERR_DROP, VEH_VEHICLE_MAGIC_ERROR, path);
            continue;
        }
        trap_FS_Read(vehicleText, (int)magicLength, handle);
        vehicleText[magicLength] = '\0';
        if (strncmp(vehicleText, VEH_VEHICLE_FILE_MAGIC, magicLength) != 0) {
            trap_FS_FCloseFile(handle);
            Com_Error(1, VEH_VEHICLE_MAGIC_ERROR, path);
            continue;
        }

        if (fileLength - (int)magicLength >= VEH_VEHICLE_FILE_BUFFER_SIZE) {
            trap_FS_FCloseFile(handle);
            Com_Error(1, VEH_VEHICLE_SIZE_ERROR, path);
            continue;
        }

        trap_FS_Read(vehicleText, fileLength - (int)magicLength, handle);
        vehicleText[fileLength - (int)magicLength] = '\0';
        trap_FS_FCloseFile(handle);

        if (Info_Validate(vehicleText) == 0) {
            Com_Error(1, VEH_VEHICLE_VALID_ERROR, path);
            continue;
        }

        {
            vehicleInfo_t *vehicleInfo = &g_vehicleInfoTable[g_vehicleInfoCount];

            /* NOT_FROM_ORIGINAL_SOURCE: the path gate proves this name fits;
             * keep the destination operation independently bounded. */
            Q_strncpyz(vehicleInfo->name, vehicleFiles[fileIndex], sizeof(vehicleInfo->name));
            if (ParseConfigStringToStruct(vehicleInfo, vehicleParseFields, s_numVehicleFields, vehicleText, VEH_VEHICLE_PARSE_CUSTOM_MAX,
                                          VEH_ParseSpecificField, VEH_Strcpy) != 0) {
                g_vehicleInfoCount++;
            }
        }
    }

    for (int vehicleIndex = 0; vehicleIndex < g_vehicleInfoCount; vehicleIndex++) {
        vehicleInfo_t *vehicleInfo = &g_vehicleInfoTable[vehicleIndex];

        vehicleInfo->acceleration *= VEH_INFO_SPEED_SCALE;
        vehicleInfo->pathSpeed *= VEH_INFO_SPEED_SCALE;
        vehicleInfo->maxSpeed *= VEH_INFO_SPEED_SCALE;
        vehicleInfo->pathSpeedDenom *= VEH_INFO_SPEED_SCALE;

        {
            float *collisionBounds = vehicleInfo->collisionMins;

            for (int axis = 0; axis < 6; axis++) {
                collisionBounds[axis] = vehicleInfo->collisionBoundsSource[axis] * VEH_INFO_HALF_SCALE;
            }
        }

        for (int soundIndex = 0; soundIndex < VEH_INFO_SOUND_ALIAS_COUNT; soundIndex++) {
            const char *soundName = vehicleInfo->soundNames[soundIndex];

            if (soundName[0] == '\0') {
                ((uint8_t *)(void *)&vehicleInfo->idleBlendSound0)[soundIndex] = 0;
            } else {
                ((uint8_t *)(void *)&vehicleInfo->idleBlendSound0)[soundIndex] = G_SoundAliasIndex(soundName);
            }
        }

        vehicleInfo->hitPersonSound = G_SoundAliasIndex(VEH_VEHICLE_HIT_PERSON_SOUND);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8aabc  G_InitScrVehicles                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8aabc, 9aabc_G_InitScrVehicles.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_InitScrVehicles(void)
{
    for (int index = 0; index < VEH_MAX_SCR_VEHICLES; index++) {
        vehicle_state_t *vehicleState = game_compat_veh_scr_vehicle_state_at(index);

        G_VehInitPathPos(game_compat_veh_path_cursor(vehicleState));
        vehicleState->entityNum = ENTITYNUM_NONE;
    }

    level.vehicleStateBase = vehClientPhysicsRecords;
}

/* ------------------------------------------------------------------ */
/*  0x8ab36  G_SetupScrVehicles                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ab36, 9ab36_G_SetupScrVehicles.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_SetupScrVehicles(void)
{
    for (int index = 0; index < VEH_MAX_SCR_VEHICLES; index++) {
        vehicle_state_t *vehicleState = game_compat_veh_scr_vehicle_state_at(index);

        if (vehicleState->entityNum != ENTITYNUM_NONE) {
            VEH_SetupCollmap(VEH_GetEntity(vehicleState->entityNum));
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8abaf  G_FreeScrVehicles                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8abaf, 9abaf_G_FreeScrVehicles.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_FreeScrVehicles(void)
{
    for (int index = 0; index < VEH_MAX_SCR_VEHICLES; index++) {
        G_VehFreePathPos(game_compat_veh_path_cursor(game_compat_veh_scr_vehicle_state_at(index)));
    }
}

/* ------------------------------------------------------------------ */
/*  0x8abfd  G_SpawnVehicle                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8abfd, 9abfd_G_SpawnVehicle.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_SpawnVehicle(gentity_t *ent, const char *vehicleName, int skipCollmap)
{
    vehicle_state_t *vehicleState = NULL;
    int16_t vehicleSlot;
    int vehicleInfoIndex;

    for (vehicleSlot = 0; vehicleSlot < VEH_MAX_SCR_VEHICLES; vehicleSlot++) {
        if (!game_compat_veh_scr_vehicle_slot_in_use(vehicleSlot)) {
            vehicleState = game_compat_veh_scr_vehicle_state_at(vehicleSlot);
            break;
        }
    }

    if (vehicleSlot == VEH_MAX_SCR_VEHICLES) {
        Com_Error(1, VEH_VEHICLE_MAX_COUNT_ERROR, VEH_MAX_SCR_VEHICLES);
    }

    memset(vehicleState, 0, sizeof(*vehicleState));
    vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
    for (int index = 0; index < 4; index++) {
        vehicleState->soundBlendEntityNums[index] = ENTITYNUM_NONE;
    }

    vehicleInfoIndex = VEH_FindVehicleInfoIndex(vehicleName);
    if (vehicleInfoIndex < 0) {
        Com_Error(1, VEH_VEHICLE_INFO_LOOKUP_ERROR, SL_ConvertToString(ent->targetname));
    }

    ent->s.vehicleSlot = vehicleSlot;
    vehicleState->slotIndex = vehicleSlot;
    VEH_InitEntity(ent, vehicleState, (int16_t)vehicleInfoIndex);
    VEH_InitVehicle(ent, vehicleState, (int16_t)vehicleInfoIndex);

    if (skipCollmap == 0) {
        VEH_SetupCollmap(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8ad7d  G_FreeVehicle                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ad7d, 9ad7d_G_FreeVehicle.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void G_FreeVehicle(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    ent->health = 0;
    VEH_UpdateSounds(ent);

    for (int index = 0; index < 4; index++) {
        if (vehicleState->soundBlendEntityNums[index] != ENTITYNUM_NONE) {
            G_FreeEntity(&g_entities[vehicleState->soundBlendEntityNums[index]]);
        }
    }

    ent->think = NULL;
    ent->pain = NULL;
    ent->die = NULL;
    ent->touch = NULL;
    ent->use = NULL;
    ent->controller = NULL;
    ent->nextthink = 0;
    game_compat_veh_set_entity_usable_byte(ent, 0);
    ent->maxSpeed = 0;
    game_compat_veh_set_entity_collision_mode(ent, 0);
    ent->s.eFlags = 0;
    ent->s.pos.trType = TR_STATIONARY;
    ent->s.apos.trType = TR_STATIONARY;

    Scr_SetString(&vehicleState->pathPosition.targetNode.targetname, 0);
    Scr_SetString(&vehicleState->pathPosition.cachedNode.targetname, 0);
    Scr_SetString(&vehicleState->pathPosition.targetNode.target, 0);
    Scr_SetString(&vehicleState->pathPosition.cachedNode.target, 0);

    vehicleState->entityNum = ENTITYNUM_NONE;
    ent->vehicle = NULL;
}

/* ------------------------------------------------------------------ */
/*  0x8afdf  G_FreeVehicleRefs                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8afdf, 9afdf_G_FreeVehicleRefs.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
void G_FreeVehicleRefs(gentity_t *ent)
{
    for (int index = 0; index < VEH_MAX_SCR_VEHICLES; index++) {
        vehicle_state_t *vehicleState = game_compat_veh_scr_vehicle_state_at(index);

        if (!game_compat_veh_scr_vehicle_slot_in_use(index)) {
            continue;
        }

        for (int soundIndex = 0; soundIndex < 4; soundIndex++) {
            if (vehicleState->soundBlendEntityNums[soundIndex] == ent->s.number) {
                vehicleState->soundBlendEntityNums[soundIndex] = ENTITYNUM_NONE;
            }
        }

        if (vehicleState->gunnerEntityNum == ent->s.number) {
            vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8b0ce  G_VehicleClientThink                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b0ce, 9b0ce_G_VehicleClientThink.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
void G_VehicleClientThink(void)
{
    vehClientThinkRecursing = 1;
    for (int index = 0; index < VEH_MAX_SCR_VEHICLES; index++) {
        if (game_compat_veh_scr_vehicle_slot_in_use(index)) {
            gentity_t *ent = &g_entities[game_compat_veh_scr_vehicle_state_at(index)->entityNum];

            if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_ALT_TRACE) {
                G_RunThink(ent);
            }
        }
    }
    vehClientThinkRecursing = 0;
}

/* ------------------------------------------------------------------ */
/*  0x8b16c  G_UpdateVehicleTags                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b16c, 9b16c_G_UpdateVehicleTags.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
void G_UpdateVehicleTags(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    vehicleState->driverTagIndex = trap_DObjGetBoneIndex(ent, BG_GetVehiclePosTag(1));
    vehicleState->detachTagIndex = trap_DObjGetBoneIndex(ent, "tag_detach");
    vehicleState->popoutTagIndex = trap_DObjGetBoneIndex(ent, "tag_popout");
    vehicleState->bodyTagIndex = trap_DObjGetBoneIndex(ent, "tag_body");
    vehicleState->primaryBaseTagIndex = trap_DObjGetBoneIndex(ent, "tag_turret");
    vehicleState->primaryTurretTagIndex = trap_DObjGetBoneIndex(ent, "tag_barrel");
    vehicleState->primaryAltTurretTagIndex = trap_DObjGetBoneIndex(ent, "tag_coaxel");
    vehicleState->gunnerTagIndex = trap_DObjGetBoneIndex(ent, BG_GetVehiclePosTag(2));
    vehicleState->gunnerTurretTagIndex = trap_DObjGetBoneIndex(ent, "tag_secondary_gun");
    vehicleState->secondaryBaseTagIndex = trap_DObjGetBoneIndex(ent, "tag_secondary_base");
    vehicleState->chasecamTagIndex = trap_DObjGetBoneIndex(ent, "tag_chasecam");
    vehicleState->aimDownBarrelTagIndex = trap_DObjGetBoneIndex(ent, "tag_aimdownbarrel");

    for (int index = 0; index < VEH_TAG_FLASH_COUNT; index++) {
        vehicleState->primaryFlashTagIndices[index] = trap_DObjGetBoneIndex(ent, vehiclePrimaryFlashTagNames[index]);
    }

    for (int index = 0; index < VEH_TAG_FLASH_COUNT; index++) {
        vehicleState->altFireTagIndices[index] = trap_DObjGetBoneIndex(ent, vehicleAltFireTagNames[index]);
    }

    for (int index = 0; index < VEH_TAG_FLASH_COUNT; index++) {
        vehicleState->secondaryFlashTagIndices[index] = trap_DObjGetBoneIndex(ent, vehicleSecondaryFlashTagNames[index]);
    }

    for (int index = 0; index < VEH_WHEEL_TAG_COUNT; index++) {
        vehicleState->wheelTagIndices[index] = trap_DObjGetBoneIndex(ent, vehicleWheelTagNames[index]);
    }

    for (int index = 0; index < VEH_PASSENGER_TAG_COUNT; index++) {
        const char *tagName = index == 0 ? "tag_passenger" : va("tag_passenger%i", index + 1);

        vehicleState->passengerTagIndices[index] = trap_DObjGetBoneIndex(ent, tagName);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8b46a  G_GetVehicleInfoIndex                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b46a, 9b46a_G_GetVehicleInfoIndex.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
int G_GetVehicleInfoIndex(const char *name)
{
    return (int)(int16_t)VEH_FindVehicleInfoIndex(name);
}

/* ------------------------------------------------------------------ */
/*  0x8b485  G_GetVehicleInfoName                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b485, 9b485_G_GetVehicleInfoName.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
const char *G_GetVehicleInfoName(int16_t vehicleInfoIndex)
{
    return game_compat_veh_get_vehicle_info(vehicleInfoIndex)->name;
}

/* ------------------------------------------------------------------ */
/*  0x8b4b9  GScr_GetNumVehicles                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b4b9, 9b4b9_GScr_GetNumVehicles.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
void GScr_GetNumVehicles(void)
{
    int count = 0;

    for (int index = 0; index < VEH_MAX_SCR_VEHICLES; index++) {
        if (game_compat_veh_scr_vehicle_slot_in_use(index)) {
            count++;
        }
    }

    Scr_AddInt(count);
}

/* ------------------------------------------------------------------ */
/*  0x8b51c  GScr_PrecacheVehicle                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b51c, 9b51c_GScr_PrecacheVehicle.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
void GScr_PrecacheVehicle(void)
{
    level_locals_t *lvl = &level;
    const char *vehicleName = Scr_GetString(0);
    int vehicleInfoIndex;
    const vehicleInfo_t *vehicleInfo;

    if (lvl->spawning == 0) {
        Scr_Error("precacheVehicle must be called before any wait statements in the level script\n");
    }

    vehicleInfoIndex = VEH_FindVehicleInfoIndex(vehicleName);
    if (vehicleInfoIndex < 0) {
        Scr_Error(va("Cannot find vehicle info for [%s]\n", vehicleName));
    }

    vehicleInfo = game_compat_veh_get_vehicle_info(vehicleInfoIndex);
    game_compat_veh_precache_weapon_if_present(vehicleInfo->turretWeapon);
    game_compat_veh_precache_weapon_if_present(vehicleInfo->turretAltWeapon);
    game_compat_veh_precache_weapon_if_present(vehicleInfo->turretGunnerWeapon);
}

/* ------------------------------------------------------------------ */
/*  0x8b673  G_IsVehicleUsable                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b673, 9b673_G_IsVehicleUsable.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
qboolean G_IsVehicleUsable(gentity_t *vehicle, gentity_t *player)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)vehicle->vehicle;
    int passengerSlot;

    if (player->client == NULL) {
        return qfalse;
    }

    if ((player->client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        return qfalse;
    }

    if (player->passEntityNum != ENTITYNUM_NONE) {
        return qfalse;
    }

    passengerSlot = VEH_FindNextPassengerSlot(vehicleState, 0, qfalse);
    if (passengerSlot == 0) {
        return qfalse;
    }

    if (game_compat_veh_entity_current_speed(vehicle) > VEH_USABLE_MAX_SPEED) {
        return qfalse;
    }

    if (vehicle->health < 1) {
        return qfalse;
    }

    if ((vehicle->scriptContents & VEH_USABLE_SCRIPT_CONTENTS) == 0) {
        return qfalse;
    }

    if (player->client->ps.grenadeTimeLeft != 0) {
        return qfalse;
    }

    for (int slot = VEH_PASSENGER_SLOT_DRIVER; slot < VEH_PASSENGER_SLOT_COUNT; slot++) {
        const int occupantNum = vehicleState->passengerEntityNums[slot];

        if (occupantNum != ENTITYNUM_NONE && OnSameTeam(&g_entities[occupantNum], player) == 0) {
            return qfalse;
        }
    }

    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x8b7ed  G_IsVehicleUnusable                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b7ed, 9b7ed_G_IsVehicleUnusable.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *G_IsVehicleUnusable(gentity_t *player)
{
    gentity_t *vehicle;

    if (player->client == NULL) {
        return NULL;
    }

    if ((player->client->ps.entityStateFlags & EF_IN_VEHICLE) == 0) {
        return NULL;
    }

    if (player->passEntityNum == ENTITYNUM_NONE) {
        return NULL;
    }

    vehicle = &g_entities[player->passEntityNum];
    if ((vehicle->scriptContents & VEH_USABLE_SCRIPT_CONTENTS) == 0) {
        return NULL;
    }

    return vehicle;
}

/* ------------------------------------------------------------------ */
/*  0x8bf6d  G_GetNonPVSTankInfo                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8bf6d, 9bf6d_G_GetNonPVSTankInfo.c, VERIFY-VEHICLE-REFS-2026-06-17): DATAFLOW_VERIFIED */
int G_GetNonPVSTankInfo(gentity_t *viewer, const float *origin, int previousEntNum)
{
    static const float half = 0.5f;
    static const float yawScale = VEH_NONPVS_TANK_YAW_SCALE;
    const int firstEntNum = previousEntNum == VEH_NONPVS_TANK_ENTITY_NONE ? 0 : previousEntNum + 1;

    for (int scan = 0; scan < level.num_entities; scan++) {
        const int entNum = (firstEntNum + scan) % level.num_entities;
        gentity_t *tank = &g_entities[entNum];
        vehicle_state_t *vehicleState;
        int driverEntNum;
        float xScale;
        float yScale;
        float diffX;
        float diffY;
        int tankIndex;
        int deltaX;
        int deltaY;
        uint32_t yaw;
#if defined(__x86_64__)
        coduo_x87_truncation_control_t conversionControl;
#endif

        if (tank->linked == 0) {
            continue;
        }

        tankIndex = G_GetTankIndex(tank->s.number);
        if (tankIndex == -1 || trap_InSnapshot(origin, tank->s.number) != 0) {
            continue;
        }

        vehicleState = (vehicle_state_t *)tank->vehicle;
        driverEntNum = vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER];
        if (driverEntNum == ENTITYNUM_NONE || driverEntNum == viewer->s.number) {
            continue;
        }

        if (OnSameTeam(viewer, &g_entities[driverEntNum]) == 0) {
            continue;
        }

        tankIndex = G_GetTankIndex(tank->s.number);
        /* 0x8c0a6..0x8c0f9: the origin deltas are stored to float slots, but
         * the +0.5f sums feed fistp directly with no float store. */
        diffX = tank->currentOrigin[0] - origin[0];
        diffY = tank->currentOrigin[1] - origin[1];
#if EMULATE_X87
        deltaX = x87f_store_i32_trunc(x87f_add(x87f_load_f32(diffX), x87f_load_f32(half)));
        deltaY = x87f_store_i32_trunc(x87f_add(x87f_load_f32(diffY), x87f_load_f32(half)));
#elif defined(__x86_64__)
        deltaX = CODUO_X87_TRUNCATE_I32_FIRST(&conversionControl, (long double)diffX + (long double)half);
        deltaY = CODUO_X87_TRUNCATE_I32_NEXT(&conversionControl, (long double)diffY + (long double)half);
#else
        deltaX = (int)(diffX + half);
        deltaY = (int)(diffY + half);
#endif

        xScale = game_compat_veh_non_pvs_tank_delta_scale(deltaX);
        yScale = game_compat_veh_non_pvs_tank_delta_scale(deltaY);
        if (xScale < 1.0f || yScale < 1.0f) {
            /* 0x8c19b/0x8c1bb: fild * scale feeds fistp directly. */
            if (xScale < yScale) {
#if EMULATE_X87
                deltaY = x87f_store_i32_trunc(x87f_mul(x87f_load_i32(deltaY), x87f_load_f32(xScale)));
#elif defined(__x86_64__)
                deltaY = CODUO_X87_TRUNCATE_I32_NEXT(&conversionControl, (long double)deltaY * (long double)xScale);
#else
                deltaY = (int)(deltaY * xScale);
#endif
            } else if (yScale < xScale) {
#if EMULATE_X87
                deltaX = x87f_store_i32_trunc(x87f_mul(x87f_load_i32(deltaX), x87f_load_f32(yScale)));
#elif defined(__x86_64__)
                deltaX = CODUO_X87_TRUNCATE_I32_NEXT(&conversionControl, (long double)deltaX * (long double)yScale);
#else
                deltaX = (int)(deltaX * yScale);
#endif
            }
        }

        deltaX = game_compat_veh_clamp_non_pvs_tank_delta(deltaX);
        deltaY = game_compat_veh_clamp_non_pvs_tank_delta(deltaY);
        /* 0x8c289: the yaw product feeds fistp directly (no float store). */
#if EMULATE_X87
        yaw = (uint32_t)x87f_store_i32_trunc(x87f_mul(x87f_load_f32(tank->currentAngles[1]), x87f_load_f32(yawScale)));
#elif defined(__x86_64__)
        yaw = (uint32_t)CODUO_X87_TRUNCATE_I32_NEXT(&conversionControl, (long double)tank->currentAngles[1] * (long double)yawScale);
#else
        yaw = (uint32_t)(int)(tank->currentAngles[1] * yawScale);
#endif

        return (yaw << 24) | (game_compat_veh_pack_non_pvs_tank_coord(deltaY) << 15) |
               (game_compat_veh_pack_non_pvs_tank_coord(deltaX) << 6) | ((uint32_t)tankIndex & VEH_NONPVS_TANK_INDEX_MASK);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  0x7bfa8  VEH_Strcpy                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7bfa8, 8bfa8_FUN_0008bfa8.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_Strcpy(char *dest, const char *src)
{
    /* NOT_FROM_ORIGINAL_SOURCE: every vehicle string destination is a fixed
     * inline field; require the complete authored value and NUL to fit. */
    if (strlen(src) >= VEH_VEHICLE_INLINE_STRING_SIZE) {
        Com_Error(ERR_DROP, COM_ERROR_MARKER "Vehicle string value is too long");
        return;
    }
    Q_strncpyz(dest, src, VEH_VEHICLE_INLINE_STRING_SIZE);
}

/* ------------------------------------------------------------------ */
/*  0x7bfd2  VEH_DebugLine                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7bfd2, 8bfd2_FUN_0008bfd2.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_DebugLine(const float *start, const float *end, float red, float green, float blue)
{
    const vec4_t color = {red, green, blue, 1.0f};

    G_DebugLine(start, end, color, 1, 0);
}

/* ------------------------------------------------------------------ */
/*  0x7c02c  VEH_DebugBox                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c02c, 8c02c_FUN_0008c02c.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_DebugBox(const float *origin, float size, float red, float green, float blue)
{
    const vec4_t color = {red, green, blue, 1.0f};
    vec3_t maxs;
    vec3_t mins;

    maxs[0] = (float)((long double)origin[0] + (long double)size * 0.5L);
    maxs[1] = (float)((long double)origin[1] + (long double)size * 0.5L);
    maxs[2] = (float)((long double)origin[2] + (long double)size * 0.5L);

    mins[0] = (float)((long double)origin[0] - (long double)size * 0.5L);
    mins[1] = (float)((long double)origin[1] - (long double)size * 0.5L);
    mins[2] = (float)((long double)origin[2] - (long double)size * 0.5L);

    G_DebugBox(maxs, mins, color, 1, 0);
}

/* ------------------------------------------------------------------ */
/*  0x7c110  VEH_DebugCircleVertical                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c110, 8c110_FUN_0008c110.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_DebugCircleVertical(const float *origin, float radius, float height, float red, float green, float blue)
{
    const vec4_t color = {red, green, blue, 1.0f};
    vec3_t top;

    top[0] = origin[0];
    top[1] = origin[1];
    top[2] = origin[2] + height;

    G_DebugCircle(origin, radius, color, 1, 1, 0);
    G_DebugCircle(top, radius, color, 1, 1, 0);
}

/* ------------------------------------------------------------------ */
/*  0x7c1ca  VEH_ParseSpecificField                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c1ca, 8c1ca_VEH_ParseSpecificField.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_ParseSpecificField(void *vehicleInfoBase, const char *value, int fieldType)
{
    vehicleInfo_t *vehicleInfo = vehicleInfoBase;

    if (fieldType != VEH_PARSE_FIELD_TYPE_VEHICLE) {
        Com_Error(1, VEH_PARSE_ERROR_BAD_FIELD_TYPE, fieldType);
        return 0;
    }

    for (vehicle_type_t type = VEHICLE_TYPE_UNKNOWN; type < VEHICLE_TYPE_COUNT; type = (vehicle_type_t)(type + 1)) {
        if (strcasecmp(value, vehicleTypeNames[type]) == 0) {
            vehicleInfo->type = (int16_t)type;
            return 1;
        }
    }

    Com_Error(1, VEH_PARSE_ERROR_UNKNOWN_TYPE, value);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  0x7c280  VEH_GetEntity                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c280, 8c280_FUN_0008c280.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *VEH_GetEntity(int entNum)
{
    gentity_t *ent;

    if (entNum >= (int)MAX_GENTITIES) {
        Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_RANGE, entNum));
        return NULL;
    }

    ent = &g_entities[entNum];
    if (game_compat_veh_entity_model_classname(ent) != scr_const_script_vehicle) {
        Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_TYPE, entNum));
    }

    if (ent->vehicle == NULL) {
        Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_STATE, entNum));
    }

    return ent;
}

/* ------------------------------------------------------------------ */
/*  0x8c2d2  Scr_Vehicle_Controller                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8c2d2, 9c2d2_Scr_Vehicle_Controller.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Scr_Vehicle_Controller(gentity_t *ent, uint32_t *partBits)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const vec3_t bodyAngles = {
        ((float)game_compat_veh_float_slot_int_bits(&ent->s.loopedFxForward[0]) - VEH_MOTION_ANIM_CENTER) * 30.0f / VEH_MOTION_ANIM_CENTER,
        0.0f,
        ((float)game_compat_veh_float_slot_int_bits(&ent->s.loopedFxForward[2]) - VEH_MOTION_ANIM_CENTER) * 30.0f / VEH_MOTION_ANIM_CENTER};
    const vec3_t primaryBaseAngles = {0.0f, game_compat_veh_entity_primary_yaw(ent), 0.0f};
    const vec3_t primaryBarrelAngles = {ent->s.vehicleTurret.primaryPitch, 0.0f, 0.0f};
    const vec3_t gunnerBaseAngles = {0.0f, game_compat_veh_entity_gunner_yaw_base(ent), 0.0f};
    const vec3_t gunnerBarrelAngles = {ent->s.vehicleTurret.gunnerPitch, 0.0f, 0.0f};
    int boneIndex;

    boneIndex = vehicleState->bodyTagIndex;
    if (boneIndex >= 0) {
        G_DObjSetLocalBoneIndex(ent, partBits, boneIndex, vec3_origin, bodyAngles);
    }

    boneIndex = vehicleState->primaryBaseTagIndex;
    if (boneIndex >= 0) {
        G_DObjSetLocalBoneIndex(ent, partBits, boneIndex, vec3_origin, primaryBaseAngles);
    }

    boneIndex = vehicleState->primaryTurretTagIndex;
    if (boneIndex >= 0) {
        G_DObjSetLocalBoneIndex(ent, partBits, boneIndex, vec3_origin, primaryBarrelAngles);
    }

    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL) {
        boneIndex = vehicleState->secondaryBaseTagIndex;
    } else {
        boneIndex = vehicleState->gunnerTagIndex;
    }

    if (boneIndex >= 0) {
        G_DObjSetLocalBoneIndex(ent, partBits, boneIndex, vec3_origin, gunnerBaseAngles);
    }

    boneIndex = vehicleState->gunnerTurretTagIndex;
    if (boneIndex >= 0) {
        G_DObjSetLocalBoneIndex(ent, partBits, boneIndex, vec3_origin, gunnerBarrelAngles);
    }
}

/* ------------------------------------------------------------------ */
/*  0x7c33f  VEH_FindVehicleInfoIndex                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c33f, 8c33f_FUN_0008c33f.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
int VEH_FindVehicleInfoIndex(const char *name)
{
    if (name != NULL && name[0] != '\0') {
        for (int16_t index = 0; index < g_vehicleInfoCount; index++) {
            if (strcasecmp(name, game_compat_veh_get_vehicle_info(index)->name) == 0) {
                return index;
            }
        }
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/*  0x7c3ce  VEH_FindVehicleNodeByTargetname                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c3ce, 8c3ce_FUN_0008c3ce.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *VEH_FindVehicleNodeByTargetname(const char *targetname)
{
    for (int entityNum = 0; entityNum < level.num_entities; entityNum++) {
        gentity_t *ent = &g_entities[entityNum];

        if (ent->linked == 0) {
            continue;
        }

        if (game_compat_veh_entity_model_classname(ent) != scr_const_script_vehicle_node) {
            continue;
        }

        if (strcasecmp(SL_ConvertToString(ent->targetname), targetname) == 0) {
            return ent;
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  0x7c488  VEH_GetWheelOrigin                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c488, 8c488_FUN_0008c488.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_GetWheelOrigin(gentity_t *ent, int wheelIndex, vec3_t origin)
{
    const char *tagName = vehicleWheelTagNames[wheelIndex];
    const DObjSkelMat *tagMatrix = G_DObjGetLocalTagMatrix(ent, tagName);

    if (tagMatrix == NULL) {
        Com_Error(1, VEH_SCRIPT_ENTITY_ERROR_TAG, SL_ConvertToString(ent->targetname), tagName);
    }

    origin[0] = tagMatrix->origin[0];
    origin[1] = tagMatrix->origin[1];
    origin[2] = tagMatrix->origin[2];
}

/* ------------------------------------------------------------------ */
/*  0x7c525  VEH_TrackValue                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c525, 8c525_FUN_0008c525.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
float VEH_TrackValue(float target, float current, float speed)
{
    const float delta = target - current;
    float step = speed;

    if (!(delta > 0.0f)) {
        step = -step;
    }

    step *= VEH_APPROACH_FRAME_SECONDS;

    if (!(fabsf(delta) > VEH_APPROACH_EPSILON)) {
        return target;
    }

    if (fabsf(delta) < fabsf(step)) {
        return target;
    }

    return current + step;
}

/* ------------------------------------------------------------------ */
/*  0x7c5c5  VEH_LerpValue                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c5c5, 8c5c5_FUN_0008c5c5.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
float VEH_LerpValue(float target, float current, float speed)
{
    const float delta = target - current;
#if EMULATE_X87
    const float step =
        x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(speed), x87f_load_f32(delta)), x87f_load_f32(VEH_APPROACH_FRAME_SECONDS)));
#else
    const float step = (float)((long double)speed * (long double)delta * (long double)VEH_APPROACH_FRAME_SECONDS);
#endif

    if (!(fabsf(delta) > VEH_APPROACH_EPSILON)) {
        return target;
    }

    if (fabsf(delta) < fabsf(step)) {
        return target;
    }

    return current + step;
}

/* ------------------------------------------------------------------ */
/*  0x7c63c  VEH_TrackAngle                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c63c, 8c63c_FUN_0008c63c.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
float VEH_TrackAngle(float target, float current, float speed)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (isinf(current)) {
        return 0.0f;
    }

    while (target - current > 180.0f) {
        const float previous = target;

        target -= 360.0f;
        if (target == previous) {
            break;
        }
    }

    while (target - current < -180.0f) {
        const float previous = target;

        target += 360.0f;
        if (target == previous) {
            break;
        }
    }

    return AngleNormalize180(VEH_TrackValue(target, current, speed));
}

/* ------------------------------------------------------------------ */
/*  0x7c6cf  VEH_LerpAngle                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c6cf, 8c6cf_FUN_0008c6cf.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
float VEH_LerpAngle(float target, float current, float speed)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (isinf(current)) {
        return 0.0f;
    }

    while (target - current > 180.0f) {
        const float previous = target;

        target -= 360.0f;
        if (target == previous) {
            break;
        }
    }

    while (target - current < -180.0f) {
        const float previous = target;

        target += 360.0f;
        if (target == previous) {
            break;
        }
    }

    return AngleNormalize180(VEH_LerpValue(target, current, speed));
}

/* ------------------------------------------------------------------ */
/*  0x7c762  VEH_SetPosition                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c762, 8c762_FUN_0008c762.c, VERIFY-VEHICLE-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void VEH_SetPosition(gentity_t *ent, const vec3_t origin, const vec3_t angles, const vec3_t velocity)
{
    (void)velocity;

    if (game_compat_veh_vector_changed(ent->currentOrigin, origin) || game_compat_veh_vector_changed(ent->s.pos.trBase, origin) ||
        game_compat_veh_vector_changed(ent->currentAngles, angles) || game_compat_veh_vector_changed(ent->s.apos.trBase, angles)) {
        G_SetOrigin(ent, origin);
        G_SetAngle(ent, angles);
        ent->s.pos.trType = TR_INTERPOLATE;
        ent->s.apos.trType = TR_INTERPOLATE;
        if (ent->linkedState != 0) {
            trap_LinkEntity(ent);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8c5ab  Scr_Vehicle_Init                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8c5ab, 9c5ab_Scr_Vehicle_Init.c, VERIFY-VEHICLE-INIT-WEAPON-2026-06-17): DATAFLOW_VERIFIED; wheel Z capture, flash/barrel distance, suspension gate, position stores, touch, and think scheduling audited. */
void Scr_Vehicle_Init(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_ALT_TRACE && vehClientThinkRecursing == 0) {
        ent->nextthink = level.time;
        return;
    }

    for (int wheel = 0; wheel < VEH_WHEEL_TAG_COUNT; wheel++) {
        DObjSkelMat wheelMatrix;

        if (G_DObjGetWorldTagMatrix(ent, vehicleWheelTagNames[wheel], &wheelMatrix) != 0) {
            vehicleState->wheelGroundZ[wheel] = wheelMatrix.origin[2];
        }
    }

    if (vehicleState->primaryFlashTagIndices[0] >= 0) {
        DObjSkelMat flashMatrix;

        G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryFlashTagIndices[0], &flashMatrix);
        if (vehicleState->primaryTurretTagIndex >= 0) {
            DObjSkelMat turretMatrix;

            G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryTurretTagIndex, &turretMatrix);
            vehicleState->viewState[0] = VectorDistance(turretMatrix.origin, flashMatrix.origin);
        }
    }

    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL || vehicleInfo->type == VEHICLE_TYPE_TANK) {
        VEH_UpdateSuspension(ent, qfalse);
    }

    VEH_SetPosition(ent, vehicleState->origin, vehicleState->viewClampTargetAngles, vec3_origin);

    vehicleState->previousOrigin[0] = vehicleState->origin[0];
    vehicleState->previousOrigin[1] = vehicleState->origin[1];
    vehicleState->previousOrigin[2] = vehicleState->origin[2];
    vehicleState->previousAngles[0] = vehicleState->viewClampTargetAngles[0];
    vehicleState->previousAngles[1] = vehicleState->viewClampTargetAngles[1];
    vehicleState->previousAngles[2] = vehicleState->viewClampTargetAngles[2];

    G_DoTouchTriggers(ent, ent->currentOrigin);
    ent->think = Scr_Vehicle_Think;
    ent->nextthink = level.time + VEH_ENTITY_THINK_INTERVAL_MS;
}

/* ------------------------------------------------------------------ */
/*  0x8c7cc  Scr_Vehicle_Think                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8c7cc, 9c7cc_Scr_Vehicle_Think.c, VERIFY-VEHICLE-INIT-WEAPON-2026-06-17): DATAFLOW_VERIFIED; passenger unlink loop, two-pass update/verify, heat decay, weapon/aim/body/sound calls, and reschedule audited. */
void Scr_Vehicle_Think(gentity_t *ent)
{
    static const float millisecondsPerSecond = 1000.0f;
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_ALT_TRACE && vehClientThinkRecursing == 0) {
        ent->nextthink = level.time;
        return;
    }

    VEH_Backup(ent);
    memset(&vehLastGroundTrace, 0, sizeof(vehLastGroundTrace));

    for (int slot = 1; slot < VEH_PASSENGER_SLOT_COUNT; slot++) {
        const int passengerNum = vehicleState->passengerEntityNums[slot];

        if (passengerNum != ENTITYNUM_NONE && g_entities[passengerNum].health < 1) {
            VEH_UnlinkPlayer(&g_entities[passengerNum], qfalse);
        }
    }

    for (int pass = 1; pass <= 2; pass++) {
        if (vehicleInfo->type != VEHICLE_TYPE_ARTILLERY) {
            if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_ALT_TRACE) {
                VEH_UpdateClient(ent);
            } else if (game_compat_veh_entity_collision_mode(ent) == 1u) {
                VEH_UpdatePath(ent);
            } else {
                VEH_UpdateClient(ent);
            }
        }

        if (vehicleInfo->type == VEHICLE_TYPE_ARTILLERY) {
            break;
        }

        if (VEH_VerifyPosition(ent, pass)) {
            memcpy(&vehicleState->previousPhysicsState, &vehicleState->origin, sizeof(vehicleState->previousPhysicsState));
            break;
        }

        memcpy(&vehicleState->previousPhysicsState, &vehicleState->origin, sizeof(vehicleState->previousPhysicsState));
    }

    if (vehicleState->lastSolidTime < level.time - 1000) {
        vehicleState->lastStableTime = level.time;
    }

    VEH_SetPosition(ent, vehicleState->origin, vehicleState->viewClampTargetAngles, vehicleState->velocity);
    G_DoTouchTriggers(ent, ent->currentOrigin);

    if (g_vehicleDebug.integer != 0) {
        VEH_DebugBox(vehicleState->origin, 4.0f, 1.0f, 1.0f, 0.0f);
    }

    game_compat_veh_set_turret_muzzle_back_active(vehicleState, 0);
    if (vehicleState->altHeat >= 1.0f && vehicleState->altOverheating == 0) {
        vehicleState->altOverheating = 1;
        if (vehicleInfo->type != VEHICLE_TYPE_4_WHEEL) {
            G_AddEvent(ent, EV_OVERHEATING, 0);
        }
    } else if (vehicleState->altOverheating != 0 && vehicleState->altHeat <= 0.5f) {
        vehicleState->altOverheating = 0;
    }

    if (vehicleInfo->turretAltWeapon[0] == '\0') {
        if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL) {
            if (vehicleState->altHeat > 0.0f) {
                vehicleState->altHeat -= 0.01f;
            } else {
                vehicleState->altHeat = 0.0f;
            }
        }
    } else {
        const weaponInfo_t *weaponInfo =
            (const weaponInfo_t *)BG_GetInfoForWeapon(BG_GetWeaponIndexForName(vehicleInfo->turretAltWeapon) & 0xff);

        if (vehicleState->altHeat > 0.0f) {
            vehicleState->altHeat -= weaponInfo->turretHeatDecay;
        } else {
            vehicleState->altHeat = 0.0f;
        }
    }

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_ALT_TRACE) {
        VEH_UpdateWeapon(ent);
    }

    VEH_UpdateGunnerWeapon(ent);
    VEH_UpdateAim(ent);
    VEH_UpdateGunnerAim(ent);
    VEH_UpdateBody(ent);
    VEH_UpdateSteering(ent);
    VEH_UpdateShaderTime(ent);
    VEH_UpdateSounds(ent);

    /*
     * Vehicle entities encode suspension travel in entity-state time2 as
     * milliseconds-style integer units: round(suspensionTravel * 1000).
     */
    /* 0x8cc1a: the product feeds fistp directly (no float store). */
#if EMULATE_X87
    ent->s.time2 = x87f_store_i32_trunc(x87f_mul(x87f_load_f32(vehicleInfo->suspensionTravel), x87f_load_f32(millisecondsPerSecond)));
#elif defined(__x86_64__)
    ent->s.time2 = CODUO_X87_TRUNCATE_I32((long double)vehicleInfo->suspensionTravel * (long double)millisecondsPerSecond);
#else
    ent->s.time2 = (int)(vehicleInfo->suspensionTravel * millisecondsPerSecond);
#endif
    if (g_vehicleTrafficStressTest.integer < 0) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const int stressCount = game_compat_veh_abs_int(g_vehicleTrafficStressTest.integer);

        if (vehicleState->slotIndex < stressCount && vehicleInfo->gunnerSeatEnabled != 0) {
            G_VEH_FireGunner((uint32_t)ent->s.number, qtrue);
        }
    }

    ent->nextthink = level.time + VEH_ENTITY_THINK_INTERVAL_MS;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_reset_think_turret_target(vehicle_state_t *vehicleState)
{
    game_compat_veh_set_turret_target_active(vehicleState, 0);
    vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
    vehicleState->motionControl[5] = 0.0f;
    vehicleState->motionControl[4] = 0.0f;
    vehicleState->motionControl[3] = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_copy_bone_matrix_origin(const DObjSkelMat *matrix, vec3_t origin)
{
    origin[0] = matrix->origin[0];
    origin[1] = matrix->origin[1];
    origin[2] = matrix->origin[2];
}

/* ------------------------------------------------------------------ */
/*  0x86c57  VEH_UpdateWeapon                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x86c57, 96c57_FUN_00096c57.c, VERIFY-VEHICLE-INIT-WEAPON-2026-06-17): DATAFLOW_VERIFIED; reset stores, driver weapon gate, notify timers, target trace, muzzle-back flag, and original Com_Error varargs audited. */
void VEH_UpdateWeapon(gentity_t *ent)
{
    static uint16_t turretFireString;
    static uint16_t turretAltFireString;

    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    gentity_t *passenger;
    gclient_t *passengerClient;
    vec3_t forward;
    vec3_t muzzle;
    vec3_t target;
    trace_t trace;

    game_compat_veh_reset_think_turret_target(vehicleState);

    if (ent->passEntityNum == ENTITYNUM_NONE || vehicleInfo->turretWeapon[0] == '\0') {
        return;
    }

    ent->s.weapon = BG_GetWeaponIndexForName(vehicleInfo->turretWeapon) & 0xff;
    if (ent->s.weapon == 0 || ent->vehiclePrimaryDisabled != 0) {
        return;
    }

    passenger = &g_entities[ent->passEntityNum];
    passengerClient = passenger->client;
    ent->s.headIcon = passenger->s.headIcon;
    ent->s.headIconTeam = passenger->s.headIconTeam;

    if ((passengerClient->ps.entityStateFlags & EF_VEHICLE_POPOUT) != 0) {
        return;
    }

    if (vehicleState->primaryFireTime < 1) {
        if ((passengerClient->command.buttons & VEH_CLIENT_FIRE_BUTTON) != 0) {
            if (turretFireString == 0) {
                turretFireString = game_compat_veh_script_notify_string("turret_fire");
            }
            Scr_Notify(ent, turretFireString, 0);
        }
    } else {
        vehicleState->primaryFireTime -= VEH_FIRE_TIME_THINK_STEP_MS;
    }

    vehicleState->altFireTime -= VEH_FIRE_TIME_THINK_STEP_MS;
    if (vehicleState->altFireTime < 1 && (passengerClient->command.buttons & VEH_CLIENT_ALT_FIRE_BUTTON) != 0) {
        if (turretAltFireString == 0) {
            turretAltFireString = game_compat_veh_script_notify_string("turret_alt_fire");
        }
        Scr_Notify(ent, turretAltFireString, 0);
    }

    game_compat_veh_set_turret_target_active(vehicleState, 1);

    if (vehicleState->primaryFlashTagIndices[0] < 0) {
        Com_Error(1, VEH_PLAYER_VEHICLE_NO_TAG_ERROR, vehiclePrimaryFlashTagNames[0]);
    }
    if (vehicleState->primaryTurretTagIndex < 0) {
        Com_Error(1, VEH_PLAYER_VEHICLE_NO_BARREL_ERROR);
    }

    DObjSkelMat flashMatrix;
    DObjSkelMat turretMatrix;
    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryFlashTagIndices[0], &flashMatrix);
    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryTurretTagIndex, &turretMatrix);

    AngleVectors(passengerClient->ps.viewAngles, forward, NULL, NULL);
    if (passengerClient->ps.adsFraction == 0.0f) {
        if (vehicleState->chasecamTagIndex < 0) {
            CalcMuzzlePoint(passenger, muzzle);
        } else {
            DObjSkelMat cameraMatrix;

            G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->chasecamTagIndex, &cameraMatrix);
            game_compat_veh_copy_bone_matrix_origin(&cameraMatrix, muzzle);
        }
    } else if (vehicleState->aimDownBarrelTagIndex < 0) {
        CalcMuzzlePoint(passenger, muzzle);
    } else {
        DObjSkelMat aimMatrix;

        G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->aimDownBarrelTagIndex, &aimMatrix);
        game_compat_veh_copy_bone_matrix_origin(&aimMatrix, muzzle);
    }

    target[0] = muzzle[0] + forward[0] * VEH_ALT_TRACE_TARGET_DISTANCE;
    target[1] = muzzle[1] + forward[1] * VEH_ALT_TRACE_TARGET_DISTANCE;
    target[2] = muzzle[2] + forward[2] * VEH_ALT_TRACE_TARGET_DISTANCE;

    vehicleState->motionControl[0] = target[0];
    vehicleState->motionControl[1] = target[1];
    vehicleState->motionControl[2] = target[2];

    trap_Trace(&trace, turretMatrix.origin, NULL, NULL, flashMatrix.origin, ent->s.number,
               ent->clipmask & (int32_t)~VEH_ALT_TRACE_CONTENTS_CLEAR);
    if (trace.fraction == 1.0f) {
        trap_LocationalTrace(&trace, muzzle, target, ent->passEntityNum, VEH_ALT_TRACE_LOCATIONAL_MASK, bulletPriorityMap);
        if (trace.fraction < 1.0f) {
            vehicleState->motionControl[0] = trace.endpos[0];
            vehicleState->motionControl[1] = trace.endpos[1];
            vehicleState->motionControl[2] = trace.endpos[2];
        }
    } else {
        game_compat_veh_set_turret_muzzle_back_active(vehicleState, 1);
    }
}

/* ------------------------------------------------------------------ */
/*  0x871a6  VEH_UpdateGunnerWeapon                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x871a6, 971a6_FUN_000971a6.c, VERIFY-VEHICLE-INIT-WEAPON-2026-06-17): DATAFLOW_VERIFIED; reset stores, overheat state/flag, gunner heat decay, passenger gate, cooldown, and notify path audited. */
void VEH_UpdateGunnerWeapon(gentity_t *ent)
{
    static uint16_t turretGunnerFireString;

    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    game_compat_veh_reset_think_turret_target(vehicleState);

    if (vehicleState->gunnerHeat >= 1.0f && vehicleState->gunnerOverheating == 0) {
        vehicleState->gunnerOverheating = 1;
        ent->s.turretOverheatState = 1;
        G_AddEvent(ent, EV_OVERHEATING, 0);
    } else if (vehicleState->gunnerOverheating != 0 && vehicleState->gunnerHeat <= 0.5f) {
        vehicleState->gunnerOverheating = 0;
        ent->s.turretOverheatState = 0;
    }

    if (vehicleInfo->turretGunnerWeapon[0] != '\0') {
        const weaponInfo_t *weaponInfo =
            (const weaponInfo_t *)BG_GetInfoForWeapon(BG_GetWeaponIndexForName(vehicleInfo->turretGunnerWeapon) & 0xff);

        if (vehicleState->gunnerHeat > 0.0f) {
            vehicleState->gunnerHeat -= weaponInfo->turretHeatDecay;
        } else {
            vehicleState->gunnerHeat = 0.0f;
        }
    }

    if (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_GUNNER] != ENTITYNUM_NONE) {
        gentity_t *gunner = &g_entities[vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_GUNNER]];
        gclient_t *gunnerClient = gunner->client;

        if (vehicleState->gunnerOverheating == 0) {
            gunnerClient->ps.playerStateFlags &= ~VEH_CLIENT_GUNNER_OVERHEAT_FLAG;
        } else {
            gunnerClient->ps.playerStateFlags |= VEH_CLIENT_GUNNER_OVERHEAT_FLAG;
            Scr_Notify(ent, scr_const_overheating, 0);
        }

        if ((gunnerClient->ps.entityStateFlags & EF_VEHICLE_POPOUT) == 0 && gunner->health > 0 &&
            gunnerClient->connectedState == CON_CONNECTED) {
            vehicleState->gunnerFireTime -= VEH_FIRE_TIME_THINK_STEP_MS;
            if (vehicleState->gunnerFireTime < 1 && (gunnerClient->command.buttons & VEH_CLIENT_FIRE_BUTTON) != 0) {
                if (turretGunnerFireString == 0) {
                    turretGunnerFireString = game_compat_veh_script_notify_string("turret_gunner_fire");
                }
                Scr_Notify(ent, turretGunnerFireString, 0);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x86a29  VEH_UpdateSteering                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x86a29, 96a29_FUN_00096a29.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; steer-wheel branch and clientInfoLeanFraction bit-pattern store checked. */
void VEH_UpdateSteering(gentity_t *ent)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    if (vehicleInfo->steerWheels == 0) {
        ent->s.clientInfoLeanFraction = 0.0f;
    } else {
        game_compat_veh_set_float_slot_int_bits(&ent->s.clientInfoLeanFraction,
                                                game_compat_veh_float_slot_int_bits(&vehicleState->steerAngle) ^ (int32_t)0x80000000u);
    }
}

/* ------------------------------------------------------------------ */
/*  0x86aa1  VEH_UpdateShaderTime                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x86aa1, 96aa1_FUN_00096aa1.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; texture-scroll branch, cvar fallback, NaN test, and cast-style time update checked. */
void VEH_UpdateShaderTime(gentity_t *ent)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    float scrollSpeed;
    float scrollScale;
    float scrollStep;

    if (vehicleInfo->textureScroll == 0) {
        ent->s.time = -1;
        return;
    }

    if (game_compat_veh_float_is_non_zero_or_nan(vehicleState->angularVelocity[0])) {
        scrollSpeed = vehicleState->angularVelocity[0];
    } else {
        scrollSpeed = vehicleState->acceleration[1];
    }

    scrollScale = g_vehicleTexScrollScale.value > 0.0f ? g_vehicleTexScrollScale.value : vehicleInfo->textureScrollScale;
    /* Stock 0x86b3c..0x86c4b (float DWORD constants, stepwise stores): each
     * scroll-step stage is its own float store, so the leading divide is the
     * only width-sensitive one (80-bit quotient rounded once at the store, vs
     * SSE's 32-bit divide) -> shim. Each *= is a single mul stored to a float
     * (native, docs rule 1). But the final *1000 product feeds fistp DIRECTLY
     * at 0x86c42 -- NOT spilled to a float -- so the 80-bit product is truncated
     * to int; our -mfpmath=387 build spills it (deviating), so force the 80-bit
     * product with a long double multiply (1000 is exact) and truncate it. */
#if EMULATE_X87
    scrollStep = x87f_store_f32(x87f_div(x87f_load_f32(scrollSpeed), x87f_load_f32(VEH_TEXTURE_SCROLL_SPEED_SCALE)));
    scrollStep *= VEH_PHYSICS_FRAME_SECONDS;
    scrollStep *= scrollScale;
    ent->s.time += x87f_store_i32_trunc(x87f_mul(x87f_load_f32(scrollStep), x87f_load_f32(1000.0f)));
#else
    scrollStep = scrollSpeed / VEH_TEXTURE_SCROLL_SPEED_SCALE;
    scrollStep *= VEH_PHYSICS_FRAME_SECONDS;
    scrollStep *= scrollScale;
    ent->s.time += (int)((long double)scrollStep * 1000.0f);
#endif
}

/* ------------------------------------------------------------------ */
/*  0x7c935  VEH_InitEntity                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7c935, 8c935_FUN_0008c935.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; callback setup, entity-state fields, precache checks, and RegisterItem calls checked. */
void VEH_InitEntity(gentity_t *ent, vehicle_state_t *vehicleState, int16_t vehicleInfoIndex)
{
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleInfoIndex);
    level_locals_t *lvl = &level;

    ent->think = Scr_Vehicle_Init;
    ent->pain = Scr_Vehicle_Pain;
    ent->die = Scr_Vehicle_Die;
    ent->touch = Scr_Vehicle_Touch;
    ent->use = Scr_Vehicle_Use;
    ent->controller = Scr_Vehicle_Controller;

    ent->svFlags = VEH_ENTITY_SVFLAGS;
    ent->scriptContents = VEH_ENTITY_CONTENTS;
    if ((ent->spawnflags & VEH_ENTITY_SPAWNFLAG_TRIGGER) != 0) {
        ent->scriptContents |= VEH_ENTITY_SPAWNFLAG_CONTENTS;
    }

    ent->s.eType = ET_VEHICLE;
    ent->s.eFlags = 0;
    ent->s.pos.trType = TR_INTERPOLATE;
    ent->s.apos.trType = TR_INTERPOLATE;
    ent->s.time = 0;
    ent->s.time2 = 0;
    game_compat_veh_set_entity_passenger_bits(ent, 0);
    ent->s.clientSound = 0;

    if (vehicleInfo->turretWeapon[0] != '\0') {
        ent->s.weapon = BG_GetWeaponIndexForName(vehicleInfo->turretWeapon) & 0xff;
    }

    ent->s.clientInfoLeanFraction = 0.0f;
    ent->s.vehicleType = vehicleInfo->type;
    game_compat_veh_set_float_slot_int_bits(&ent->s.loopedFxForward[2], 0);
    game_compat_veh_set_entity_gunner_yaw_base(ent, 0.0f);
    game_compat_veh_set_float_slot_int_bits(&ent->s.loopedFxForward[0], 0);
    game_compat_veh_set_float_slot_int_bits(&ent->s.loopedFxForward[0], VEH_ENTITY_DEFAULT_ANIM_VALUE);
    game_compat_veh_set_float_slot_int_bits(&ent->s.loopedFxForward[2], VEH_ENTITY_DEFAULT_ANIM_VALUE);
    ent->s.vehicleTurret.gunnerPitch = 0.0f;
    ent->s.vehicleTurret.primaryYaw = 0.0f;
    ent->s.vehicleTurret.primaryPitch = 0.0f;

    ent->vehicle = vehicleState;
    ent->nextthink = level.time + VEH_ENTITY_THINK_INTERVAL_MS;
    game_compat_veh_set_entity_usable_byte(ent, 1);
    ent->maxSpeed = 0;
    game_compat_veh_set_entity_collision_mode(ent, 0);
    ent->clipmask = VEH_ENTITY_CLIPMASK;
    ent->flags |= VEH_ENTITY_FLAG_LINKED;

    G_DObjUpdate(ent);
    trap_DObjGetBounds(ent, ent->mins, ent->maxs);
    trap_LinkEntity(ent);

    if (lvl->spawning == 0) {
        if (!IsItemRegistered(ent->s.weapon)) {
            Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_NOT_PRECACHED, vehicleInfo->name));
        }
        if (!IsItemRegistered(BG_GetWeaponIndexForName(vehicleInfo->turretAltWeapon) & 0xff)) {
            Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_NOT_PRECACHED, vehicleInfo->name));
        }
        if (!IsItemRegistered(BG_GetWeaponIndexForName(vehicleInfo->turretGunnerWeapon) & 0xff)) {
            Scr_Error(va(VEH_SCRIPT_ENTITY_ERROR_NOT_PRECACHED, vehicleInfo->name));
        }
    }

    RegisterItem(ent->s.weapon, qtrue);
    if (vehicleInfo->turretAltWeapon[0] != '\0') {
        RegisterItem(BG_GetWeaponIndexForName(vehicleInfo->turretAltWeapon) & 0xff, qtrue);
    }
    if (vehicleInfo->turretGunnerWeapon[0] != '\0') {
        RegisterItem(BG_GetWeaponIndexForName(vehicleInfo->turretGunnerWeapon) & 0xff, qtrue);
    }
}

/* ------------------------------------------------------------------ */
/*  0x7cd00  VEH_InitPhysics                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7cd00, 8cd00_VEH_InitPhysics.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; origin/angle copies, zeroed motion vectors, wheel arrays, and physics snapshot copy checked. */
void VEH_InitPhysics(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    memcpy(vehicleState->origin, ent->currentOrigin, sizeof(vehicleState->origin));
    memcpy(vehicleState->previousOrigin, ent->currentOrigin, sizeof(vehicleState->previousOrigin));
    memcpy(vehicleState->viewClampTargetAngles, ent->currentAngles, sizeof(vehicleState->viewClampTargetAngles));
    memcpy(vehicleState->previousAngles, ent->currentAngles, sizeof(vehicleState->previousAngles));

    memset(vehicleState->velocity, 0, sizeof(vehicleState->velocity));
    memset(vehicleState->angularVelocity, 0, sizeof(vehicleState->angularVelocity));
    memset(vehicleState->acceleration, 0, sizeof(vehicleState->acceleration));
    memset(vehicleState->angularAcceleration, 0, sizeof(vehicleState->angularAcceleration));
    memset(vehicleState->externalVelocity, 0, sizeof(vehicleState->externalVelocity));

    for (int index = 0; index < 6; index++) {
        vehicleState->wheelVerticalVelocity[index] = 0.0f;
        vehicleState->wheelGroundZ[index] = 0.0f;
        vehicleState->wheelMaterial[index] = 0.0f;
    }

    memcpy(&vehicleState->previousPhysicsState, &vehicleState->origin, sizeof(vehicleState->previousPhysicsState));
}

/* ------------------------------------------------------------------ */
/*  0x8ccad  Scr_Vehicle_DamageScale                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ccad, 9ccad_Scr_Vehicle_DamageScale.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; MOD set, axis dominance, grenade override, directional scale blend, and bullet multiplier checked. */
float Scr_Vehicle_DamageScale(gentity_t *vehicle, gentity_t *attacker, gentity_t *inflictor, const float *point, int mod)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)vehicle->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const float damageDirectionCosine = (float)CoduoLibm_Cos(VEH_DAMAGE_SCALE_ANGLE_RADIANS);
    axis_t axis;
    vec3_t delta = {point[0] - vehicleState->origin[0], point[1] - vehicleState->origin[1], 0.0f};
    float bulletScale = 1.0f;
    float dominantDot = 0.0f;
    int dominantAxis = -1;

    (void)attacker;

    if (mod == MOD_PISTOL_BULLET || mod == MOD_RIFLE_BULLET) {
        bulletScale = vehicleInfo->damageScaleBullet * VEH_DAMAGE_SCALE_PERCENT;
    }

    AnglesToAxis(vehicleState->viewClampTargetAngles, axis);
    VectorNormalize(delta);

    for (int axisIndex = 0; axisIndex < 2; axisIndex++) {
        const float dot = delta[0] * axis[axisIndex][0] + delta[1] * axis[axisIndex][1] + delta[2] * axis[axisIndex][2];

        if (dominantAxis < 0 || fabsf(dominantDot) < fabsf(dot)) {
            dominantAxis = axisIndex;
            dominantDot = dot;
        }
    }

    if (mod == MOD_GRENADE_SPLASH || mod == MOD_PROJECTILE_SPLASH || mod == MOD_MORTAR_SPLASH || mod == MOD_DYNAMITE_SPLASH ||
        mod == MOD_ARTILLERY_SPLASH) {
        float directScale = 1.0f;
        float blendScale = 1.0f;

        if (mod == MOD_GRENADE_SPLASH) {
            float weaponScale = 1.0f;
            const float radius = vehicle->maxs[1] * VEH_DAMAGE_SCALE_GRENADE_RADIUS_MULT;
            const float distanceSq = VectorDistanceSquared(point, vehicleState->origin);

            /* 0x8cfdb-0x8cfff: machine checks only inflictor != NULL, then
             * NULL-checks the returned weapon-info pointer (weapon 0 is
             * looked up too; there is no weapon != 0 pre-check). */
            if (inflictor != NULL) {
                const weaponInfo_t *weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(inflictor->s.weapon);

                if (weaponInfo != NULL) {
                    const float configuredScale = weaponInfo->grenadeSplashVehicleDamageScale;

                    if (configuredScale > 0.0f) {
                        weaponScale = configuredScale;
                    }
                }
            }

            if (!(vehicleState->origin[2] + vehicle->maxs[2] * 0.5f > point[2] - VEH_DAMAGE_SCALE_GRENADE_HEIGHT)) {
                return bulletScale * weaponScale;
            }
            if (!(radius * radius > distanceSq)) {
                return bulletScale * weaponScale;
            }

            return bulletScale * VEH_DAMAGE_SCALE_PERCENT * vehicleInfo->damageScaleTop * weaponScale;
        }

        if (dominantAxis == 0) {
            directScale = (dominantDot < 0.0f ? vehicleInfo->damageScaleRear : vehicleInfo->damageScaleFront) * VEH_DAMAGE_SCALE_PERCENT;
            blendScale = vehicleInfo->damageScaleSide * VEH_DAMAGE_SCALE_PERCENT;
        } else if (dominantAxis == 1) {
            const long double forwardDot = (long double)delta[0] * (long double)axis[0][0] +
                                           (long double)delta[1] * (long double)axis[0][1] +
                                           (long double)delta[2] * (long double)axis[0][2];

            directScale = vehicleInfo->damageScaleSide * VEH_DAMAGE_SCALE_PERCENT;
            blendScale = (forwardDot < 0.0L ? vehicleInfo->damageScaleRear : vehicleInfo->damageScaleFront) * VEH_DAMAGE_SCALE_PERCENT;
        }

        return ((1.0f - fabsf(dominantDot)) * blendScale + directScale * fabsf(dominantDot)) * bulletScale * VEH_DAMAGE_SCALE_RADIUS_BLEND;
    }

    if (dominantAxis == 0) {
        if (dominantDot < 0.0f) {
            if (-dominantDot < damageDirectionCosine) {
                return bulletScale * VEH_DAMAGE_SCALE_PERCENT * vehicleInfo->damageScaleSide;
            }

            return bulletScale * VEH_DAMAGE_SCALE_PERCENT * vehicleInfo->damageScaleRear;
        }

        return bulletScale * VEH_DAMAGE_SCALE_PERCENT * vehicleInfo->damageScaleFront;
    }

    if (dominantAxis == 1) {
        return bulletScale * VEH_DAMAGE_SCALE_PERCENT * vehicleInfo->damageScaleSide;
    }

    return 0.0f;
}

/* ------------------------------------------------------------------ */
/*  0x7cebf  VEH_InitVehicle                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7cebf, 8cebf_FUN_0008cebf.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; vehicle-state initialization, sound blend spawning/linking, weapon sound aliases, and final position set checked. */
void VEH_InitVehicle(gentity_t *ent, vehicle_state_t *vehicleState, int16_t vehicleInfoIndex)
{
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleInfoIndex);
    const weaponInfo_t *weaponInfo;
    const char *soundName;
    int weaponIndex;

    ent->s.vehicleEntityNum = ENTITYNUM_NONE;
    G_VehInitPathPos(game_compat_veh_path_cursor(vehicleState));
    VEH_InitPhysics(ent);

    vehicleState->entityNum = ent->s.number;
    vehicleState->typeIndex = vehicleInfoIndex;
    vehicleState->pathNodeIndex = -1;
    vehicleState->waitNodeSpeedThreshold = -1.0f;
    vehicleState->primaryFireTime = 0;
    vehicleState->primaryFlashSelector = 0;
    vehicleState->turretYaw = 0.0f;
    vehicleState->turretRoll = 0.0f;

    for (int index = 0; index < 7; index++) {
        vehicleState->viewState[index] = 0.0f;
    }

    vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
    for (int index = 0; index < 6; index++) {
        vehicleState->motionControl[index] = 0.0f;
    }

    vehicleState->localAccel.components.forward = 0.0f;
    vehicleState->localAccel.components.vertical = 0.0f;
    vehicleState->throttleScale = 0.0f;
    vehicleState->throttleScalePrevious = 0.0f;
    vehicleState->brakeScale = 0.0f;
    vehicleState->scriptedMaxSpeed = 0.0f;
    vehicleState->scriptedAcceleration = 0.0f;
    vehicleState->suspensionEnabled = 1;

    for (int index = 0; index < 4; index++) {
        vehicleState->soundBlendEntityNums[index] = ENTITYNUM_NONE;
    }
    vehicleState->idleSoundBlendRepeatDelay = 0.0f;
    vehicleState->runSoundBlendRepeatDelay = 0.0f;
    vehicleState->primaryAimSightTraceResult =
        0; /* +0x37c..+0x37f, size 0x04; VEH_UpdateAim uses it as trap_SightTrace output in the dormant turret_on_vistarget path. */
    vehicleState->collisionSweepFraction = 1.0f;
    vehicleState->cachedCollisionEntityNum = ENTITYNUM_NONE;
    vehicleState->cachedCollisionDistance = VEH_COLLISION_CACHE_DISTANCE_RESET;

    G_GetHintStringIndex(&vehicleState->hintStringIndex, vehicleInfo->hintString);

    for (int index = 0; index < 7; index++) {
        vehicleState->passengerEntityNums[index] = ENTITYNUM_NONE;
    }

    if (vehicleInfo->idleBlendSound0 != 0 && vehicleInfo->idleBlendSound1 != 0) {
        gentity_t *soundBlend = game_compat_veh_spawn_linked_sound_blend(ent);
        vehicleState->soundBlendEntityNums[0] = soundBlend->s.number;
    }

    if (vehicleInfo->runBlendSound0 != 0 && vehicleInfo->runBlendSound1 != 0) {
        gentity_t *soundBlend = game_compat_veh_spawn_linked_sound_blend(ent);
        vehicleState->soundBlendEntityNums[1] = soundBlend->s.number;
    }

    vehicleState->soundBlendEntityNums[2] = game_compat_veh_spawn_linked_sound_blend(ent)->s.number;
    vehicleState->soundBlendEntityNums[3] = game_compat_veh_spawn_linked_sound_blend(ent)->s.number;

    if (vehicleInfo->turretAltWeapon[0] != '\0') {
        weaponIndex = BG_GetWeaponIndexForName(vehicleInfo->turretAltWeapon) & 0xff;
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weaponIndex);

        soundName = weaponInfo->loopFireSound;
        if (soundName != NULL && soundName[0] != '\0') {
            vehicleState->altWeaponFireSound = G_SoundAliasIndex(soundName);
        }

        soundName = weaponInfo->stopFireSound;
        if (soundName != NULL && soundName[0] != '\0') {
            vehicleState->altWeaponStopSound = G_SoundAliasIndex(soundName);
        }
    }

    if (vehicleInfo->turretGunnerWeapon[0] != '\0') {
        weaponIndex = BG_GetWeaponIndexForName(vehicleInfo->turretGunnerWeapon) & 0xff;
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weaponIndex);

        soundName = weaponInfo->loopFireSound;
        if (soundName != NULL && soundName[0] != '\0') {
            vehicleState->gunnerWeaponFireSound = G_SoundAliasIndex(soundName);
        }

        soundName = weaponInfo->stopFireSound;
        if (soundName != NULL && soundName[0] != '\0') {
            vehicleState->gunnerWeaponStopSound = G_SoundAliasIndex(soundName);
        }
    }

    VEH_SetPosition(ent, ent->currentOrigin, ent->currentAngles, vec3_origin);
}

/* ------------------------------------------------------------------ */
/*  0x7d3e2  VEH_SetupCollmap                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7d3e2, 8d3e2_VEH_SetupCollmap.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; model lookup, empty-collmap warning, brush model setup, and contents bits checked. */
void VEH_SetupCollmap(gentity_t *ent)
{
    const char *modelName = G_ModelName(ent->modelIndex);
    gentity_t *collmapNode = VEH_FindVehicleNodeByTargetname(modelName);

    if (collmapNode == NULL) {
        return;
    }

    if (collmapNode->s.itemIndex == 0) {
        Com_Printf("WARNING: Cannot use empty vehicle collmap for [%s]\n", G_ModelName(ent->modelIndex));
        return;
    }

    ent->s.itemIndex = collmapNode->s.itemIndex;
    trap_SetBrushModel(ent);
    ent->scriptContents = VEH_COLLMAP_BASE_CONTENTS;
    ent->scriptContents |= VEH_USABLE_SCRIPT_CONTENTS;
}

/* ------------------------------------------------------------------ */
/*  0x7d492  VEH_UpdateScriptedInput                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7d492, 8d492_FUN_0008d492.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; input clamp, axis projection, 2D normalization, signed acceleration clamps, and scripted speed stores checked. */
void VEH_UpdateScriptedInput(gentity_t *ent, const vec3_t inputVector, float inputScale, float maxSpeedScale, float acceleration)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    axis_t axis;
    float clampedScale = game_compat_veh_clamp_float(inputScale, 0.0f, VEH_INPUT_SCALE_MAX);

    vehicleState->lastInputTime = level.time;
    AnglesToAxis(vehicleState->viewClampTargetAngles, axis);

    /* inputVector . axis-row dot: 3-mul/2-add chain kept 80-bit until the field
     * store -> shim (left-assoc (m0+m1)+m2). */
#if EMULATE_X87
    vehicleState->localAccel.components.forward =
        x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(inputVector[0]), x87f_load_f32(axis[0][0])),
                                         x87f_mul(x87f_load_f32(inputVector[1]), x87f_load_f32(axis[0][1]))),
                                x87f_mul(x87f_load_f32(inputVector[2]), x87f_load_f32(axis[0][2]))));
#else
    vehicleState->localAccel.components.forward = inputVector[0] * axis[0][0] + inputVector[1] * axis[0][1] + inputVector[2] * axis[0][2];
#endif
    /* 0x7d57e..0x7d5a8: the projection multiplies axis row 1, and
     * the negation is a sign-bit flip on the stored float (negate commutes with
     * round-to-nearest, so -store_f32(dot) matches). */
#if EMULATE_X87
    vehicleState->localAccel.components.vertical =
        -x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(inputVector[0]), x87f_load_f32(axis[1][0])),
                                          x87f_mul(x87f_load_f32(inputVector[1]), x87f_load_f32(axis[1][1]))),
                                 x87f_mul(x87f_load_f32(inputVector[2]), x87f_load_f32(axis[1][2]))));
#else
    vehicleState->localAccel.components.vertical =
        -(inputVector[0] * axis[1][0] + inputVector[1] * axis[1][1] + inputVector[2] * axis[1][2]);
#endif

    vehicleState->throttleScale = clampedScale * VEH_THROTTLE_SCALE_MULTIPLIER;
    vehicleState->throttleScalePrevious = vehicleState->throttleScale;
    vehicleState->brakeScale = 0.0f;

    VectorNormalize2D(vehicleState->localAccel.vector);
    /* inputScale * clampedScale * accel: 2-mul chain kept 80-bit until the
     * ClampAbs float arg -> shim ((a*b)*c). */
#if EMULATE_X87
    vehicleState->localAccel.components.forward = game_compat_veh_clamp_abs(
        x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(vehicleInfo->forwardInputScale), x87f_load_f32(clampedScale)),
                                x87f_load_f32(vehicleState->localAccel.components.forward))),
        VEH_SCRIPTED_INPUT_CLAMP);
    vehicleState->localAccel.components.vertical = game_compat_veh_clamp_abs(
        x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(vehicleInfo->verticalInputScale), x87f_load_f32(clampedScale)),
                                x87f_load_f32(vehicleState->localAccel.components.vertical))),
        VEH_SCRIPTED_INPUT_CLAMP);
#else
    vehicleState->localAccel.components.forward = game_compat_veh_clamp_abs(
        vehicleInfo->forwardInputScale * clampedScale * vehicleState->localAccel.components.forward, VEH_SCRIPTED_INPUT_CLAMP);
    vehicleState->localAccel.components.vertical = game_compat_veh_clamp_abs(
        vehicleInfo->verticalInputScale * clampedScale * vehicleState->localAccel.components.vertical, VEH_SCRIPTED_INPUT_CLAMP);
#endif

    /* 0x7d6e5: fld DWORD [ent+0x1f0] - the slot's float BITS are loaded
     * (the current-speed payload), not an integer conversion. */
    vehicleState->scriptedMaxSpeed = game_compat_veh_entity_current_speed(ent) * maxSpeedScale;
    vehicleState->scriptedAcceleration = acceleration;
}

/* ------------------------------------------------------------------ */
/*  0x8d3bc  Scr_Vehicle_PushAway                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8d3bc, 9d3bc_Scr_Vehicle_PushAway.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; push direction, distance cutoff, position test loop, trajectory base, and client origin stores checked. */
qboolean Scr_Vehicle_PushAway(gentity_t *player, gentity_t *vehicle)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)vehicle->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    vec3_t direction;
    vec3_t pushedOrigin;

    direction[0] = player->currentOrigin[0] - vehicle->currentOrigin[0];
    direction[1] = player->currentOrigin[1] - vehicle->currentOrigin[1];
    /* 0x8d427/0x8d43c: the origin delta is stored to direction[2] first,
     * then the mins delta is added in a second rounded store. */
    direction[2] = player->currentOrigin[2] - vehicle->currentOrigin[2];
    /* origin delta above is a single sub (native); the second store adds the
     * mins delta: (mins_p - mins_v) + direction[2] is sub+add kept 80-bit until
     * the store -> shim. */
#if EMULATE_X87
    direction[2] =
        x87f_store_f32(x87f_add(x87f_sub(x87f_load_f32(player->mins[2]), x87f_load_f32(vehicle->mins[2])), x87f_load_f32(direction[2])));
#else
    direction[2] = (player->mins[2] - vehicle->mins[2]) + direction[2];
#endif
    VectorNormalize(direction);

    pushedOrigin[0] = player->currentOrigin[0];
    pushedOrigin[1] = player->currentOrigin[1];
    pushedOrigin[2] = player->currentOrigin[2];

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (;;) {
        /* Stock 0x8d493..0x8d4b1: VectorDistance is called first, then
         * RADIUS+collisionMaxs[0]+PAD is summed on the x87 stack and compared
         * with a FULL-WIDTH fucompp -- the 80-bit sum is NOT spilled to a float.
         * The recon's inline form makes our -mfpmath=387 build evaluate the sum
         * first and spill it (fstp DWORD) -> a 32-bit compare that deviates; so
         * bind the distance to a float first (matches stock: no spill), and
         * emulate the compare at 80-bit width. */
        {
            const float pushDistance = VectorDistance(pushedOrigin, vehicle->currentOrigin);
#if EMULATE_X87
            if (!x87f_lt(x87f_load_f32(pushDistance),
                         x87f_add(x87f_add(x87f_load_f32(VEH_PUSH_AWAY_PLAYER_RADIUS), x87f_load_f32(vehicleInfo->collisionMaxs[0])),
                                  x87f_load_f32(kVehSafetyBuffer)))) {
                return qfalse;
            }
#else
            if (!(VEH_PUSH_AWAY_PLAYER_RADIUS + vehicleInfo->collisionMaxs[0] + kVehSafetyBuffer > pushDistance)) {
                return qfalse;
            }
#endif
        }

        /* pushedOrigin += direction*STEP: mul then add-into, 80-bit, one store */
#if EMULATE_X87
        for (int i = 0; i < 3; i++) {
            pushedOrigin[i] = x87f_store_f32(
                x87f_add(x87f_load_f32(pushedOrigin[i]), x87f_mul(x87f_load_f32(direction[i]), x87f_load_f32(VEH_PUSH_AWAY_STEP))));
        }
#else
        pushedOrigin[0] += direction[0] * VEH_PUSH_AWAY_STEP;
        pushedOrigin[1] += direction[1] * VEH_PUSH_AWAY_STEP;
        pushedOrigin[2] += direction[2] * VEH_PUSH_AWAY_STEP;
#endif

        if (G_TestEntityPosition(player, pushedOrigin) == NULL) {
            break;
        }
    }

    player->currentOrigin[0] = pushedOrigin[0];
    player->currentOrigin[1] = pushedOrigin[1];
    player->currentOrigin[2] = pushedOrigin[2];
    player->s.pos.trBase[0] = pushedOrigin[0];
    player->s.pos.trBase[1] = pushedOrigin[1];
    player->s.pos.trBase[2] = pushedOrigin[2];

    if (player->client != NULL) {
        player->client->ps.psOrigin[0] = pushedOrigin[0];
        player->client->ps.psOrigin[1] = pushedOrigin[1];
        player->client->ps.psOrigin[2] = pushedOrigin[2];
    }

    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x8d5a1  Scr_Vehicle_Touch                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8d5a1, 9d5a1_Scr_Vehicle_Touch.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; empty touch callback checked. */
void Scr_Vehicle_Touch(gentity_t *self, gentity_t *other, int traceMode)
{
    (void)self;
    (void)other;
    (void)traceMode;
}

/* ------------------------------------------------------------------ */
/*  0x8d5a6  VEH_GetMinsMaxs                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8d5a6, 9d5a6_VEH_GetMinsMaxs.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; collision bounds expansion and min/max padding constants checked. */
void VEH_GetMinsMaxs(gentity_t *ent, float *mins, float *maxs)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const float *padding;

    /* 0x8d5e9..0x8d77d: the scaled bounds are stored first, then the pad
     * vector (tv(-50,-50,0) / tv(50,50,20)) is added in a second rounded
     * store per component - including the mins[2] + 0.0f add. */
    mins[0] = vehicleInfo->collisionBoundsSource[0] * VEH_BOUNDS_EXPANSION_SCALE;
    mins[1] = vehicleInfo->collisionBoundsSource[1] * VEH_BOUNDS_EXPANSION_SCALE;
    mins[2] = vehicleInfo->collisionBoundsSource[2] * VEH_BOUNDS_EXPANSION_SCALE;
    padding = tv(VEH_BOUNDS_MINS_PAD_XY, VEH_BOUNDS_MINS_PAD_XY, VEH_BOUNDS_MINS_PAD_Z);
    mins[0] = mins[0] + padding[0];
    padding = tv(VEH_BOUNDS_MINS_PAD_XY, VEH_BOUNDS_MINS_PAD_XY, VEH_BOUNDS_MINS_PAD_Z);
    mins[1] = mins[1] + padding[1];
    padding = tv(VEH_BOUNDS_MINS_PAD_XY, VEH_BOUNDS_MINS_PAD_XY, VEH_BOUNDS_MINS_PAD_Z);
    mins[2] = mins[2] + padding[2];

    maxs[0] = vehicleInfo->collisionBoundsSource[3] * VEH_BOUNDS_EXPANSION_SCALE;
    maxs[1] = vehicleInfo->collisionBoundsSource[4] * VEH_BOUNDS_EXPANSION_SCALE;
    maxs[2] = vehicleInfo->collisionBoundsSource[5] * VEH_BOUNDS_EXPANSION_SCALE;
    padding = tv(VEH_BOUNDS_MAXS_PAD_XY, VEH_BOUNDS_MAXS_PAD_XY, VEH_BOUNDS_MAXS_PAD_Z);
    maxs[0] = maxs[0] + padding[0];
    padding = tv(VEH_BOUNDS_MAXS_PAD_XY, VEH_BOUNDS_MAXS_PAD_XY, VEH_BOUNDS_MAXS_PAD_Z);
    maxs[1] = maxs[1] + padding[1];
    padding = tv(VEH_BOUNDS_MAXS_PAD_XY, VEH_BOUNDS_MAXS_PAD_XY, VEH_BOUNDS_MAXS_PAD_Z);
    maxs[2] = maxs[2] + padding[2];
}

/* ------------------------------------------------------------------ */
/*  0x8d787  VEH_CheckPushClients                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8d787, 9d787_VEH_CheckPushClients.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; entity box, pass-entity skip, client filter, trace arguments, and touch condition checked. */
void VEH_CheckPushClients(gentity_t *vehicle)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)vehicle->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    vec3_t mins;
    vec3_t maxs;
    int entityNums[VEH_CHECK_PUSH_MAX_CLIENTS];
    int entityCount;

    /* 0x8d7cb..0x8d8a7: the origin+bounds sums are stored first (unrolled),
     * then the pad is applied in a second rounded store per component. */
    mins[0] = vehicle->currentOrigin[0] + vehicleInfo->collisionMins[0];
    mins[1] = vehicle->currentOrigin[1] + vehicleInfo->collisionMins[1];
    mins[2] = vehicle->currentOrigin[2] + vehicleInfo->collisionMins[2];
    maxs[0] = vehicle->currentOrigin[0] + vehicleInfo->collisionMaxs[0];
    maxs[1] = vehicle->currentOrigin[1] + vehicleInfo->collisionMaxs[1];
    maxs[2] = vehicle->currentOrigin[2] + vehicleInfo->collisionMaxs[2];
    for (int axis = 0; axis < 3; axis++) {
        mins[axis] = mins[axis] - kVehSafetyBuffer;
        maxs[axis] = maxs[axis] + kVehSafetyBuffer;
    }

    entityCount = trap_EntitiesInBox(mins, maxs, entityNums, VEH_CHECK_PUSH_MAX_CLIENTS, CONTENTS_BODY);
    for (int index = 0; index < entityCount; index++) {
        trace_t trace;
        gentity_t *clientEnt;

        if (vehicle->passEntityNum == entityNums[index]) {
            continue;
        }

        clientEnt = &g_entities[entityNums[index]];
        if (clientEnt->client == NULL) {
            continue;
        }

        trap_Trace(&trace, clientEnt->currentOrigin, clientEnt->mins, clientEnt->maxs, clientEnt->currentOrigin, clientEnt->s.number,
                   clientEnt->clipmask);
        if ((trace.startsolid != 0 || trace.allsolid != 0) && trace.entityNum == vehicle->s.number) {
            Scr_Vehicle_Touch(vehicle, clientEnt, 1);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8da1e  G_VEH_CycleSlot                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8da1e, 9da1e_G_VEH_CycleSlot.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; next-slot probe, tank retry reset, delay command throttle, unlink/relink order, and relink cooldown checked. */
void G_VEH_CycleSlot(gentity_t *player, int previous)
{
    gclient_t *client = player->client;
    gentity_t *vehicle;
    vehicle_state_t *vehicleState;
    int currentSlot;
    int nextSlot;
    int exitState;

    vehicle = &g_entities[player->passEntityNum];
    vehicleState = (vehicle_state_t *)vehicle->vehicle;
    if (VEH_FindNextPassengerSlot(vehicleState, 0, qfalse) == 0) {
        return;
    }

    if (game_compat_veh_get_vehicle_info(vehicleState->typeIndex)->type == VEHICLE_TYPE_TANK && client->vehicleExitState == 0) {
        client->vehicleControlTime = 0;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (client->vehicleControlTime > level.time - VEH_CYCLE_SLOT_RETRY_MS) {
        if (client->vehicleDelayIgnoreTime < level.time - VEH_CYCLE_SLOT_COMMAND_REPEAT_MS) {
            trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0, va(VEH_CLIENT_COMMAND_DELAY_IGNORE));
            client->vehicleDelayIgnoreTime = level.time;
        }
        return;
    }

    currentSlot = client->ps.vehiclePosition;
    client->vehicleControlTime = 0;
    exitState = client->vehicleExitState;
    client->vehicleExitState = exitState + 1;
    VEH_UnlinkPlayer(player, 0);

    nextSlot = VEH_FindNextPassengerSlot(vehicleState, currentSlot, previous != 0);
    client->vehicleControlTime = 0;
    VEH_LinkPlayer(vehicle, player, nextSlot, qtrue);
    client->vehicleControlTime = level.time + VEH_CYCLE_SLOT_RELINK_DELAY_MS;
}

/* ------------------------------------------------------------------ */
/*  0x8dc43  Scr_Vehicle_Use                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8dc43, 9dc43_Scr_Vehicle_Use.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; client guard, link/unlink branch, and exit-state reset checked. */
void Scr_Vehicle_Use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
    gentity_t *player = other;

    (void)activator;

    if (player->client == NULL) {
        return;
    }

    if ((player->client->ps.entityStateFlags & EF_IN_VEHICLE) == 0) {
        VEH_LinkPlayer(self, player, 0, qfalse);
        player->client->vehicleExitState = 0;
    } else {
        VEH_UnlinkPlayer(player, 1);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8dccb  Scr_Vehicle_Die                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8dccb, 9dccb_Scr_Vehicle_Die.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; passenger damage/eject loop, keep-vehicle flag, origin raise, and projectile/grenade impulse checked. */
void Scr_Vehicle_Die(gentity_t *ent, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                     int hitLocation)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    (void)inflictor;
    (void)damage;
    (void)mod;
    (void)weapon;
    (void)hitLocation;

    for (int slot = 1; slot < VEH_PASSENGER_SLOT_COUNT; slot++) {
        int passengerNum = vehicleState->passengerEntityNums[slot];
        gentity_t *passenger;
        int oldHealth;
        int keepVehicle;
        vec3_t raisedOrigin;

        if (passengerNum == ENTITYNUM_NONE) {
            continue;
        }

        passenger = &g_entities[passengerNum];
        if (passenger->client == NULL) {
            continue;
        }

        oldHealth = passenger->health;
        passenger->health += VEH_DIE_PASSENGER_DAMAGE_BOOST;
        G_Damage(passenger, attacker, attacker, vec3_origin, passenger->client->ps.psOrigin, VEH_DIE_PASSENGER_DAMAGE, 0, MOD_EXPLOSIVE, 0);
        keepVehicle = passenger->health == oldHealth + VEH_DIE_PASSENGER_DAMAGE_BOOST;
        passenger->health = oldHealth;
        VEH_UnlinkPlayer(passenger, keepVehicle);

        raisedOrigin[0] = passenger->currentOrigin[0];
        raisedOrigin[1] = passenger->currentOrigin[1];
        raisedOrigin[2] = passenger->currentOrigin[2] + VEH_DIE_PASSENGER_Z_RAISE;
        G_SetOrigin(passenger, raisedOrigin);
    }

    if (attacker != NULL && attacker->s.weapon != 0) {
        const weaponInfo_t *weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(attacker->s.weapon);

        if (weaponInfo->weaponType == WEAPTYPE_PROJECTILE || weaponInfo->weaponType == WEAPTYPE_GRENADE) {
            VEH_UpdateScriptedInput(ent, dir, 1.0f, 0.0f, 0.0f);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8dec6  SP_script_vehicle                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8dec6, 9dec6_SP_script_vehicle.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; delay spawnvar, spawn-var backup gate, string conversion, and spawn-map flag propagation checked. */
void SP_script_vehicle(gentity_t *ent)
{
    const char *delayDefault = g_vehicleBurnTime.string;
    const char *vehicleName;

    G_SpawnFloat("delay", delayDefault, &ent->concussiveFxEndTime);
    if (level.spawningMapEntities != 0) {
        G_BackupSpawnVars(ent);
    }

    vehicleName = SL_ConvertToString(ent->vehicleSpawnName);
    G_SpawnVehicle(ent, vehicleName, level.spawningMapEntities);
}

/* ------------------------------------------------------------------ */
/*  0x8df52  SP_script_vehicle_collmap                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8df52, 9df52_SP_script_vehicle_collmap.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; contents clear and entity type store checked. */
void SP_script_vehicle_collmap(gentity_t *ent)
{
    ent->scriptContents = 0;
    ent->s.eType = VEH_COLLMAP_ENTITY_TYPE;
}

/* ------------------------------------------------------------------ */
/*  0x8df6e  ScriptVehicle_GetMethod                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8df6e, 9df6e_ScriptVehicle_GetMethod.c, VERIFY-VEHICLE-SETUP-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED; 31-entry scan, canonical name store, callback return, and null miss return checked. */
script_method_callback_t ScriptVehicle_GetMethod(const char **name)
{
    for (uint32_t index = 0; index < sizeof(scriptVehicleMethods) / sizeof(scriptVehicleMethods[0]); index++) {
        if (strcmp(*name, scriptVehicleMethods[index].name) == 0) {
            *name = scriptVehicleMethods[index].name;
            return scriptVehicleMethods[index].callback;
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  0x8dfe9  CMD_VEH_AttachPath                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8dfe9, 9dfe9_CMD_VEH_AttachPath.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_AttachPath(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    int16_t nodeIndex = GScr_GetVehicleNodeIndex(0);

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    G_VehSetUpPathPos(game_compat_veh_path_cursor(vehicleState), nodeIndex);

    for (int axis = 0; axis < 3; axis++) {
        vehicleState->origin[axis] = game_compat_veh_path_cursor(vehicleState)->origin[axis];
        vehicleState->viewClampTargetAngles[axis] = game_compat_veh_path_cursor(vehicleState)->currentAngles[axis];
    }

    VEH_SetPosition(ent, vehicleState->origin, vehicleState->viewClampTargetAngles, vec3_origin);

    for (int axis = 0; axis < 3; axis++) {
        vehicleState->previousOrigin[axis] = vehicleState->origin[axis];
        vehicleState->previousAngles[axis] = vehicleState->viewClampTargetAngles[axis];
    }

    for (int wheel = 0; wheel < VEH_WHEEL_TAG_COUNT; wheel++) {
        DObjSkelMat tagMatrix;

        if (G_DObjGetWorldTagMatrix(ent, vehicleWheelTagNames[wheel], &tagMatrix) != 0) {
            vehicleState->wheelGroundZ[wheel] = tagMatrix.origin[2];
        }
    }

    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL || vehicleInfo->type == VEHICLE_TYPE_TANK) {
        VEH_UpdateSuspension(ent, qfalse);
    }

    VEH_SetPosition(ent, vehicleState->origin, vehicleState->viewClampTargetAngles, vec3_origin);

    for (int axis = 0; axis < 3; axis++) {
        vehicleState->previousOrigin[axis] = vehicleState->origin[axis];
        vehicleState->previousAngles[axis] = vehicleState->viewClampTargetAngles[axis];
    }
}

/* ------------------------------------------------------------------ */
/*  0x8e21c  CMD_VEH_StartPath                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e21c, 9e21c_CMD_VEH_StartPath.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_StartPath(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    if (game_compat_veh_path_cursor(vehicleState)->nodeIndex < 0) {
        Scr_Error(va(VEH_PATH_NOT_ATTACHED_ERROR));
    }

    game_compat_veh_set_entity_collision_mode(ent, VEH_PATH_ACTIVE_MODE);
}

/* ------------------------------------------------------------------ */
/*  0x8e299  CMD_VEH_SetSwitchNode                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e299, 9e299_CMD_VEH_SetSwitchNode.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_SetSwitchNode(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    int16_t fromNodeIndex = GScr_GetVehicleNodeIndex(0);
    int16_t toNodeIndex = GScr_GetVehicleNodeIndex(1);

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    G_VehSetSwitchNode(game_compat_veh_path_cursor(vehicleState), fromNodeIndex, toNodeIndex);
}

/* ------------------------------------------------------------------ */
/*  0x8e328  CMD_VEH_SetWaitNode                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e328, 9e328_CMD_VEH_SetWaitNode.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_SetWaitNode(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    vehicleState->pathNodeIndex = GScr_GetVehicleNodeIndex(0);
}

/* ------------------------------------------------------------------ */
/*  0x8e394  CMD_VEH_SetWaitSpeed                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e394, 9e394_CMD_VEH_SetWaitSpeed.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_SetWaitSpeed(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    vehicleState->waitNodeSpeedThreshold = Scr_GetFloat(0) * VEH_WAIT_SPEED_SCALE;
    if (vehicleState->waitNodeSpeedThreshold < 0.0f) {
        Scr_ParamError(0, VEH_WAIT_SPEED_NEGATIVE_ERROR);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8e435  CMD_VEH_SetSpeed                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e435, 9e435_CMD_VEH_SetSpeed.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_SetSpeed(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    game_compat_veh_set_path_speed_mode(vehicleState, VEH_PATH_SPEED_MODE_SCRIPT);
    vehicleState->viewState[3] = Scr_GetFloat(0) * VEH_WAIT_SPEED_SCALE;
    vehicleState->viewState[4] = Scr_GetFloat(1) * VEH_WAIT_SPEED_SCALE;

    if (vehicleState->viewState[3] < 0.0f) {
        Scr_ParamError(0, VEH_SET_SPEED_NEGATIVE_ERROR);
    }
    if (vehicleState->viewState[4] < 0.0f) {
        Scr_ParamError(1, VEH_SET_ACCELERATION_NEGATIVE_ERROR);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8e52e  CMD_VEH_ResumeSpeed                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e52e, 9e52e_CMD_VEH_ResumeSpeed.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_ResumeSpeed(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_USED) {
        Scr_Error(va(VEH_PATH_ALREADY_USED_ERROR));
    }

    game_compat_veh_set_path_speed_mode(vehicleState, VEH_PATH_SPEED_MODE_PATH);
    vehicleState->viewState[4] = Scr_GetFloat(0) * VEH_WAIT_SPEED_SCALE;

    if (vehicleState->viewState[4] < 0.0f) {
        Scr_ParamError(0, VEH_SET_ACCELERATION_NEGATIVE_ERROR);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8e5dc  CMD_VEH_JoltBody                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e5dc, 9e5dc_CMD_VEH_JoltBody.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_JoltBody(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    const uint32_t paramCount = Scr_GetNumParam();
    vec3_t source;
    vec3_t inputVector;
    float inputScale;
    float speedFraction = 0.0f;
    float deceleration = 0.0f;

    Scr_GetVector(0, source);
    inputScale = Scr_GetFloat(1);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (paramCount >= 3) {
        speedFraction = Scr_GetFloat(2);
        if (speedFraction < 0.0f || speedFraction > 1.0f) {
            Scr_ParamError(2, VEH_JOLT_SPEED_FRACTION_ERROR);
        }

        deceleration = Scr_GetFloat(3) * VEH_WAIT_SPEED_SCALE;
        if (deceleration < 0.0f) {
            Scr_ParamError(3, VEH_JOLT_DECELERATION_NEGATIVE_ERROR);
        }
    }

    for (int axis = 0; axis < 3; axis++) {
        inputVector[axis] = ent->currentOrigin[axis] - source[axis];
    }
    VectorNormalize(inputVector);

    VEH_UpdateScriptedInput(ent, inputVector, inputScale, speedFraction, deceleration);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint32_t game_compat_veh_wheel_index_for_const_string(uint16_t wheelName)
{
    if (wheelName == scr_const_front_left) {
        return 0;
    }
    if (wheelName == scr_const_front_right) {
        return 1;
    }
    if (wheelName == scr_const_back_left) {
        return 2;
    }
    if (wheelName == scr_const_back_right) {
        return 3;
    }
    if (wheelName == scr_const_middle_left) {
        return 4;
    }
    if (wheelName == scr_const_middle_right) {
        return 5;
    }

    Scr_ParamError(0, VEH_WHEEL_SURFACE_VALID_NAMES_ERROR);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  0x8e72f  CMD_VEH_FreeVehicle                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e72f, 9e72f_CMD_VEH_FreeVehicle.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_FreeVehicle(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    G_FreeVehicle(ent);
    ent->s.eType = VEH_FREE_ENTITY_TYPE;
    Scr_SetString(game_compat_veh_entity_model_classname_slot(ent), scr_const_script_vehicle_corpse);
    Scr_Notify(ent, scr_const_death, 0);
    Scr_FreeEntityNum(ent->s.number, 0);
}

/* ------------------------------------------------------------------ */
/*  0x8e7c1  CMD_VEH_GetWheelSurface                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e7c1, 9e7c1_CMD_VEH_GetWheelSurface.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetWheelSurface(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const uint16_t wheelName = Scr_GetConstString(0);
    uint32_t wheelIndex;
    int32_t surfaceType;

    if (vehicleInfo->type != VEHICLE_TYPE_TANK && vehicleInfo->type != VEHICLE_TYPE_4_WHEEL) {
        Scr_Error(va(VEH_WHEEL_SURFACE_NO_WHEELS_ERROR, vehicleInfo->name));
    }

    wheelIndex = game_compat_veh_wheel_index_for_const_string(wheelName);
    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL && wheelIndex >= VEH_SUSPENSION_WHEEL_COUNT_4WHEEL) {
        Scr_ParamError(0, VEH_WHEEL_SURFACE_NO_MIDDLE_ERROR);
    }

    surfaceType = game_compat_veh_wheel_surface_at(vehicleState, wheelIndex);
    if (surfaceType == 0) {
        Scr_AddString(VEH_WHEEL_SURFACE_DEFAULT);
    } else {
        Scr_AddString(trap_SurfaceTypeToName(surfaceType));
    }
}

/* ------------------------------------------------------------------ */
/*  0x8e97f  CMD_VEH_GetSpeedMPH                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e97f, 9e97f_CMD_VEH_GetSpeedMPH.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetSpeedMPH(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    Scr_AddFloat(game_compat_veh_entity_current_speed(ent) / VEH_INFO_SPEED_SCALE);
}

/* ------------------------------------------------------------------ */
/*  0x8e9cb  CMD_VEH_GetVehicleOwner                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8e9cb, 9e9cb_CMD_VEH_GetVehicleOwner.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetVehicleOwner(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    if (ent->passEntityNum != ENTITYNUM_NONE) {
        Scr_AddEntity(&g_entities[ent->passEntityNum]);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8ea21  CMD_VEH_MakeVehicleUsable                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ea21, 9ea21_CMD_VEH_MakeVehicleUsable.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_MakeVehicleUsable(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    ent->spawnflags |= VEH_ENTITY_SPAWNFLAG_TRIGGER;
    ent->scriptContents |= VEH_USABLE_SCRIPT_CONTENTS;
}

/* ------------------------------------------------------------------ */
/*  0x8ea63  CMD_VEH_MakeVehicleUnusable                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ea63, 9ea63_CMD_VEH_MakeVehicleUnusable.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_MakeVehicleUnusable(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    if (ent->passEntityNum != ENTITYNUM_NONE) {
        Scr_Error(va(VEH_ALREADY_IN_USE_ERROR));
    }

    ent->spawnflags &= ~VEH_ENTITY_SPAWNFLAG_TRIGGER;
    ent->scriptContents &= ~VEH_USABLE_SCRIPT_CONTENTS;
}

/* ------------------------------------------------------------------ */
/*  0x8eada  CMD_VEH_AddVehicleToCompass                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8eada, 9eada_CMD_VEH_AddVehicleToCompass.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_AddVehicleToCompass(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    game_compat_veh_set_compass_visible((vehicle_state_t *)ent->vehicle, 1);
}

/* ------------------------------------------------------------------ */
/*  0x8eb09  CMD_VEH_RemoveVehicleFromCompass                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8eb09, 9eb09_CMD_VEH_RemoveVehicleFromCompass.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_RemoveVehicleFromCompass(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    game_compat_veh_set_compass_visible((vehicle_state_t *)ent->vehicle, 0);
}

/* ------------------------------------------------------------------ */
/*  0x8eb38  CMD_VEH_StartEngineSound                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8eb38, 9eb38_CMD_VEH_StartEngineSound.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_StartEngineSound(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    game_compat_veh_set_engine_sound_active((vehicle_state_t *)ent->vehicle, 1);
}

/* ------------------------------------------------------------------ */
/*  0x8eb67  CMD_VEH_StopEngineSound                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8eb67, 9eb67_CMD_VEH_StopEngineSound.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_StopEngineSound(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);

    game_compat_veh_set_engine_sound_active((vehicle_state_t *)ent->vehicle, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_check_turret_control_health(const gentity_t *ent)
{
    if (ent->health <= 0) {
        Scr_Error(va(VEH_TURRET_HEALTH_ERROR));
    }
}

/* ------------------------------------------------------------------ */
/*  0x8eb96  CMD_VEH_SetTurretTargetVec                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8eb96, 9eb96_CMD_VEH_SetTurretTargetVec.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_SetTurretTargetVec(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    vec3_t target;

    game_compat_veh_check_turret_control_health(ent);
    game_compat_veh_set_turret_target_active(vehicleState, 1);
    vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
    Scr_GetVector(0, target);
    vehicleState->motionControl[0] = target[0];
    vehicleState->motionControl[1] = target[1];
    vehicleState->motionControl[2] = target[2];
    vehicleState->motionControl[3] = 0.0f;
    vehicleState->motionControl[4] = 0.0f;
    vehicleState->motionControl[5] = 0.0f;
}

/* ------------------------------------------------------------------ */
/*  0x8ec6e  CMD_VEH_SetTurretTargetEnt                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ec6e, 9ec6e_CMD_VEH_SetTurretTargetEnt.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_SetTurretTargetEnt(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    gentity_t *targetEnt;
    vec3_t offset;

    game_compat_veh_check_turret_control_health(ent);
    targetEnt = Scr_GetEntity(0);
    game_compat_veh_set_turret_target_active(vehicleState, 1);
    vehicleState->gunnerEntityNum = targetEnt != NULL ? targetEnt->s.number : ENTITYNUM_NONE;
    Scr_GetVector(1, offset);
    vehicleState->motionControl[0] = 0.0f;
    vehicleState->motionControl[1] = 0.0f;
    vehicleState->motionControl[2] = 0.0f;
    vehicleState->motionControl[3] = offset[0];
    vehicleState->motionControl[4] = offset[1];
    vehicleState->motionControl[5] = offset[2];
}

/* ------------------------------------------------------------------ */
/*  0x8ed78  CMD_VEH_ClearTurretTarget                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ed78, 9ed78_CMD_VEH_ClearTurretTarget.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_ClearTurretTarget(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    game_compat_veh_set_turret_target_active(vehicleState, 0);
    vehicleState->gunnerEntityNum = ENTITYNUM_NONE;
    for (int axis = 0; axis < 3; axis++) {
        vehicleState->motionControl[axis] = 0.0f;
        vehicleState->motionControl[3 + axis] = 0.0f;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static const weaponInfo_t *game_compat_veh_set_turret_weapon(gentity_t *ent, const char weaponName[0x040])
{
    const int weaponIndex = BG_GetWeaponIndexForName(weaponName) & 0xff;

    ent->s.weapon = weaponIndex;
    if (ent->s.weapon == 0) {
        Scr_Error(va(VEH_TURRET_NO_WEAPON_ERROR, SL_ConvertToString(ent->targetname)));
    }

    return (const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_check_turret_weapon_type(const weaponInfo_t *weaponInfo)
{
    const int weaponType = weaponInfo->weaponType;

    if (weaponType != WEAPTYPE_BULLET && weaponType != WEAPTYPE_PROJECTILE) {
        Scr_Error(va(VEH_TURRET_WEAPON_TYPE_ERROR));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_check_turret_barrel_bone(gentity_t *ent, int boneIndex)
{
    if (boneIndex < 0) {
        Scr_Error(va(VEH_TURRET_NO_BARREL_ERROR, SL_ConvertToString(ent->targetname)));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_build_turret_muzzle(gentity_t *ent, vehicle_state_t *vehicleState, const weaponInfo_t *weaponInfo,
                                                const char *tagName, qboolean allowTargetCorrection, weapon_muzzle_t *muzzle)
{
    DObjSkelMat tagMatrix = {0};
    vec3_t forward;
    vec3_t tagAngles;
    vec3_t targetAngles;
    vec3_t deltaAngles;

    if (G_DObjGetWorldTagMatrix(ent, tagName, &tagMatrix) == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: forward the completed warning as data
         * through a literal conversion. */
        G_Printf("%s", va(VEH_TURRET_NO_FLASH_WARNING, tagName, SL_ConvertToString(ent->targetname)));
    }

    forward[0] = tagMatrix.axis[0][0];
    forward[1] = tagMatrix.axis[0][1];
    forward[2] = tagMatrix.axis[0][2];

    if (allowTargetCorrection) {
        forward[0] = vehicleState->motionControl[0] - tagMatrix.origin[0];
        forward[1] = vehicleState->motionControl[1] - tagMatrix.origin[1];
        forward[2] = vehicleState->motionControl[2] - tagMatrix.origin[2];
        VectorNormalize(forward);

        vectoangles(tagMatrix.axis[0], tagAngles);
        vectoangles(forward, targetAngles);
        AnglesSubtract(tagAngles, targetAngles, deltaAngles);
        deltaAngles[0] = game_compat_veh_clamp_abs(deltaAngles[0], VEH_TURRET_AIM_PITCH_CLAMP);
        deltaAngles[1] = game_compat_veh_clamp_abs(deltaAngles[1], VEH_TURRET_AIM_YAW_CLAMP);
        deltaAngles[2] = 0.0f;
        AnglesSubtract(tagAngles, deltaAngles, tagAngles);
        AngleVectors(tagAngles, forward, NULL, NULL);
    }

    muzzle->forward[0] = forward[0];
    muzzle->forward[1] = forward[1];
    muzzle->forward[2] = forward[2];
    muzzle->right[0] = tagMatrix.axis[1][0];
    muzzle->right[1] = tagMatrix.axis[1][1];
    muzzle->right[2] = tagMatrix.axis[1][2];
    muzzle->up[0] = tagMatrix.axis[2][0];
    muzzle->up[1] = tagMatrix.axis[2][1];
    muzzle->up[2] = tagMatrix.axis[2][2];

    if (game_compat_veh_turret_muzzle_back_active(vehicleState) == 0) {
        muzzle->origin[0] = tagMatrix.origin[0];
        muzzle->origin[1] = tagMatrix.origin[1];
        muzzle->origin[2] = tagMatrix.origin[2];
    } else {
        const float back = vehicleState->viewState[0];

        /* origin[i] = tagMatrix[12+i] - back*forward[i]: mul then sub, 80-bit,
         * one store per component -> shim. */
#if EMULATE_X87
        for (int i = 0; i < 3; i++) {
            muzzle->origin[i] =
                x87f_store_f32(x87f_sub(x87f_load_f32(tagMatrix.origin[i]), x87f_mul(x87f_load_f32(back), x87f_load_f32(forward[i]))));
        }
#else
        muzzle->origin[0] = tagMatrix.origin[0] - back * forward[0];
        muzzle->origin[1] = tagMatrix.origin[1] - back * forward[1];
        muzzle->origin[2] = tagMatrix.origin[2] - back * forward[2];
#endif
    }

    muzzle->extraVector[0] = 0.0f; /* +0x30..+0x33, size 0x04; generated 6a8b0/6aaa9 keep this as the muzzle packet's extra vector. */
    muzzle->extraVector[1] = 0.0f; /* +0x34..+0x37, size 0x04; generated 6a8b0/6aaa9 keep this as the muzzle packet's extra vector. */
    muzzle->extraVector[2] = 0.0f; /* +0x38..+0x3b, size 0x04; generated 6a8b0/6aaa9 keep this as the muzzle packet's extra vector. */
    muzzle->weaponInfo = weaponInfo;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_fire_turret_muzzle(gentity_t *ent, gentity_t *attacker, const weaponInfo_t *weaponInfo,
                                                   weapon_muzzle_t *muzzle)
{
    const int weaponType = weaponInfo->weaponType;

    if (weaponType == WEAPTYPE_BULLET) {
        /* Draw spread from the selected server domain, then apply the original
         * max-spread and hip-spread arithmetic. */
#if EMULATE_X87
        const float spread =
            x87f_store_f32(x87f_add(x87f_load_f32(weaponInfo->hipSpreadStandMin),
                                    x87f_mul(x87f_load_f64(coduo_server_rand_unit()), x87f_load_f32(weaponInfo->maxSpread))));
#else
        const float spread = (float)((long double)weaponInfo->hipSpreadStandMin +
                                     (long double)coduo_server_rand_unit() * (long double)weaponInfo->maxSpread);
#endif

        return Bullet_Fire(attacker, spread, weaponInfo->flameDamage, muzzle, ent);
    }

    Weapon_Artillery_Fire(ent, 0.0f, muzzle);
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_add_turret_fire_event(gentity_t *ent, const weaponInfo_t *weaponInfo, int event, qboolean bulletHit)
{
    const int weaponType = weaponInfo->weaponType;

    if (weaponType == WEAPTYPE_BULLET) {
        uint32_t eventParm = (uint32_t)ent->s.weapon;

        if (!bulletHit) {
            eventParm |= VEH_EVENT_MISS_FLAG;
        }
        G_AddEvent(ent, event, (int)eventParm);
    } else {
        G_AddEvent(ent, event, 0);
    }
}

/* ------------------------------------------------------------------ */
/*  0x8ee02  CMD_VEH_FireTurret                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ee02, 9ee02_CMD_VEH_FireTurret.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_FireTurret(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    qboolean bulletHit = qfalse;

    game_compat_veh_check_turret_control_health(ent);
    if (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] == ENTITYNUM_NONE) {
        return;
    }

    const weaponInfo_t *weaponInfo = game_compat_veh_set_turret_weapon(ent, vehicleInfo->turretWeapon);

    game_compat_veh_check_turret_weapon_type(weaponInfo);
    game_compat_veh_check_turret_barrel_bone(ent, vehicleState->primaryTurretTagIndex);

    DObjSkelMat barrelMatrix;
    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryTurretTagIndex, &barrelMatrix);

    const int primaryPassenger = vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER];
    gentity_t *attacker = &g_entities[primaryPassenger];
    const qboolean dualFlash = vehicleInfo->primaryDualFlash != 0;
    const int flashStart = dualFlash ? vehicleState->primaryFlashSelector * 2 : 0;
    const int flashCount = dualFlash ? 2 : 1;
    const int fireEvent =
        !dualFlash ? EV_FIRE_WEAPON : (vehicleState->primaryFlashSelector == 0 ? EV_FIRE_QUADBARREL_1 : EV_FIRE_QUADBARREL_2);

    for (int flash = 0; flash < flashCount; flash++) {
        weapon_muzzle_t muzzle;
        const qboolean allowTargetCorrection =
            ent->passEntityNum != ENTITYNUM_NONE && flashCount == 1 && game_compat_veh_turret_muzzle_back_active(vehicleState) == 0;

        game_compat_veh_build_turret_muzzle(ent, vehicleState, weaponInfo, vehiclePrimaryFlashTagNames[flashStart + flash],
                                            allowTargetCorrection, &muzzle);
        bulletHit = game_compat_veh_fire_turret_muzzle(ent, attacker, weaponInfo, &muzzle);
    }

    game_compat_veh_add_turret_fire_event(ent, weaponInfo, fireEvent, bulletHit);

    {
        const vec3_t scriptedInput = {-barrelMatrix.axis[0][0], -barrelMatrix.axis[0][1], -barrelMatrix.axis[0][2]};

        VEH_UpdateScriptedInput(ent, scriptedInput, VEH_TURRET_SCRIPTED_INPUT_SCALE, 0.0f, 0.0f);
    }

    vehicleState->primaryFireTime = weaponInfo->fireTime;
    vehicleState->primaryFlashSelector = vehicleState->primaryFlashSelector == 0;
}

/* ------------------------------------------------------------------ */
/*  0x8f57e  CMD_VEH_FireAltTurret                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8f57e, 9f57e_CMD_VEH_FireAltTurret.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_FireAltTurret(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const int primaryPassenger = vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER];
    qboolean bulletHit = qfalse;

    game_compat_veh_check_turret_control_health(ent);
    if (vehicleState->altOverheating != 0 || primaryPassenger == ENTITYNUM_NONE) {
        return;
    }

    const weaponInfo_t *weaponInfo = game_compat_veh_set_turret_weapon(ent, vehicleInfo->turretAltWeapon);

    game_compat_veh_check_turret_weapon_type(weaponInfo);
    game_compat_veh_check_turret_barrel_bone(ent, vehicleState->primaryAltTurretTagIndex);

    DObjSkelMat barrelMatrix;
    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryAltTurretTagIndex, &barrelMatrix);
    (void)barrelMatrix;

    gentity_t *attacker = &g_entities[primaryPassenger];
    if (attacker->client != NULL && attacker->client->sessionState == VEH_CLIENT_SESSION_STATE_SPECTATOR) {
        return;
    }

    for (int flash = 0; flash < 1; flash++) {
        weapon_muzzle_t muzzle;
        const qboolean allowTargetCorrection =
            ent->passEntityNum != ENTITYNUM_NONE && game_compat_veh_turret_muzzle_back_active(vehicleState) == 0;

        game_compat_veh_build_turret_muzzle(ent, vehicleState, weaponInfo, vehicleAltFireTagNames[flash], allowTargetCorrection, &muzzle);
        bulletHit = game_compat_veh_fire_turret_muzzle(ent, attacker, weaponInfo, &muzzle);
        if (weaponInfo->weaponType == WEAPTYPE_BULLET) {
            vehicleState->altHeat += weaponInfo->turretHeatPerShot;
        }
    }

    game_compat_veh_add_turret_fire_event(ent, weaponInfo, EV_FIRE_WEAPONB, bulletHit);
    vehicleState->altFireTime = weaponInfo->fireTime;
    vehicleState->altWeaponSoundTime = 200;
}

/* ------------------------------------------------------------------ */
/*  0x8fc7a  G_VEH_FireGunner                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8fc7a, 9fc7a_G_VEH_FireGunner.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void G_VEH_FireGunner(uint32_t scriptObject, qboolean ignoreReady)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    qboolean bulletHit = qfalse;

    if (!ignoreReady) {
        game_compat_veh_check_turret_control_health(ent);
    }

    if (!ignoreReady &&
        (vehicleState->gunnerOverheating != 0 || vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_GUNNER] == ENTITYNUM_NONE)) {
        return;
    }

    const weaponInfo_t *weaponInfo = game_compat_veh_set_turret_weapon(ent, vehicleInfo->turretGunnerWeapon);

    game_compat_veh_check_turret_weapon_type(weaponInfo);
    if (vehicleState->gunnerTurretTagIndex < 0) {
        Scr_Error(va(VEH_GUNNER_NO_BARREL_ERROR, SL_ConvertToString(ent->targetname)));
    }

    DObjSkelMat barrelMatrix;
    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->gunnerTurretTagIndex, &barrelMatrix);
    (void)barrelMatrix;

    gentity_t *attacker = ignoreReady ? &g_entities[0] : &g_entities[vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_GUNNER]];
    if (!ignoreReady && attacker->client != NULL && attacker->client->sessionState == VEH_CLIENT_SESSION_STATE_SPECTATOR) {
        return;
    }

    for (int flash = 0; flash < 1; flash++) {
        weapon_muzzle_t muzzle;

        game_compat_veh_build_turret_muzzle(ent, vehicleState, weaponInfo, vehicleSecondaryFlashTagNames[flash], qfalse, &muzzle);
        bulletHit = game_compat_veh_fire_turret_muzzle(ent, attacker, weaponInfo, &muzzle);
        if (weaponInfo->weaponType == WEAPTYPE_BULLET) {
            vehicleState->gunnerHeat += weaponInfo->turretHeatPerShot;
        }
    }

    game_compat_veh_add_turret_fire_event(ent, weaponInfo, EV_FIRE_WEAPONC, bulletHit);
    vehicleState->gunnerFireTime = weaponInfo->fireTime;
    vehicleState->gunnerWeaponSoundTime = 200;
}

/* ------------------------------------------------------------------ */
/*  0x901b0  CMD_VEH_FireGunner                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x901b0, a01b0_CMD_VEH_FireGunner.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_FireGunner(uint32_t scriptObject)
{
    G_VEH_FireGunner(scriptObject, qfalse);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_check_player_controlled_vehicle(const gentity_t *ent)
{
    if (game_compat_veh_entity_collision_mode(ent) != VEH_COLLISION_MODE_ALT_TRACE) {
        Scr_Error(va(VEH_PLAYER_CONTROLLED_VEHICLE_ERROR));
    }
}

/* ------------------------------------------------------------------ */
/*  0x901db  CMD_VEH_IsTurretReady                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x901db, a01db_CMD_VEH_IsTurretReady.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_IsTurretReady(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    game_compat_veh_check_player_controlled_vehicle(ent);
    Scr_AddInt(vehicleState->primaryFireTime < 1);
}

/* ------------------------------------------------------------------ */
/*  0x90255  CMD_VEH_GetFireTime                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x90255, a0255_CMD_VEH_GetFireTime.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetFireTime(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    game_compat_veh_check_player_controlled_vehicle(ent);
    Scr_AddInt(vehicleState->primaryFireTime);
}

/* ------------------------------------------------------------------ */
/*  0x902ba  CMD_VEH_GetAltHeat                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x902ba, a02ba_CMD_VEH_GetAltHeat.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetAltHeat(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    Scr_AddFloat(vehicleState->altHeat);
}

/* ------------------------------------------------------------------ */
/*  0x90383  CMD_VEH_GetGunnerHeat                                    */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x90383, a0383_CMD_VEH_GetGunnerHeat.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetGunnerHeat(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    Scr_AddFloat(vehicleState->gunnerHeat);
}

/* ------------------------------------------------------------------ */
/*  0x902fd  CMD_VEH_GetAltOverheating                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x902fd, a02fd_CMD_VEH_GetAltOverheating.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetAltOverheating(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    Scr_AddInt(vehicleState->altOverheating);
}

/* ------------------------------------------------------------------ */
/*  0x90340  CMD_VEH_GetGunnerOverheating                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x90340, a0340_CMD_VEH_GetGunnerOverheating.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetGunnerOverheating(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    Scr_AddInt(vehicleState->gunnerOverheating);
}

/* ------------------------------------------------------------------ */
/*  0x903c6  CMD_VEH_GetDismountSpot                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x903c6, a03c6_CMD_VEH_GetDismountSpot.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void CMD_VEH_GetDismountSpot(uint32_t scriptObject)
{
    gentity_t *ent = VEH_GetEntity((int)scriptObject);
    vec3_t dismountMins = {-15.0f, -15.0f, 15.0f};
    vec3_t dismountMaxs = {15.0f, 15.0f, 70.0f};
    vec3_t dismountOrigin = {ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2]};

    if (VEH_FindValidDismountSpot(ent, ent->currentOrigin, dismountMins, dismountMaxs, dismountOrigin, 0)) {
        Scr_AddVector(dismountOrigin);
    } else {
        Scr_AddVector(ent->currentOrigin);
    }
}

/* ------------------------------------------------------------------ */
/*  0x9048f  G_PlayerVehiclePositionAndBlend                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9048f, a048f_G_PlayerVehiclePositionAndBlend.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED; invalid vehicle/seat exits return 0, helper-extracted animation blend side effects, tag lookup/fail return, position offset stores, entity-state update, axis-angle stores, and success return 1 checked against current decompiler output. */
qboolean G_PlayerVehiclePositionAndBlend(gentity_t *player)
{
    gclient_t *client = player->client;
    clientInfo_t *clientInfo;
    gentity_t *vehicleEnt;
    const char *tagName;
    const float *tagOffset;
    int vehicleType;
    DObjSkelMat tagMatrix;

    if (player->s.vehicleEntityNum < VEH_PHYSICS_CLIENT_COUNT || player->s.vehicleEntityNum == ENTITYNUM_NONE) {
        return qfalse;
    }

    if (client->ps.vehiclePosition < 1 || client->ps.vehiclePosition > 6) {
        return qfalse;
    }

    vehicleEnt = &g_entities[player->passEntityNum];
    clientInfo = &bgs.clientinfo[player->s.clientNum];

    if (game_compat_veh_player_blend_anim_slot_enabled(clientInfo)) {
        game_compat_veh_player_blend_update_anims(player, vehicleEnt, clientInfo);
    }

    tagName = BG_GetVehiclePosTag(client->ps.vehiclePosition);
    if (G_DObjGetWorldTagMatrix(vehicleEnt, tagName, &tagMatrix) == 0) {
        Com_Printf(VEH_PLAYER_BLEND_TAG_FAIL_WARNING, tagName);
        return qfalse;
    }

    client->ps.psOrigin[0] = tagMatrix.origin[0];
    client->ps.psOrigin[1] = tagMatrix.origin[1];
    client->ps.psOrigin[2] = tagMatrix.origin[2];

    vehicleType = vehicleEnt->s.vehicleType;
    tagOffset = BG_GetVehiclePosOffset(vehicleType, client->ps.vehiclePosition);
    /* 0x916e1..0x91837: nine separate += stores, one rounded store per
     * axis term (offset component groups of three), not three collapsed
     * dot products. */
    client->ps.psOrigin[0] += tagMatrix.axis[0][0] * tagOffset[0];
    client->ps.psOrigin[1] += tagMatrix.axis[0][1] * tagOffset[0];
    client->ps.psOrigin[2] += tagMatrix.axis[0][2] * tagOffset[0];
    client->ps.psOrigin[0] += tagMatrix.axis[1][0] * tagOffset[1];
    client->ps.psOrigin[1] += tagMatrix.axis[1][1] * tagOffset[1];
    client->ps.psOrigin[2] += tagMatrix.axis[1][2] * tagOffset[1];
    client->ps.psOrigin[0] += tagMatrix.axis[2][0] * tagOffset[2];
    client->ps.psOrigin[1] += tagMatrix.axis[2][1] * tagOffset[2];
    client->ps.psOrigin[2] += tagMatrix.axis[2][2] * tagOffset[2];

    BG_PlayerStateToEntityState(&client->ps, &player->s, qtrue);
    player->currentOrigin[0] = client->ps.psOrigin[0];
    player->currentOrigin[1] = client->ps.psOrigin[1];
    player->currentOrigin[2] = client->ps.psOrigin[2];
    Axis4ToAngles(&tagMatrix, player->currentAngles);
    Axis4ToAngles(&tagMatrix, clientInfo->turretOverrideAngles);
    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x918e2  script_vehicle_owner_enable                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x918e2, a18e2_script_vehicle_owner_enable.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void script_vehicle_owner_enable(gentity_t *ent)
{
    gentity_t *owner = ent->vehicleOwner;

    if (owner != NULL && owner->client != NULL && (ent->svFlags & VEH_OWNER_ICON_SVFLAG_LINKED) == 0) {
        ent->s.pos.trBase[0] = ent->currentOrigin[0];
        ent->s.pos.trBase[1] = ent->currentOrigin[1];
        ent->s.pos.trBase[2] = ent->currentOrigin[2];
        ent->tempClientNumFc = owner->s.number;
        ent->svFlags |= VEH_OWNER_ICON_SVFLAG_LINKED;
        /*
         * Owner-icon entities reuse entity-state time for the rounded absolute
         * expiry time.
         */
        /* 0x91991..0x919bc: level.time feeds the sum via fild with no
         * float store, and the sum feeds fistp directly. */
#if EMULATE_X87
        ent->s.time = x87f_store_i32_trunc(x87f_add(
            x87f_mul(x87f_load_f32(ent->ownerIconDelaySeconds), x87f_load_f32(VEH_OWNER_ICON_TIME_SCALE)), x87f_load_i32(level.time)));
#else
        ent->s.time = (int)((long double)ent->ownerIconDelaySeconds * (long double)VEH_OWNER_ICON_TIME_SCALE + (long double)level.time);
#endif
        /*
         * Owner-icon entities write enemyScanRadius as a float at gentity_t+0x0d8,
         * reusing the player client-info lean-fraction word.
         */
        ent->s.clientInfoLeanFraction = (float)ent->enemyScanRadius;
        trap_LinkEntity(ent);
    }

    ent->nextthink = level.time + VEH_OWNER_ICON_DELAY_MS;
}

/* ------------------------------------------------------------------ */
/*  0x919fd  SP_script_vehicle_owner_icon                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x919fd, a19fd_SP_script_vehicle_owner_icon.c, VERIFY-VEHICLE-CMDS-TURRET-2026-06-17): DATAFLOW_VERIFIED */
void SP_script_vehicle_owner_icon(gentity_t *ent)
{
    ent->s.eType = ET_VEHICLE_OWNER_ICON;
    ent->think = script_vehicle_owner_enable;
    ent->nextthink = level.time + VEH_OWNER_ICON_DELAY_MS;
}

/* ------------------------------------------------------------------ */
/*  0x8d1ca  Scr_Vehicle_Pain                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8d1ca, 9d1ca_Scr_Vehicle_Pain.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void Scr_Vehicle_Pain(gentity_t *ent, gentity_t *attacker, int damage, const float *point, int mod, const float *dir, int hitLocation)
{
    const weaponInfo_t *weaponInfo = NULL;

    (void)hitLocation;

    if (mod != MOD_COLLISION && attacker != NULL && attacker->s.weapon != 0) {
        weaponInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(attacker->s.weapon);
    }

    if ((weaponInfo != NULL &&
         (damage > 200 || weaponInfo->weaponType == WEAPTYPE_PROJECTILE || weaponInfo->weaponType == WEAPTYPE_GRENADE)) ||
        mod == MOD_EXPLOSIVE) {
        VEH_UpdateScriptedInput(ent, dir, 1.0f, 0.0f, 0.0f);
    }

    if (ent->s.vehicleEntityNum >= 0 && ent->s.vehicleEntityNum < level.maxclients) {
        gentity_t *feedbackEnt = &g_entities[ent->s.vehicleEntityNum];
        gclient_t *client = feedbackEnt->client;

        if (client != NULL && client->connectedState == CON_CONNECTED) {
            client->damageTaken += damage;
            if (attacker == ent) {
                client->damageFrom[0] = point[0];
                client->damageFrom[1] = point[1];
                client->damageFrom[2] = point[2];
                client->damageFromWorld = 1;
            } else {
                client->damageFrom[0] = ent->currentOrigin[0] - point[0];
                client->damageFrom[1] = ent->currentOrigin[1] - point[1];
                client->damageFrom[2] = ent->currentOrigin[2] - point[2];
                client->damageFromWorld = 0;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x7d709  VEH_CalcAccel                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7d709, 8d709_FUN_0008d709.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_CalcAccel(gentity_t *ent, const int8_t input[3], vec3_t linearAccel, vec3_t angularAccel)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    gentity_t *driver = &g_entities[ent->passEntityNum];
    gclient_t *driverClient = driver->client;
    float maxSpeed;
    float accelLimit;
    vec3_t desiredLocalVelocity;
    float speed;

    /* 0x7d776..0x7d7c4: stock enters the scripted arm only through an
     * ordered-above x87 branch.  A NaN throttle therefore takes the default
     * arm rather than satisfying the logical complement of <=. */
    if (vehicleState->throttleScale > 0.0f && vehicleState->scriptedInputEndTime > level.time) {
        maxSpeed = vehicleState->scriptedMaxSpeed;
        accelLimit = vehicleState->scriptedAcceleration;
    } else {
        const float entitySpeedScale = ent->doorAltSpeed;

        maxSpeed = vehicleInfo->maxSpeed * VEH_DEFAULT_SPEED_SCALE;
        if (game_compat_veh_float_is_non_zero_or_nan(entitySpeedScale)) {
            maxSpeed *= entitySpeedScale;
        }
        accelLimit = vehicleInfo->acceleration;
    }

    if (input[0] < 0) {
        desiredLocalVelocity[0] = vehicleState->angularVelocity[0] - accelLimit * VEH_PHYSICS_FRAME_SECONDS;
        if (desiredLocalVelocity[0] < -maxSpeed * VEH_REVERSE_SPEED_SCALE) {
            desiredLocalVelocity[0] = -maxSpeed * VEH_REVERSE_SPEED_SCALE;
        }
    } else if (input[0] > 0) {
        desiredLocalVelocity[0] = vehicleState->angularVelocity[0] + accelLimit * VEH_PHYSICS_FRAME_SECONDS;
    } else if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL && game_compat_veh_float_is_non_zero_or_nan(vehicleState->angularVelocity[0])) {
        desiredLocalVelocity[0] = game_compat_veh_approach_float_step(vehicleState->angularVelocity[0], 0.0f, VEH_IDLE_BRAKE_STEP);
    } else {
        desiredLocalVelocity[0] = 0.0f;
    }

    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL && fabsf(vehicleState->angularVelocity[0]) < VEH_4WHEEL_LATERAL_SPEED) {
        /* 0x7d9af..0x7d9e3: the 80-bit multiply chain runs in this order. */
        desiredLocalVelocity[1] = (1.0f - fabsf(vehicleState->angularVelocity[0]) / VEH_4WHEEL_LATERAL_SPEED) *
                                  fabsf(desiredLocalVelocity[0]) * VEH_4WHEEL_LATERAL_SCALE * vehicleState->acceleration[1] /
                                  VEH_4WHEEL_LATERAL_DIVISOR;
    } else {
        desiredLocalVelocity[1] = 0.0f;
    }

    desiredLocalVelocity[2] = vehicleState->angularVelocity[2];

    if (vehicleState->viewClampTargetAngles[0] < 0.0f) {
        maxSpeed *= 1.0f - vehicleState->viewClampTargetAngles[0] / -90.0f;
    } else {
        maxSpeed *= vehicleState->viewClampTargetAngles[0] / 90.0f + 1.0f;
    }

    if (game_compat_veh_float_is_non_zero_or_nan(maxSpeed)) {
        speed = game_compat_veh_length3(desiredLocalVelocity);
        if (game_compat_veh_float_is_non_zero_or_nan(speed) && maxSpeed < speed) {
            VectorNormalize(desiredLocalVelocity);
            desiredLocalVelocity[0] *= maxSpeed;
            desiredLocalVelocity[1] *= maxSpeed;
            desiredLocalVelocity[2] *= maxSpeed;
        }
    }

    for (int axis = 0; axis < 3; axis++) {
        linearAccel[axis] = (desiredLocalVelocity[axis] - vehicleState->angularVelocity[axis]) / VEH_PHYSICS_FRAME_SECONDS;
    }

    speed = game_compat_veh_length3(linearAccel);
    if (accelLimit < speed) {
        VectorNormalize(linearAccel);
        linearAccel[0] *= accelLimit;
        linearAccel[1] *= accelLimit;
        linearAccel[2] *= accelLimit;
    }

    if (vehicleState->wheelBaseLength == 0.0f) {
        vec3_t frontWheel;
        vec3_t rearWheel;

        VEH_GetWheelOrigin(ent, 0, frontWheel);
        VEH_GetWheelOrigin(ent, 2, rearWheel);
        vehicleState->wheelBaseLength = VectorDistance(frontWheel, rearWheel);
    }

    if (vehicleInfo->type != VEHICLE_TYPE_4_WHEEL) {
        float targetYawRate;

        if (input[2] < 1) {
            if (input[1] < 0) {
                targetYawRate = vehicleState->acceleration[1] + vehicleInfo->steeringRate * VEH_PHYSICS_FRAME_SECONDS;
            } else if (input[1] > 0) {
                targetYawRate = vehicleState->acceleration[1] - vehicleInfo->steeringRate * VEH_PHYSICS_FRAME_SECONDS;
            } else {
                targetYawRate = 0.0f;
            }

            targetYawRate = game_compat_veh_clamp_abs(targetYawRate, vehicleInfo->steeringLimit);
            angularAccel[1] = (targetYawRate - vehicleState->acceleration[1]) / VEH_PHYSICS_FRAME_SECONDS;
            angularAccel[1] = game_compat_veh_clamp_abs(angularAccel[1], vehicleInfo->steeringRate);
        } else {
            targetYawRate = AngleSubtract(driverClient->ps.viewAngles[1], vehicleState->previousAngles[1]) / VEH_PHYSICS_FRAME_SECONDS;
            targetYawRate = game_compat_veh_clamp_abs(targetYawRate, vehicleInfo->steeringLimit);
            angularAccel[1] = (targetYawRate - vehicleState->acceleration[1]) / VEH_PHYSICS_FRAME_SECONDS;
        }
    } else {
        float turnRate;
        float targetSteerAngle;
        float steerAngle;
        axis_t axis;
        vec3_t worldAccel;
        vec3_t predictedVelocity;
        vec3_t predictedLocalVelocity;
        float steeringScale;
        float targetYawRate;
        float yawRateScale;

        /* 0x7dca9..0x7df34: the clamp conditionals compare the unrounded
         * 80-bit factor chain; only the pass-through arm rounds, and it
         * rounds STEPWISE (one float store per operation). */
        {
            const float speedLen3 = game_compat_veh_length3(vehicleState->velocity);
            const long double speedFactor = 1.0f - speedLen3 / vehicleInfo->maxSpeed;

            if (speedFactor > 0.0f) {
                const long double scaledFactor = speedFactor * 0.9f + 0.1f;

                if (scaledFactor < 0.0f) {
                    turnRate = 0.0f; /* 0x7df22 */
                } else if (scaledFactor > 1.0f) {
                    turnRate = 1.0f; /* 0x7df14 */
                } else {
                    float steppedFactor = (float)speedFactor; /* 0x7ded6 */
                    steppedFactor = 0.9f * steppedFactor; /* 0x7dee8 */
                    turnRate = 0.1f + steppedFactor; /* 0x7defe */
                }
            } else {
                turnRate = 0.1f; /* 0x7df06 */
            }
        }

        if (input[1] < 0) {
            /* 0x7dfa8..0x7e1ef: same stepwise cascade, scaled by the steer
             * angle as a fourth rounded store; the clamped leaves are the
             * compile-time folded products. */
            const float speedLen3 = game_compat_veh_length3(vehicleState->velocity);
            const long double steerFactor = 1.0f - speedLen3 / vehicleInfo->maxSpeed;

            if (steerFactor > 0.0f) {
                const long double scaledFactor = steerFactor * 0.95f + 0.05f;

                if (scaledFactor < 0.0f) {
                    targetSteerAngle = -VEH_4WHEEL_STEER_ANGLE * 0.0f; /* -0.0f @0x7e1dd */
                } else if (scaledFactor > 1.0f) {
                    targetSteerAngle = -VEH_4WHEEL_STEER_ANGLE; /* 0x7e1cf */
                } else {
                    float steppedFactor = (float)steerFactor; /* 0x7e17d */
                    steppedFactor = 0.95f * steppedFactor; /* 0x7e191 */
                    steppedFactor = 0.05f + steppedFactor; /* 0x7e1a5 */
                    targetSteerAngle = -VEH_4WHEEL_STEER_ANGLE * steppedFactor; /* 0x7e1b9 */
                }
            } else {
                targetSteerAngle = -VEH_4WHEEL_STEER_ANGLE * 0.05f; /* 0x7e1c1 */
            }
        } else if (input[1] > 0) {
            /* 0x7e20d..0x7e4af: positive-steer mirror of the cascade. */
            const float speedLen3 = game_compat_veh_length3(vehicleState->velocity);
            const long double steerFactor = 1.0f - speedLen3 / vehicleInfo->maxSpeed;

            if (steerFactor > 0.0f) {
                const long double scaledFactor = steerFactor * 0.95f + 0.05f;

                if (scaledFactor < 0.0f) {
                    targetSteerAngle = VEH_4WHEEL_STEER_ANGLE * 0.0f; /* 0x7e49d */
                } else if (scaledFactor > 1.0f) {
                    targetSteerAngle = VEH_4WHEEL_STEER_ANGLE; /* 0x7e48f */
                } else {
                    float steppedFactor = (float)steerFactor; /* 0x7e43d */
                    steppedFactor = 0.95f * steppedFactor; /* 0x7e451 */
                    steppedFactor = 0.05f + steppedFactor; /* 0x7e465 */
                    targetSteerAngle = VEH_4WHEEL_STEER_ANGLE * steppedFactor; /* 0x7e479 */
                }
            } else {
                targetSteerAngle = VEH_4WHEEL_STEER_ANGLE * 0.05f; /* 0x7e481 */
            }
        } else {
            targetSteerAngle = 0.0f;
        }

        if (driverClient == NULL || input[2] < 1) {
            if (driverClient != NULL && (driverClient->currentButtons & VEH_DRIVER_BUTTON_STRAFE) != 0) {
                turnRate = 2.0f;
                if (input[1] < 0) {
                    targetSteerAngle = -VEH_4WHEEL_STEER_ANGLE;
                } else if (input[1] > 0) {
                    targetSteerAngle = VEH_4WHEEL_STEER_ANGLE;
                } else {
                    targetSteerAngle = 0.0f;
                }
            }
        } else {
            turnRate = 2.0f;
            if (input[1] < 0) {
                targetSteerAngle = -VEH_4WHEEL_STEER_ANGLE;
            } else if (input[1] > 0) {
                targetSteerAngle = VEH_4WHEEL_STEER_ANGLE;
            }

            targetSteerAngle = AngleSubtract(driverClient->ps.viewAngles[1] - targetSteerAngle, vehicleState->previousAngles[1]);
            targetSteerAngle = game_compat_veh_clamp_abs(targetSteerAngle, VEH_4WHEEL_STEER_ANGLE);

            if (input[0] < 0) {
                /* 0x7e5bf..0x7e675: the steer-delta chain is compared
                 * unrounded against DOUBLE 10.0/60.0/1.0; the scaled accel
                 * rounds to double, then to float. */
                const double steerDelta = fabs((double)(vehicleState->steerAngle - targetSteerAngle));
                if (steerDelta > VEH_4WHEEL_STEER_CENTER) {
                    const double accel0 = linearAccel[0];
                    double scaledAccel;

                    if ((steerDelta - VEH_4WHEEL_STEER_CENTER) / VEH_4WHEEL_STEER_DAMP_RANGE < 1.0) {
                        scaledAccel = (1.0 - (steerDelta - VEH_4WHEEL_STEER_CENTER) / VEH_4WHEEL_STEER_DAMP_RANGE) * accel0;
                    } else {
                        scaledAccel = 0.0 * accel0; /* 0x7e655 */
                    }
                    linearAccel[0] = (float)scaledAccel;
                }
            } else {
                targetSteerAngle = -targetSteerAngle;
            }
        }

        steerAngle = vehicleState->steerAngle;
        if ((targetSteerAngle < 0.0f && steerAngle > 0.0f) || (targetSteerAngle > 0.0f && steerAngle < 0.0f)) {
            turnRate += turnRate;
        }

        steerAngle = game_compat_veh_approach_float_step(steerAngle, targetSteerAngle, turnRate * VEH_4WHEEL_RATE_STEP_SCALE);
        vehicleState->steerAngle = steerAngle;

        AnglesToAxis(vehicleState->viewClampTargetAngles, axis);
        MatrixTransformVector(linearAccel, (const vec_t(*)[3])axis, worldAccel);

        predictedVelocity[0] = vehicleState->velocity[0] + worldAccel[0] * VEH_PHYSICS_FRAME_SECONDS;
        predictedVelocity[1] = vehicleState->velocity[1] + worldAccel[1] * VEH_PHYSICS_FRAME_SECONDS;
        predictedVelocity[2] = vehicleState->velocity[2] + worldAccel[2] * VEH_PHYSICS_FRAME_SECONDS;

        if (game_compat_veh_length3(predictedVelocity) < VEH_4WHEEL_MIN_SPEED) {
            predictedVelocity[0] = 0.0f;
            predictedVelocity[1] = 0.0f;
            predictedVelocity[2] = 0.0f;
        }

        for (int row = 0; row < 3; row++) {
            predictedLocalVelocity[row] =
                axis[row][0] * predictedVelocity[0] + axis[row][1] * predictedVelocity[1] + axis[row][2] * predictedVelocity[2];
        }

        /* 0x7e977..0x7ea7d: |pred[0]|/wheelBase is rounded to DOUBLE; the
         * speed factor is one 80-bit chain with DOUBLE 0.7/0.3 constants
         * (values differ from 0.7f/0.3f) and no 0..1 clamp at this site;
         * the product rounds to double, then to float. */
        {
            const double steerQuotient = fabs((double)predictedLocalVelocity[0]) / vehicleState->wheelBaseLength;
            double scaledQuotient;

            speed = game_compat_veh_length3(vehicleState->velocity);
            if (1.0 - speed / vehicleInfo->maxSpeed > 0.0) {
                scaledQuotient = ((1.0 - speed / vehicleInfo->maxSpeed) * 0.7 + 0.3) * steerQuotient;
            } else {
                scaledQuotient = 0.3 * steerQuotient; /* 0x7ea63 */
            }
            steeringScale = (float)scaledQuotient;
        }
        steeringScale = game_compat_veh_clamp_float(steeringScale, 0.0f, 3.0f);

        if (predictedLocalVelocity[0] > 0.0f) {
            targetYawRate = -steerAngle * steeringScale;
        } else if (predictedLocalVelocity[0] < 0.0f) {
            targetYawRate = steerAngle * steeringScale;
        } else {
            targetYawRate = 0.0f;
        }
        /* 0x7eb4c..0x7ebc8: the clamp limit expression stays 80-bit in the
         * compares; only the clamped leaf stores round. */
        if (targetYawRate < -vehicleInfo->steeringLimit * steeringScale) {
            targetYawRate = -vehicleInfo->steeringLimit * steeringScale;
        } else if (targetYawRate > vehicleInfo->steeringLimit * steeringScale) {
            targetYawRate = vehicleInfo->steeringLimit * steeringScale;
        }

        if (game_compat_veh_wheel_surface_at(vehicleState, 0) == 0 && game_compat_veh_wheel_surface_at(vehicleState, 1) == 0 &&
            game_compat_veh_wheel_surface_at(vehicleState, 2) == 0 && game_compat_veh_wheel_surface_at(vehicleState, 3) == 0) {
            yawRateScale = 0.0f;
        } else if (game_compat_veh_float_is_non_zero_or_nan(game_compat_veh_length3(predictedVelocity))) {
            const float velocitySpeed = game_compat_veh_length3(vehicleState->velocity);

            if (fabsf(predictedLocalVelocity[0] / velocitySpeed) * 2.0f < 2.0f) {
                yawRateScale = fabsf(predictedLocalVelocity[0] / velocitySpeed) * 2.0f;
            } else {
                yawRateScale = 2.0f;
            }
        } else {
            yawRateScale = 2.0f;
        }

        if (fabsf(targetYawRate - vehicleState->acceleration[1]) < VEH_4WHEEL_STEER_CENTER) {
            yawRateScale = 1.0f;
        }

        angularAccel[1] = ((targetYawRate - vehicleState->acceleration[1]) * yawRateScale) / VEH_PHYSICS_FRAME_SECONDS;
        /* 0x7ed84..0x7ee0f: the clamp limit expression stays 80-bit in the
         * compares; only the clamped leaf stores round. */
        if (angularAccel[1] < -vehicleInfo->steeringRate * yawRateScale) {
            angularAccel[1] = -vehicleInfo->steeringRate * yawRateScale;
        } else if (angularAccel[1] > vehicleInfo->steeringRate * yawRateScale) {
            angularAccel[1] = vehicleInfo->steeringRate * yawRateScale;
        }
    }

    angularAccel[0] = (0.0f - vehicleState->acceleration[0]) / VEH_PHYSICS_FRAME_SECONDS;
    angularAccel[0] = game_compat_veh_clamp_abs(angularAccel[0], vehicleInfo->steeringRate);
    angularAccel[2] = (0.0f - vehicleState->acceleration[2]) / VEH_PHYSICS_FRAME_SECONDS;
    angularAccel[2] = game_compat_veh_clamp_abs(angularAccel[2], vehicleInfo->steeringRate);

    {
        float rollTarget;
        float rollAccelTarget;
        float rollAccelBlend;
        float rollAccel;
        float rollAngle = vehicleState->externalVelocity[0];

        if (input[0] < 0) {
            /* 0x7f1a6..0x7f24c: DOUBLE roll math, one rounded double store
             * per operation (|input|/100, *scale, /2), then one float. */
            const double rollScale = vehicleInfo->rollInputScale;
            double rolled;

            if (fabs((double)input[0]) < 100.0) {
                double rollFraction = fabs((double)input[0]) / 100.0;
                rollFraction = rollFraction * rollScale;
                rolled = rollFraction / 2.0;
            } else {
                rolled = rollScale / 2.0;
            }
            rollTarget = (float)rolled;
        } else if (input[0] > 0) {
            /* 0x7f260..0x7f31b: mirror with the fraction negated in place
             * (sign-bit flip on the stored double). */
            const double rollScale = vehicleInfo->rollInputScale;
            double rolled;

            if (fabs((double)input[0]) < 100.0) {
                double rollFraction = fabs((double)input[0]) / 100.0;
                rollFraction = -rollFraction;
                rollFraction = rollFraction * rollScale;
                rolled = rollFraction / 2.0;
            } else {
                rolled = -1.0 * rollScale; /* 0x7f2ed */
                rolled = rolled / 2.0;
            }
            rollTarget = (float)rolled;
        } else {
            rollTarget = 0.0f;
        }

        if (linearAccel[0] < -VEH_4WHEEL_STEER_CENTER) {
            /* 0x7f347..0x7f3da: one 80-bit chain rounded to double, then
             * to float (unlike the stepwise block above). */
            const double rollScale = vehicleInfo->rollInputScale;
            const double currentRoll = rollTarget;
            double rolled;

            if (fabs((double)input[0]) < 100.0) {
                rolled = fabs((double)input[0]) / 100.0 * rollScale / 2.0 + currentRoll;
            } else {
                rolled = rollScale / 2.0 + currentRoll;
            }
            rollTarget = (float)rolled;
        } else if (linearAccel[0] > VEH_4WHEEL_STEER_CENTER) {
            /* 0x7f401..0x7f4a0: negated mirror of the chain. */
            const double rollScale = vehicleInfo->rollInputScale;
            const double currentRoll = rollTarget;
            double rolled;

            if (fabs((double)input[0]) < 100.0) {
                rolled = -(fabs((double)input[0]) / 100.0) * rollScale / 2.0 + currentRoll;
            } else {
                rolled = -1.0 * rollScale / 2.0 + currentRoll; /* 0x7f476 */
            }
            rollTarget = (float)rolled;
        }

        rollAccelTarget = (rollTarget - rollAngle) * VEH_ROLL_ACCEL_SCALE;
        /* 0x7f4d6..0x7f50b: the pow argument is the quotient rounded to
         * DOUBLE with its sign masked, and the call is double pow(). */
        rollAccelBlend = game_compat_veh_clamp_float(0.2f + pow(fabs((double)(rollAngle / vehicleInfo->rollLimit)), 2.0f), 0.0f, 1.0f);
        /* 0x7f571..0x7f59f: blend*target is stored to a float slot before
         * the second term is accumulated. */
        rollAccel = rollAccelBlend * rollAccelTarget;
        rollAccel = (rollAccelTarget + vehicleState->angularAcceleration[0]) * (1.0f - rollAccelBlend) + rollAccel;

        if (fabsf(rollAccel) < VEH_ROLL_ACCEL_SNAP && rollAngle < VEH_ROLL_ACCEL_SNAP) {
            rollAccel = 0.0f;
        }

        rollAccel = game_compat_veh_approach_float_step(vehicleState->angularAcceleration[0], rollAccel, VEH_ROLL_ACCEL_MAX_STEP);
        rollAccel = game_compat_veh_clamp_abs(rollAccel, VEH_ROLL_ACCEL_CLAMP);
        vehicleState->angularAcceleration[0] = rollAccel;
        vehicleState->externalVelocity[0] = rollAngle + rollAccel * VEH_PHYSICS_FRAME_SECONDS;
    }

    {
        float steeringRollTarget;
        float steeringRollAccelTarget;
        float steeringRollVelocity = vehicleState->angularAcceleration[2];

        if (vehicleState->acceleration[1] > 0.0f) {
            steeringRollTarget = vehicleInfo->steeringRollScale;
            if (vehicleState->acceleration[1] / VEH_STEERING_ROLL_SPEED < 1.0f) {
                steeringRollTarget *= vehicleState->acceleration[1] / VEH_STEERING_ROLL_SPEED;
            }
        } else if (vehicleState->acceleration[1] < 0.0f) {
            steeringRollTarget = -vehicleInfo->steeringRollScale;
            if (-vehicleState->acceleration[1] / VEH_STEERING_ROLL_SPEED < 1.0f) {
                steeringRollTarget *= -vehicleState->acceleration[1] / VEH_STEERING_ROLL_SPEED;
            }
        } else {
            steeringRollTarget = 0.0f;
        }

        if (input[0] < 0) {
            steeringRollTarget = -steeringRollTarget;
        } else if (input[0] == 0) {
            steeringRollTarget = 0.0f;
        }

        steeringRollAccelTarget = (steeringRollTarget - vehicleState->externalVelocity[2]) * VEH_ROLL_ACCEL_SCALE;

        /* 0x7f842..0x7f908: clamped-delta approach; the step expression
         * (steeringLimit * frame) stays 80-bit inside compares and sums. */
        if (steeringRollAccelTarget - steeringRollVelocity < -vehicleInfo->steeringLimit * VEH_PHYSICS_FRAME_SECONDS) {
            steeringRollVelocity = steeringRollVelocity + -vehicleInfo->steeringLimit * VEH_PHYSICS_FRAME_SECONDS;
        } else if (steeringRollAccelTarget - steeringRollVelocity > vehicleInfo->steeringLimit * VEH_PHYSICS_FRAME_SECONDS) {
            steeringRollVelocity = vehicleInfo->steeringLimit * VEH_PHYSICS_FRAME_SECONDS + steeringRollVelocity;
        } else {
            steeringRollVelocity = (steeringRollAccelTarget - steeringRollVelocity) + steeringRollVelocity;
        }
        vehicleState->angularAcceleration[2] = steeringRollVelocity;
        vehicleState->externalVelocity[2] += steeringRollVelocity * VEH_PHYSICS_FRAME_SECONDS;
    }
}

/* ------------------------------------------------------------------ */
/*  0x87cd3  VEH_UpdateClient                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x87cd3, 97cd3_FUN_00097cd3.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_UpdateClient(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    int8_t input[3] = {0, 0, 0};

    if (ent->health >= 1) {
        if (ent->passEntityNum == ENTITYNUM_NONE) {
            if (vehicleState->scriptedDriverEndTime > level.time) {
                input[0] = (int8_t)vehicleState->animLeftTarget;
                input[1] = (int8_t)vehicleState->animRightTarget;
            } else {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                const int stressCount = game_compat_veh_abs_int(g_vehicleTrafficStressTest.integer);

                if (vehicleState->slotIndex < stressCount) {
                    static const float forwardTimeScale = VEH_STRESS_FORWARD_TIME_SCALE;
                    static const float rightTimeScale = VEH_STRESS_RIGHT_TIME_SCALE;
                    static const float inputScale = VEH_STRESS_INPUT_SCALE;
                    /* 0x8803c..0x880d9: level.time feeds fmul via fild with
                     * no float store; the argument rounds to DOUBLE for
                     * CoduoLibm_Sin()/CoduoLibm_Cos(), and each scaled result
                     * feeds fistp WORD directly. */
#if EMULATE_X87
                    const double forwardPhase = x87f_store_f64(x87f_mul(x87f_load_i32(level.time), x87f_load_f32(forwardTimeScale)));
                    const double rightPhase = x87f_store_f64(x87f_mul(x87f_load_i32(level.time), x87f_load_f32(rightTimeScale)));
                    input[0] = (int8_t)(int16_t)x87f_store_i32_trunc(
                        x87f_mul(x87f_load_f64(CoduoLibm_Sin(forwardPhase)), x87f_load_f32(inputScale)));
                    input[1] = (int8_t)(int16_t)x87f_store_i32_trunc(
                        x87f_mul(x87f_load_f64(CoduoLibm_Cos(rightPhase)), x87f_load_f32(inputScale)));
#elif defined(__x86_64__)
                    {
                        const double forwardPhase = (double)((long double)level.time * (long double)forwardTimeScale);
                        const double rightPhase = (double)((long double)level.time * (long double)rightTimeScale);

                        input[0] = (int8_t)CODUO_X87_TRUNCATE_I16((long double)CoduoLibm_Sin(forwardPhase) * (long double)inputScale);
                        input[1] = (int8_t)CODUO_X87_TRUNCATE_I16((long double)CoduoLibm_Cos(rightPhase) * (long double)inputScale);
                    }
#else
                    input[0] =
                        (int8_t)(int16_t)(CoduoLibm_Sin((double)((long double)level.time * (long double)forwardTimeScale)) * inputScale);
                    input[1] =
                        (int8_t)(int16_t)(CoduoLibm_Cos((double)((long double)level.time * (long double)rightTimeScale)) * inputScale);
#endif
                }
            }
        } else {
            gentity_t *driver = &g_entities[ent->passEntityNum];
            gclient_t *driverClient = driver->client;

            if (driverClient != NULL && driverClient->driverUnlinkRequested != 0 && VEH_UnlinkPlayer(driver, qtrue)) {
                driverClient->driverUnlinkRequested = 0;
                return;
            }

            driverClient->ps.entityStateFlags |= EF_VEHICLE_ACTIVE;
            if (ent->vehiclePrimaryDisabled == 0 && vehicleState->scriptedInputEndTime < level.time &&
                (driverClient->ps.entityStateFlags & EF_VEHICLE_POPOUT) == 0) {
                input[0] = (int8_t)driverClient->command.forwardmove;
                input[1] = (int8_t)driverClient->command.rightmove;
                input[2] = (int8_t)driverClient->command.upmove;

                if (input[0] != 0 && vehicleState->animLeftSource == 0) {
                    vehicleState->animLeftTime = level.time;
                }
                vehicleState->animLeftSource = input[0];
                vehicleState->animRightSource = 0;

                if (vehicleInfo->type == VEHICLE_TYPE_TANK && vehicleInfo->primaryYawLimitPos == 0.0f &&
                    vehicleInfo->primaryYawLimitNeg == 0.0f && game_compat_veh_float_is_non_zero_or_nan(driverClient->ps.adsFraction)) {
                    input[2] = 127;
                }

                if (vehicleInfo->type == VEHICLE_TYPE_TANK || input[2] > 0) {
                    driverClient->ps.entityStateFlags &= ~EF_VEHICLE_ACTIVE;
                }

                if (vehicleInfo->type == VEHICLE_TYPE_TANK && input[0] < 0) {
                    input[1] = (int8_t)-input[1];
                }
            }
        }
    }

    if (input[0] != 0 || input[1] != 0 || input[2] != 0 || !VEH_PhysicsNotRequired(ent, qtrue)) {
        vec3_t linearAccel;
        vec3_t angularAccel;
        matrix43_t axis;

        VEH_CalcAccel(ent, input, linearAccel, angularAccel);
        VEH_GroundTrace(ent);

        if (vehGroundTraceWalkable) {
            for (int axisIndex = 0; axisIndex < 3; axisIndex++) {
                vehicleState->acceleration[axisIndex] += angularAccel[axisIndex] * VEH_PHYSICS_FRAME_SECONDS;
            }
        }

        vehicleState->viewClampTargetAngles[1] =
            AngleNormalize180(vehicleState->previousAngles[1] + vehicleState->acceleration[1] * VEH_PHYSICS_FRAME_SECONDS);
        vehicleState->viewClampTargetAngles[0] = 0.0f;
        vehicleState->viewClampTargetAngles[2] = 0.0f;

        if (vehicleInfo->type == VEHICLE_TYPE_TANK) {
            game_compat_veh_set_entity_primary_yaw(ent, game_compat_veh_entity_primary_yaw(ent) -
                                                            vehicleState->acceleration[1] * VEH_PHYSICS_FRAME_SECONDS);
        }

        AnglesToAxis(vehicleState->viewClampTargetAngles, axis.axis);
        axis.origin[0] = 0.0f;
        axis.origin[1] = 0.0f;
        axis.origin[2] = 0.0f;

        if (vehGroundTraceWalkable) {
            vec3_t worldAccel;

            MatrixTransformVector(linearAccel, (const vec_t(*)[3])axis.axis, worldAccel);
            for (int axisIndex = 0; axisIndex < 3; axisIndex++) {
                vehicleState->velocity[axisIndex] += worldAccel[axisIndex] * VEH_PHYSICS_FRAME_SECONDS;
            }
            VEH_GroundFriction(ent);
        }

        if (game_compat_veh_float_is_non_zero_or_nan(vehicleState->velocity[0]) ||
            game_compat_veh_float_is_non_zero_or_nan(vehicleState->velocity[1]) ||
            game_compat_veh_float_is_non_zero_or_nan(vehicleState->velocity[2]) || !vehGroundTraceWalkable) {
            VEH_GroundTrace(ent);
            if (vehGroundTraceWalkable) {
                VEH_GroundMove(ent);
            } else {
                VEH_AirMove(ent);
            }
        }

        MatrixTransposeTransformVector43(vehicleState->velocity, &axis, vehicleState->angularVelocity);

        if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL || vehicleInfo->type == VEHICLE_TYPE_TANK) {
            VEH_UpdateSuspension(ent, qtrue);
        }
    }

    game_compat_veh_set_entity_current_speed(ent, fabsf(vehicleState->angularVelocity[0]));

    {
        /* 0x88383..0x88492: the max/clamp cascade compares the unrounded
         * 80-bit quotients (clamp boundary is DOUBLE 1.0); the selected
         * quotient rounds to double, then to float. */
        const long double forwardFraction = fabsf(vehicleState->angularVelocity[0]) / vehicleInfo->maxSpeed;
        const long double steerFraction = fabsf(vehicleState->acceleration[1]) / (vehicleInfo->steeringLimit + vehicleInfo->steeringLimit);
        float clampedSpeedFraction;

        if (forwardFraction > steerFraction) {
            if (forwardFraction >= 1.0) {
                clampedSpeedFraction = 1.0f;
            } else {
                clampedSpeedFraction = (float)(double)forwardFraction;
            }
        } else {
            if (steerFraction >= 1.0) {
                clampedSpeedFraction = 1.0f;
            } else {
                clampedSpeedFraction = (float)(double)steerFraction;
            }
        }

        vehicleState->idleSoundBlendRepeatDelay =
            VEH_LerpValue(1.0f - clampedSpeedFraction, vehicleState->idleSoundBlendRepeatDelay, VEH_SPEED_BLEND_RATE);
        vehicleState->runSoundBlendRepeatDelay =
            VEH_LerpValue(clampedSpeedFraction, vehicleState->runSoundBlendRepeatDelay, VEH_SPEED_BLEND_RATE);
    }

    if (g_vehicleDebug.integer != 0) {
        VEH_DebugCircleVertical(vehicleState->origin, vehicleInfo->collisionMaxs[0], vehicleInfo->collisionMaxs[2], 1.0f, 1.0f, 0.0f);
    }
}

/* ------------------------------------------------------------------ */
/*  0x7f932  VEH_ClipVelocity                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7f932, 8f932_FUN_0008f932.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out)
{
    float backoff;

    /* 0x7f945..0x7f998 reaches the ground-plane shortcut only after two
     * ordered-above-or-equal branches.  Writing the other arm as `< || <`
     * incorrectly selects the shortcut when either comparison is unordered. */
    if (!(normal[2] >= VEH_GROUND_NORMAL_MIN_Z && in[0] * in[0] + in[1] * in[1] >= in[2] * in[2])) {
        backoff = in[0] * normal[0] + in[1] * normal[1] + in[2] * normal[2];
        if (backoff < 0.0f) {
            backoff *= VEH_SLIDE_OVERCLIP;
        } else {
            backoff /= VEH_SLIDE_OVERCLIP;
        }

        for (int axis = 0; axis < 3; axis++) {
            /* 0x7fa70..0x7fa99: the product is stored to a float slot
             * before the subtract (two roundings per component). */
            const float scaledNormal = normal[axis] * backoff;
            out[axis] = in[axis] - scaledNormal;
        }
        return;
    }

    /* 0x7f9c2: machine stores out[2] BEFORE out[0]/out[1].  Callers pass
     * the same array as in and out, so this order is load-bearing: out[2]
     * must be computed from the still-unclobbered in[0]/in[1]. */
    out[2] = -in[0] * normal[0] - in[1] * normal[1];
    out[0] = in[0] * normal[2];
    out[1] = in[1] * normal[2];
}

/* ------------------------------------------------------------------ */
/*  0x7faaa  VEH_CorrectAllSolid                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7faaa, 8faaa_FUN_0008faaa.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_CorrectAllSolid(gentity_t *ent, trace_t *trace)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    vec3_t probe;

    for (int offsetIndex = 0; offsetIndex < VEH_UNSTICK_VECTOR_COUNT; offsetIndex++) {
        probe[0] = vehicleState->origin[0] + vehUnstickOffsets[offsetIndex * 3 + 0];
        probe[1] = vehicleState->origin[1] + vehUnstickOffsets[offsetIndex * 3 + 1];
        probe[2] = vehicleState->origin[2] + vehUnstickOffsets[offsetIndex * 3 + 2];

        trap_Trace(trace, probe, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, probe, ent->s.number, ent->clipmask);
        if (trace->startsolid == 0) {
            break;
        }
    }

    if (trace->startsolid != 0) {
        return qfalse;
    }

    vehicleState->origin[0] = probe[0];
    vehicleState->origin[1] = probe[1];
    vehicleState->origin[2] = probe[2];

    probe[0] = vehicleState->origin[0];
    probe[1] = vehicleState->origin[1];
    probe[2] = vehicleState->origin[2] - VEH_GROUND_TRACE_DOWN_STEP;

    trap_Trace(trace, vehicleState->origin, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, probe, ent->s.number, ent->clipmask);
    memcpy(&vehLastGroundTrace, trace, sizeof(trace_t));

    vehicleState->origin[0] = trace->endpos[0];
    vehicleState->origin[1] = trace->endpos[1];
    vehicleState->origin[2] = trace->endpos[2];
    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x7fc99  VEH_GroundTrace                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7fc99, 8fc99_FUN_0008fc99.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_GroundTrace(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    trace_t trace;
    vec3_t start;
    vec3_t end;

    start[0] = vehicleState->origin[0];
    start[1] = vehicleState->origin[1];
    start[2] = vehicleState->origin[2] + VEH_GROUND_PROBE_STEP;

    end[0] = vehicleState->origin[0];
    end[1] = vehicleState->origin[1];
    if (vehicleState->passengerEntityNums[1] == ENTITYNUM_NONE) {
        end[2] = vehicleState->origin[2] - vehicleInfo->suspensionTravel;
    } else {
        end[2] = vehicleState->origin[2] - VEH_GROUND_PROBE_FALLBACK;
    }

    trap_Trace(&trace, start, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, end, ent->s.number, ent->clipmask);
    memcpy(&vehLastGroundTrace, &trace, sizeof(trace_t));

    vehGroundTraceHit = qfalse;
    vehGroundTraceWalkable = qfalse;

    /* 0x7fdca: machine reads trace+0x2e (allsolid), not +0x2f (startsolid). */
    if ((trace.allsolid == 0 || VEH_CorrectAllSolid(ent, &trace)) && trace.fraction != 1.0f) {
        qboolean acceptableImpact = qtrue;

        /* 0x7fe02..0x7fe42 performs the dot rejection only after an
         * ordered velocity-z > 0 branch.  Unordered velocity or dot values
         * therefore remain acceptable. */
        if (vehicleState->velocity[2] > 0.0f && vehicleState->velocity[0] * trace.normal[0] + vehicleState->velocity[1] * trace.normal[1] +
                                                        vehicleState->velocity[2] * trace.normal[2] >
                                                    VEH_GROUND_MAX_IMPACT_DOT) {
            acceptableImpact = qfalse;
        }

        if (!acceptableImpact) {
            return;
        }

        vehGroundTraceHit = qtrue;
        /* 0x7fe44..0x7fe62 rejects walkability only through ordered
         * normal-z < 0.7; unordered normals take the walkable arm. */
        if (!(trace.normal[2] < VEH_GROUND_NORMAL_MIN_Z)) {
            vehGroundTraceWalkable = qtrue;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x8036e  VEH_SlideMove                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8036e, 9036e_VEH_SlideMove.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_SlideMove(gentity_t *ent, qboolean gravity, int *touchEnts, int maxTouchEnts)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    float timeLeft = VEH_PHYSICS_FRAME_SECONDS;
    vec3_t endVelocity = {0.0f, 0.0f, 0.0f};
    float planes[VEH_SLIDE_MAX_CLIP_PLANES][3];
    int numPlanes = 0;
    int touchCount = 0;
    int bump;

    if (gravity) {
        endVelocity[0] = vehicleState->velocity[0];
        endVelocity[1] = vehicleState->velocity[1];
        endVelocity[2] = vehicleState->velocity[2] - VEH_SLIDE_GRAVITY_STEP;

        vehicleState->velocity[2] = (vehicleState->velocity[2] + endVelocity[2]) * 0.5f;

        if (vehGroundTraceHit) {
            VEH_ClipVelocity(vehicleState->velocity, vehLastGroundTrace.normal, vehicleState->velocity);
        }
    }

    if (vehGroundTraceHit) {
        planes[0][0] = vehLastGroundTrace.normal[0];
        planes[0][1] = vehLastGroundTrace.normal[1];
        planes[0][2] = vehLastGroundTrace.normal[2];
        numPlanes = 1;
    }

    VectorNormalize2(vehicleState->velocity, planes[numPlanes]);
    numPlanes++;

    for (int index = 0; index < maxTouchEnts; index++) {
        touchEnts[index] = ENTITYNUM_NONE;
    }

    for (bump = 0; bump < VEH_SLIDE_MAX_BUMPS; bump++) {
        trace_t trace;
        vec3_t end;

        end[0] = vehicleState->origin[0] + vehicleState->velocity[0] * timeLeft;
        end[1] = vehicleState->origin[1] + vehicleState->velocity[1] * timeLeft;
        end[2] = vehicleState->origin[2] + vehicleState->velocity[2] * timeLeft;

        trap_Trace(&trace, vehicleState->origin, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, end, ent->s.number, ent->clipmask);

        if (trace.startsolid != 0) {
            vehicleState->velocity[2] = 0.0f;
            return qtrue;
        }

        if (trace.fraction > 0.0f) {
            vehicleState->origin[0] = trace.endpos[0];
            vehicleState->origin[1] = trace.endpos[1];
            vehicleState->origin[2] = trace.endpos[2];
        }

        if (trace.fraction == 1.0f) {
            break;
        }

        if (touchEnts != NULL && trace.entityNum != ENTITYNUM_WORLD && touchCount < maxTouchEnts) {
            touchEnts[touchCount] = trace.entityNum;
            touchCount++;
        }

        timeLeft -= timeLeft * trace.fraction;
        if (numPlanes >= VEH_SLIDE_MAX_CLIP_PLANES) {
            game_compat_veh_clear_vehicle_velocity(vehicleState);
            return qtrue;
        }

        int planeIndex;
        for (planeIndex = 0; planeIndex < numPlanes; planeIndex++) {
            const float dot =
                trace.normal[0] * planes[planeIndex][0] + trace.normal[1] * planes[planeIndex][1] + trace.normal[2] * planes[planeIndex][2];

            if (dot > VEH_SLIDE_RECOLLIDE_DOT) {
                vehicleState->velocity[0] += trace.normal[0];
                vehicleState->velocity[1] += trace.normal[1];
                vehicleState->velocity[2] += trace.normal[2];
                break;
            }
        }

        if (planeIndex < numPlanes) {
            continue;
        }

        planes[numPlanes][0] = trace.normal[0];
        planes[numPlanes][1] = trace.normal[1];
        planes[numPlanes][2] = trace.normal[2];
        numPlanes++;

        for (planeIndex = 0; planeIndex < numPlanes; planeIndex++) {
            vec3_t newVelocity;
            vec3_t clippedEndVelocity;
            const float into = vehicleState->velocity[0] * planes[planeIndex][0] + vehicleState->velocity[1] * planes[planeIndex][1] +
                               vehicleState->velocity[2] * planes[planeIndex][2];

            if (into >= VEH_SLIDE_CLIP_EPSILON) {
                continue;
            }

            VEH_ClipVelocity(vehicleState->velocity, planes[planeIndex], newVelocity);
            VEH_ClipVelocity(endVelocity, planes[planeIndex], clippedEndVelocity);

            for (int otherPlane = 0; otherPlane < numPlanes; otherPlane++) {
                if (otherPlane == planeIndex) {
                    continue;
                }

                if (newVelocity[0] * planes[otherPlane][0] + newVelocity[1] * planes[otherPlane][1] +
                        newVelocity[2] * planes[otherPlane][2] >=
                    VEH_SLIDE_CLIP_EPSILON) {
                    continue;
                }

                VEH_ClipVelocity(newVelocity, planes[otherPlane], newVelocity);
                VEH_ClipVelocity(clippedEndVelocity, planes[otherPlane], clippedEndVelocity);

                /* 0x80a48..0x80a68 skips this arm only through an ordered
                 * dot >= 0 branch; an unordered dot enters the crease path. */
                if (!(newVelocity[0] * planes[planeIndex][0] + newVelocity[1] * planes[planeIndex][1] +
                          newVelocity[2] * planes[planeIndex][2] >=
                      0.0f)) {
                    vec3_t crease;
                    float projection;

                    CrossProduct(planes[planeIndex], planes[otherPlane], crease);
                    VectorNormalize(crease);

                    projection = crease[0] * vehicleState->velocity[0] + crease[1] * vehicleState->velocity[1] +
                                 crease[2] * vehicleState->velocity[2];
                    newVelocity[0] = crease[0] * projection;
                    newVelocity[1] = crease[1] * projection;
                    newVelocity[2] = crease[2] * projection;

                    CrossProduct(planes[planeIndex], planes[otherPlane], crease);
                    VectorNormalize(crease);

                    projection = crease[0] * endVelocity[0] + crease[1] * endVelocity[1] + crease[2] * endVelocity[2];
                    clippedEndVelocity[0] = crease[0] * projection;
                    clippedEndVelocity[1] = crease[1] * projection;
                    clippedEndVelocity[2] = crease[2] * projection;

                    for (int thirdPlane = 0; thirdPlane < numPlanes; thirdPlane++) {
                        if (thirdPlane == planeIndex || thirdPlane == otherPlane) {
                            continue;
                        }

                        /* 0x80c71..0x80c8d clears velocity unless an
                         * ordered dot >= epsilon branch succeeds. */
                        if (!(newVelocity[0] * planes[thirdPlane][0] + newVelocity[1] * planes[thirdPlane][1] +
                                  newVelocity[2] * planes[thirdPlane][2] >=
                              VEH_SLIDE_CLIP_EPSILON)) {
                            game_compat_veh_clear_vehicle_velocity(vehicleState);
                            return qtrue;
                        }
                    }
                }
            }

            newVelocity[2] = 0.0f;
            clippedEndVelocity[2] = 0.0f;
            vehicleState->velocity[0] = newVelocity[0];
            vehicleState->velocity[1] = newVelocity[1];
            vehicleState->velocity[2] = newVelocity[2];
            endVelocity[0] = clippedEndVelocity[0];
            endVelocity[1] = clippedEndVelocity[1];
            endVelocity[2] = clippedEndVelocity[2];
            break;
        }
    }

    if (gravity) {
        vehicleState->velocity[0] = endVelocity[0];
        vehicleState->velocity[1] = endVelocity[1];
        vehicleState->velocity[2] = endVelocity[2];
    }

    return bump != 0;
}

/* ------------------------------------------------------------------ */
/*  0x80d93  VEH_StepSlideMove                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x80d93, 90d93_FUN_00090d93.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_StepSlideMove(gentity_t *ent, qboolean gravity)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    vec3_t oldOrigin;
    vec3_t oldVelocity;
    int touchEnts[VEH_STEP_TOUCH_COUNT];
    trace_t trace;
    vec3_t end;

    oldOrigin[0] = vehicleState->origin[0];
    oldOrigin[1] = vehicleState->origin[1];
    oldOrigin[2] = vehicleState->origin[2];
    oldVelocity[0] = vehicleState->velocity[0];
    oldVelocity[1] = vehicleState->velocity[1];
    oldVelocity[2] = vehicleState->velocity[2];

    if (!VEH_SlideMove(ent, gravity, touchEnts, VEH_STEP_TOUCH_COUNT)) {
        return;
    }

    end[0] = oldOrigin[0];
    end[1] = oldOrigin[1];
    end[2] = oldOrigin[2] - vehicleInfo->stepSize;
    trap_Trace(&trace, oldOrigin, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, end, ent->s.number, ent->clipmask);

    if (vehicleState->velocity[2] > 0.0f && (trace.fraction == 1.0f || trace.normal[2] < VEH_GROUND_NORMAL_MIN_Z)) {
        return;
    }

    for (int index = 0; index < VEH_STEP_TOUCH_COUNT && touchEnts[index] != ENTITYNUM_NONE; index++) {
        gentity_t *touchEnt = &g_entities[touchEnts[index]];

        if (touchEnt->s.eType == ET_VEHICLE) {
            return;
        }

        if (game_compat_veh_entity_model_classname(touchEnt) == scr_const_script_model) {
            Scr_AddEntity(ent);
            Scr_Notify(touchEnt, scr_const_trigger, 1);
        }
    }

    end[0] = oldOrigin[0];
    end[1] = oldOrigin[1];
    end[2] = oldOrigin[2] + vehicleInfo->stepSize;
    trap_Trace(&trace, oldOrigin, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, end, ent->s.number, ent->clipmask);

    if (trace.startsolid != 0) {
        return;
    }

    vehicleState->origin[0] = trace.endpos[0];
    vehicleState->origin[1] = trace.endpos[1];
    vehicleState->origin[2] = trace.endpos[2];
    vehicleState->velocity[0] = oldVelocity[0];
    vehicleState->velocity[1] = oldVelocity[1];
    vehicleState->velocity[2] = oldVelocity[2];

    VEH_SlideMove(ent, gravity, NULL, 0);

    end[0] = vehicleState->origin[0];
    end[1] = vehicleState->origin[1];
    /* 0x810b7: the 80-bit chain groups (oldOrigin - endpos) first, then
     * adds the current origin. */
    end[2] = (oldOrigin[2] - trace.endpos[2]) + vehicleState->origin[2];
    trap_Trace(&trace, vehicleState->origin, vehicleInfo->collisionMins, vehicleInfo->collisionMaxs, end, ent->s.number, ent->clipmask);

    if (trace.startsolid == 0) {
        vehicleState->origin[0] = trace.endpos[0];
        vehicleState->origin[1] = trace.endpos[1];
        vehicleState->origin[2] = trace.endpos[2];
    }

    if (trace.fraction < 1.0f) {
        VEH_ClipVelocity(vehicleState->velocity, trace.normal, vehicleState->velocity);
    }
}

/* ------------------------------------------------------------------ */
/*  0x81175  VEH_GroundMove                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x81175, 91175_FUN_00091175.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_GroundMove(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    vec3_t originalVelocity;
    const float speed = game_compat_veh_length3(vehicleState->velocity);

    originalVelocity[0] = vehicleState->velocity[0];
    originalVelocity[1] = vehicleState->velocity[1];
    originalVelocity[2] = vehicleState->velocity[2];

    VEH_ClipVelocity(vehicleState->velocity, vehLastGroundTrace.normal, vehicleState->velocity);

    if (vehicleState->velocity[0] * originalVelocity[0] + vehicleState->velocity[1] * originalVelocity[1] +
            vehicleState->velocity[2] * originalVelocity[2] >
        0.0f) {
        VectorNormalize(vehicleState->velocity);
        vehicleState->velocity[0] *= speed;
        vehicleState->velocity[1] *= speed;
        vehicleState->velocity[2] *= speed;
    }

    if (game_compat_veh_float_is_non_zero_or_nan(vehicleState->velocity[0]) ||
        game_compat_veh_float_is_non_zero_or_nan(vehicleState->velocity[1])) {
        VEH_StepSlideMove(ent, qfalse);
    }
}

/* ------------------------------------------------------------------ */
/*  0x812c4  VEH_AirMove                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x812c4, 912c4_FUN_000912c4.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_AirMove(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    if (vehGroundTraceHit) {
        VEH_ClipVelocity(vehicleState->velocity, vehLastGroundTrace.normal, vehicleState->velocity);
    }

    VEH_StepSlideMove(ent, qtrue);
}

/* ------------------------------------------------------------------ */
/*  0x81331  VEH_UpdateSuspension                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x81331, 91331_FUN_00091331.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_UpdateSuspension(gentity_t *ent, qboolean livePhysicsPass)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const int wheelCount = vehicleInfo->type == VEHICLE_TYPE_4_WHEEL ? VEH_SUSPENSION_WHEEL_COUNT_4WHEEL : VEH_SUSPENSION_WHEEL_COUNT_TANK;
    matrix43_t axis;
    float wheelPoints[VEH_SUSPENSION_WHEEL_COUNT_TANK][3];
    vec3_t traceStart;
    vec3_t traceEnd;
    vec3_t pairSumA;
    vec3_t pairSumB;
    vec3_t sideAxis;
    vec3_t lengthAxis;
    vec3_t groundNormal;
    float planeDistance;
    vec3_t projectedAxis;
    vec3_t targetAngles;

    /*
     * Stock callers pass false during setup/path attachment and true during
     * live vehicle physics.  The Linux body does not read this argument.
     */
    (void)livePhysicsPass;

    AnglesToAxis(vehicleState->viewClampTargetAngles, axis.axis);
    axis.origin[0] = vehicleState->origin[0];
    axis.origin[1] = vehicleState->origin[1];
    axis.origin[2] = vehicleState->previousOrigin[2];

    for (int wheelIndex = 0; wheelIndex < wheelCount; wheelIndex++) {
        trace_t trace;
        vec3_t localWheel;

        VEH_GetWheelOrigin(ent, wheelIndex, localWheel);
        MatrixTransformVector43(localWheel, &axis, traceStart);

        if (g_vehicleDebug.integer != 0) {
            VEH_DebugBox(traceStart, 4.0f, 1.0f, 0.0f, 0.0f);
        }

        traceEnd[0] = traceStart[0];
        traceEnd[1] = traceStart[1];
        traceEnd[2] = traceStart[2] - VEH_SUSPENSION_TRACE_DOWN;
        traceStart[2] += VEH_SUSPENSION_TRACE_UP;

        if (g_vehicleDebug.integer != 0) {
            VEH_DebugLine(traceStart, traceEnd, 0.0f, 0.0f, 1.0f);
        }

        if (game_compat_veh_entity_collision_mode(ent) == VEH_COLLISION_MODE_ALT_TRACE) {
            trap_Trace(&trace, traceStart, NULL, NULL, traceEnd, ent->s.number, VEH_SUSPENSION_TRACE_MASK_ALT);
        } else {
            trap_Trace(&trace, traceStart, NULL, NULL, traceEnd, ent->s.number, VEH_SUSPENSION_TRACE_MASK);
        }

        if (trace.fraction < 1.0f) {
            wheelPoints[wheelIndex][0] = trace.endpos[0];
            wheelPoints[wheelIndex][1] = trace.endpos[1];
            wheelPoints[wheelIndex][2] = trace.endpos[2];
            game_compat_veh_set_wheel_surface_at(
                vehicleState, (uint32_t)wheelIndex,
                (int32_t)((trace.surfaceFlags & (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >> SURFACE_TYPE_SHIFT));
        } else {
            wheelPoints[wheelIndex][0] = traceEnd[0];
            wheelPoints[wheelIndex][1] = traceEnd[1];
            wheelPoints[wheelIndex][2] = traceEnd[2];
            game_compat_veh_set_wheel_surface_at(vehicleState, (uint32_t)wheelIndex, 0);
        }

        vehicleState->wheelGroundZ[wheelIndex] = wheelPoints[wheelIndex][2];
        vehicleState->wheelVerticalVelocity[wheelIndex] = 0.0f;

        if (g_vehicleDebug.integer != 0) {
            VEH_DebugBox(wheelPoints[wheelIndex], 4.0f, 0.0f, 1.0f, 0.0f);
        }
    }

    /* 0x8171d..0x8181f: each pair sum is stored to a float slot, scaled in
     * place, and only then subtracted; keep the intermediate roundings. */
    pairSumA[0] = wheelPoints[1][0] + wheelPoints[3][0];
    pairSumA[1] = wheelPoints[1][1] + wheelPoints[3][1];
    pairSumA[2] = wheelPoints[1][2] + wheelPoints[3][2];
    pairSumB[0] = wheelPoints[0][0] + wheelPoints[2][0];
    pairSumB[1] = wheelPoints[0][1] + wheelPoints[2][1];
    pairSumB[2] = wheelPoints[0][2] + wheelPoints[2][2];
    pairSumA[0] *= 0.5f;
    pairSumA[1] *= 0.5f;
    pairSumA[2] *= 0.5f;
    pairSumB[0] *= 0.5f;
    pairSumB[1] *= 0.5f;
    pairSumB[2] *= 0.5f;
    sideAxis[0] = pairSumA[0] - pairSumB[0];
    sideAxis[1] = pairSumA[1] - pairSumB[1];
    sideAxis[2] = pairSumA[2] - pairSumB[2];
    VectorNormalize(sideAxis);

    /* 0x81835..0x81937: same spill structure as sideAxis above. */
    pairSumA[0] = wheelPoints[0][0] + wheelPoints[1][0];
    pairSumA[1] = wheelPoints[0][1] + wheelPoints[1][1];
    pairSumA[2] = wheelPoints[0][2] + wheelPoints[1][2];
    pairSumB[0] = wheelPoints[2][0] + wheelPoints[3][0];
    pairSumB[1] = wheelPoints[2][1] + wheelPoints[3][1];
    pairSumB[2] = wheelPoints[2][2] + wheelPoints[3][2];
    pairSumA[0] *= 0.5f;
    pairSumA[1] *= 0.5f;
    pairSumA[2] *= 0.5f;
    pairSumB[0] *= 0.5f;
    pairSumB[1] *= 0.5f;
    pairSumB[2] *= 0.5f;
    lengthAxis[0] = pairSumA[0] - pairSumB[0];
    lengthAxis[1] = pairSumA[1] - pairSumB[1];
    lengthAxis[2] = pairSumA[2] - pairSumB[2];
    VectorNormalize(lengthAxis);

    CrossProduct(sideAxis, lengthAxis, groundNormal);
    planeDistance = wheelPoints[0][0] * groundNormal[0] + wheelPoints[0][1] * groundNormal[1] + wheelPoints[0][2] * groundNormal[2];

    for (int wheelIndex = 1; wheelIndex < wheelCount; wheelIndex++) {
        /* 0x819ba..0x81ab7: the dot product stays in the 80-bit chain up to
         * the subtraction (never rounded on its own), and the update path
         * recomputes the full chain minus suspensionTravel. */
        const float wheelDistance = (wheelPoints[wheelIndex][0] * groundNormal[0] + wheelPoints[wheelIndex][1] * groundNormal[1] +
                                     wheelPoints[wheelIndex][2] * groundNormal[2]) -
                                    planeDistance;

        if (vehicleInfo->suspensionTravel < wheelDistance) {
            planeDistance = (wheelPoints[wheelIndex][0] * groundNormal[0] + wheelPoints[wheelIndex][1] * groundNormal[1] +
                             wheelPoints[wheelIndex][2] * groundNormal[2]) -
                            vehicleInfo->suspensionTravel;
        }
    }

    CrossProduct(groundNormal, axis.axis[0], projectedAxis);
    VectorNormalize(projectedAxis);
    CrossProduct(projectedAxis, groundNormal, axis.axis[0]);
    VectorNormalize(axis.axis[0]);
    AxisToAngles((const vec_t(*)[3])axis.axis, targetAngles);

    vehicleState->viewClampTargetAngles[0] = VEH_LerpAngle(targetAngles[0], vehicleState->previousAngles[0], VEH_SUSPENSION_ANGLE_RATE);
    vehicleState->viewClampTargetAngles[2] = VEH_LerpAngle(targetAngles[2], vehicleState->previousAngles[2], VEH_SUSPENSION_ANGLE_RATE);

    vehicleState->viewClampTargetAngles[0] = game_compat_veh_clamp_abs(vehicleState->viewClampTargetAngles[0], VEH_SUSPENSION_ANGLE_CLAMP);
    vehicleState->viewClampTargetAngles[2] = game_compat_veh_clamp_abs(vehicleState->viewClampTargetAngles[2], VEH_SUSPENSION_ANGLE_CLAMP);

    if (game_compat_veh_entity_collision_mode(ent) != VEH_COLLISION_MODE_ALT_TRACE) {
        vehicleState->origin[2] =
            -((vehicleState->origin[0] * groundNormal[0] + vehicleState->origin[1] * groundNormal[1]) - planeDistance) / groundNormal[2];
    }

    AnglesSubtract(vehicleState->viewClampTargetAngles, vehicleState->previousAngles, vehicleState->acceleration);
    vehicleState->acceleration[0] *= VEH_SUSPENSION_ANGULAR_SCALE;
    vehicleState->acceleration[1] *= VEH_SUSPENSION_ANGULAR_SCALE;
    vehicleState->acceleration[2] *= VEH_SUSPENSION_ANGULAR_SCALE;

    if (g_vehicleDebug.integer != 0) {
        float projected[VEH_SUSPENSION_WHEEL_COUNT_4WHEEL][3];

        for (int wheelIndex = 0; wheelIndex < VEH_SUSPENSION_WHEEL_COUNT_4WHEEL; wheelIndex++) {
            projected[wheelIndex][0] = wheelPoints[wheelIndex][0];
            projected[wheelIndex][1] = wheelPoints[wheelIndex][1];
            projected[wheelIndex][2] =
                -((projected[wheelIndex][0] * groundNormal[0] + projected[wheelIndex][1] * groundNormal[1]) - planeDistance) /
                groundNormal[2];
        }

        VEH_DebugLine(projected[0], projected[1], 1.0f, 1.0f, 0.0f);
        VEH_DebugLine(projected[1], projected[3], 1.0f, 1.0f, 0.0f);
        VEH_DebugLine(projected[3], projected[2], 1.0f, 1.0f, 0.0f);
        VEH_DebugLine(projected[2], projected[0], 1.0f, 1.0f, 0.0f);
    }
}

/* ------------------------------------------------------------------ */
/*  0x7fe81  VEH_GroundFriction                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7fe81, 8fe81_VEH_GroundFriction.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_GroundFriction(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    axis_t axis;
    vec3_t localVelocity;
    vec3_t normalizedVelocity;
    float sideSpeed;

    AnglesToAxis(vehicleState->viewClampTargetAngles, axis);
    game_compat_veh_project_velocity_to_local_axis(vehicleState, axis, localVelocity);

    sideSpeed = fabsf(localVelocity[1]);
    if (sideSpeed > VEH_GROUND_SIDE_CLAMP_SPEED) {
        sideSpeed = VEH_GROUND_SIDE_CLAMP_SPEED;
    }

    VectorNormalize2(vehicleState->velocity, normalizedVelocity);
    vehicleState->velocity[0] += -sideSpeed * VEH_GROUND_FRICTION_SCALE * normalizedVelocity[0];
    vehicleState->velocity[1] += -sideSpeed * VEH_GROUND_FRICTION_SCALE * normalizedVelocity[1];
    vehicleState->velocity[2] += -sideSpeed * VEH_GROUND_FRICTION_SCALE * normalizedVelocity[2];

    game_compat_veh_project_velocity_to_local_axis(vehicleState, axis, localVelocity);

    if (vehicleInfo->type == VEHICLE_TYPE_TANK && fabsf(localVelocity[1]) > VEH_GROUND_SIDE_CLAMP_SPEED &&
        fabsf(localVelocity[0]) < fabsf(localVelocity[1])) {
        vehicleState->velocity[0] = 0.0f;
        vehicleState->velocity[1] = 0.0f;
        vehicleState->velocity[2] = 0.0f;

        game_compat_veh_add_scaled_axis(vehicleState->velocity, axis, 0, localVelocity[0]);
        if (localVelocity[1] > 0.0f) {
            game_compat_veh_add_scaled_axis(vehicleState->velocity, axis, 1, VEH_GROUND_SIDE_CLAMP_SPEED);
        } else {
            game_compat_veh_add_scaled_axis(vehicleState->velocity, axis, 1, -VEH_GROUND_SIDE_CLAMP_SPEED);
        }
        game_compat_veh_add_scaled_axis(vehicleState->velocity, axis, 2, localVelocity[2]);
    }

    if (localVelocity[0] > VEH_GROUND_STEER_MIN_FORWARD && localVelocity[1] > VEH_GROUND_STEER_MIN_SIDE) {
        /* 0x80266: the dot product is rounded to double for CoduoLibm_Sqrt(), not to
         * float for sqrtf(). */
        const float speed = game_compat_veh_length3(vehicleState->velocity);
        vec3_t correction;
        float correctionLength;

        /* 0x80271..0x802ad: the products are stored to float slots first and
         * the velocity is subtracted in place (two roundings per component). */
        correction[0] = axis[0][0] * speed;
        correction[1] = axis[0][1] * speed;
        correction[2] = axis[0][2] * speed;
        correction[0] -= vehicleState->velocity[0];
        correction[1] -= vehicleState->velocity[1];
        correction[2] -= vehicleState->velocity[2];

        correctionLength = VectorNormalize(correction);
        if (correctionLength > VEH_GROUND_STEER_MAX_STEP) {
            correctionLength = VEH_GROUND_STEER_MAX_STEP;
        }

        vehicleState->velocity[0] += correction[0] * correctionLength;
        vehicleState->velocity[1] += correction[1] * correctionLength;
        vehicleState->velocity[2] += correction[2] * correctionLength;

        VectorNormalize(vehicleState->velocity);
        vehicleState->velocity[0] *= speed;
        vehicleState->velocity[1] *= speed;
        vehicleState->velocity[2] *= speed;
    }
}

/* ------------------------------------------------------------------ */
/*  0x87a13  VEH_PhysicsNotRequired                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x87a13, 97a13_VEH_PhysicsNotRequired.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_PhysicsNotRequired(gentity_t *ent, qboolean checkNearbyClients)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)ent->vehicle;

    if (game_compat_veh_vector3_is_non_zero(vehicleState->acceleration) || game_compat_veh_vector3_is_non_zero(vehicleState->velocity) ||
        game_compat_veh_vector3_is_non_zero(vehicleState->externalVelocity) ||
        game_compat_veh_vector3_is_non_zero(vehicleState->angularAcceleration) ||
        game_compat_veh_vector3_is_non_zero(vehicleState->angularVelocity)) {
        return qfalse;
    }

    if (!checkNearbyClients) {
        return qtrue;
    }

    for (int clientNum = 0; clientNum < VEH_PHYSICS_CLIENT_COUNT; clientNum++) {
        vehicle_state_t *clientPhysics = &vehClientPhysicsRecords[clientNum];
        const int entityNum = clientPhysics->entityNum;

        if (entityNum != ENTITYNUM_NONE &&
            VectorDistanceSquared(game_compat_veh_physics_client_origin(clientPhysics), vehicleState->origin) <
                VEH_PHYSICS_NEARBY_RADIUS_SQ &&
            !VEH_PhysicsNotRequired(&g_entities[entityNum], qfalse)) {
            return qfalse;
        }
    }

    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x81f2e  VEH_Backup                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x81f2e, 91f2e_FUN_00091f2e.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_Backup(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;

    vehicleState->previousOrigin[0] = ent->currentOrigin[0];
    vehicleState->previousOrigin[1] = ent->currentOrigin[1];
    vehicleState->previousOrigin[2] = ent->currentOrigin[2];
    vehicleState->previousAngles[0] = ent->currentAngles[0];
    vehicleState->previousAngles[1] = ent->currentAngles[1];
    vehicleState->previousAngles[2] = ent->currentAngles[2];

    memcpy(&vehPhysicsSnapshotPrefix, vehicleState, sizeof(vehPhysicsSnapshotPrefix));
    memcpy(&vehPhysicsSnapshotState, &vehicleState->origin, sizeof(vehPhysicsSnapshotState));
}

/* ------------------------------------------------------------------ */
/*  0x81ffd  VEH_PlayerDamage                                         */
/* ------------------------------------------------------------------ */

/*
 * Apply damage to a player from a vehicle collision.
 *
 * Determines the appropriate means of death (MOD) based on vehicle type,
 * resolves the attacker (vehicle driver or vehicle itself), and calls
 * G_Damage to apply the damage.
 *
 * RECOVERED(UO-GAME-UNK-0167): Vehicle info table at DAT_000f3ec0 contains
 * vehicle type data with stride 0x3dc. The type at offset +0x40 determines
 * whether to use MOD_CRUSH_TANK or MOD_CRUSH_JEEP.
 */
/* VERIFIED_DECOMPILER(0x81ffd, 91ffd_VEH_PlayerDamage.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_PlayerDamage(gentity_t *player, gentity_t *vehicle, int damage)
{
    const vehicleInfo_t *vehicleInfo = NULL;
    int mod;
    gentity_t *attacker;

    /* Get vehicle info from vehicle entity */
    if (vehicle->vehicle != NULL) {
        const vehicle_state_t *vehicleState = (const vehicle_state_t *)vehicle->vehicle;
        vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    }

    /* Determine MOD based on vehicle type */
    if (vehicleInfo == NULL) {
        /* Stock fallback uses MOD_SUICIDE when vehicle info is unavailable. */
        G_Damage(player, vehicle, vehicle, vec3_origin, player->currentOrigin, damage, 0, MOD_SUICIDE, 0);
        return;
    }

    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL) {
        mod = MOD_CRUSH_JEEP;
    } else {
        mod = MOD_CRUSH_TANK;
    }

    /* Resolve attacker: vehicle driver or vehicle itself */
    if (vehicle->passEntityNum == ENTITYNUM_NONE) {
        attacker = vehicle;
    } else {
        attacker = &g_entities[vehicle->passEntityNum];
    }

    /* Apply damage to player */
    G_Damage(player, vehicle, attacker, vec3_origin, player->currentOrigin, damage, 0, mod, 0);
}

/* ------------------------------------------------------------------ */
/*  0x8213f  VEH_PlayerCollision                                      */
/* ------------------------------------------------------------------ */

/*
 * Handle collision between a vehicle and a player.
 *
 * Calculates knockback velocity based on vehicle speed and direction,
 * applies it to the player, and potentially calls VEH_PlayerDamage
 * if enough time has passed since the last collision damage.
 *
 * RECOVERED(UO-GAME-UNK-0168): Vehicle velocity is read from vehicle->vehicle
 * at offsets +0xe8 to +0xf0 (3 floats). Player collision cooldown is stored
 * in client->lastCollisionDamageTime.
 */
/* VERIFIED_DECOMPILER(0x8213f, 9213f_VEH_PlayerCollision.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_PlayerCollision(gentity_t *vehicle, gentity_t *player)
{
    gclient_t *client = player->client;
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)vehicle->vehicle;
    const vehicleInfo_t *vehicleInfo = &g_vehicleInfoTable[vehicleState->typeIndex];
    vec3_t vehicleVelocity;
    float vehicleSpeed;
    vec3_t direction;
    float speedFactor;
    float impactSpeedFactor;
    vec3_t knockback;
    float knockbackMagnitude;
    vec3_t totalVelocity;
    float totalSpeed;
    float damageImpulse;
    double verticalSpeedFactor;
    gentity_t *collisionVehicle;

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        return;
    }

    /* Get vehicle velocity */
    vehicleVelocity[0] = vehicleState->velocity[0];
    vehicleVelocity[1] = vehicleState->velocity[1];
    vehicleVelocity[2] = vehicleState->velocity[2];

    /* Calculate vehicle speed */
    vehicleSpeed = game_compat_veh_length3(vehicleVelocity);

    /* Return if vehicle is nearly stationary */
    if (vehicleSpeed < 0.01f) {
        return;
    }

    /* Calculate direction from vehicle to player */
    direction[0] = client->ps.psOrigin[0] - vehicleState->origin[0];
    direction[1] = client->ps.psOrigin[1] - vehicleState->origin[1];
    direction[2] = client->ps.psOrigin[2] - vehicleState->origin[2];

    /* Normalize direction */
    VectorNormalize(direction);
    knockback[0] = direction[0] * game_compat_veh_length3(vehicleState->velocity);
    knockback[1] = direction[1] * game_compat_veh_length3(vehicleState->velocity);
    knockback[2] = direction[2] * game_compat_veh_length3(vehicleState->velocity);

    /* Calculate speed factor (0.0 to 1.0) */
    knockbackMagnitude = game_compat_veh_length3(knockback);
    if (knockbackMagnitude < VEH_COLLISION_MAX_SPEED) {
        speedFactor = game_compat_veh_length3(knockback) / VEH_COLLISION_MAX_SPEED;
    } else {
        speedFactor = 1.0f;
    }
    impactSpeedFactor = speedFactor;

    /* Calculate damage impulse based on whether vehicle is moving toward player */
    if (knockback[0] * vehicleState->velocity[0] + knockback[1] * vehicleState->velocity[1] + knockback[2] * vehicleState->velocity[2] >
        0.01f) {
        damageImpulse = impactSpeedFactor * VEH_COLLISION_KNOCKBACK_BASE;
    } else {
        damageImpulse = 0.0f;
    }

    /* Clamp knockback magnitude if speed is low */
    knockbackMagnitude = game_compat_veh_length3(knockback);
    if (knockbackMagnitude < VEH_COLLISION_SPEED_THRESHOLD) {
        VectorNormalize(knockback);
        knockback[0] *= VEH_COLLISION_SPEED_THRESHOLD;
        knockback[1] *= VEH_COLLISION_SPEED_THRESHOLD;
        knockback[2] *= VEH_COLLISION_SPEED_THRESHOLD;
    }

    /* Apply 1.3x multiplier to knockback */
    knockback[0] *= VEH_COLLISION_KNOCKBACK_MULT;
    knockback[1] *= VEH_COLLISION_KNOCKBACK_MULT;
    knockback[2] *= VEH_COLLISION_KNOCKBACK_MULT;

    /* Add knockback to player velocity */
    client->ps.velocity[0] += knockback[0];
    client->ps.velocity[1] += knockback[1];
    client->ps.velocity[2] += knockback[2];

    /* Cap total player velocity magnitude */
    totalVelocity[0] = client->ps.velocity[0];
    totalVelocity[1] = client->ps.velocity[1];
    totalVelocity[2] = client->ps.velocity[2];
    totalSpeed = game_compat_veh_length3(totalVelocity);
    knockbackMagnitude = game_compat_veh_length3(knockback);
    if (totalSpeed > knockbackMagnitude * VEH_COLLISION_KNOCKBACK_MULT) {
        VectorNormalize(client->ps.velocity);
        client->ps.velocity[0] *= game_compat_veh_length3(knockback) * VEH_COLLISION_KNOCKBACK_MULT;
        client->ps.velocity[1] *= game_compat_veh_length3(knockback) * VEH_COLLISION_KNOCKBACK_MULT;
        client->ps.velocity[2] *= game_compat_veh_length3(knockback) * VEH_COLLISION_KNOCKBACK_MULT;
    }

    /* Set vertical boost based on applied knockback speed */
    knockbackMagnitude = game_compat_veh_length3(knockback);
    if (knockbackMagnitude < VEH_COLLISION_MAX_SPEED) {
        verticalSpeedFactor = (double)((long double)game_compat_veh_length3(knockback) / (long double)VEH_COLLISION_MAX_SPEED);
    } else {
        verticalSpeedFactor = 1.0;
    }
    client->ps.velocity[2] = VEH_COLLISION_VERTICAL_BASE + VEH_COLLISION_VERTICAL_SCALE * pow(verticalSpeedFactor, 2.0);

    collisionVehicle = vehicle;
    if (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] != ENTITYNUM_NONE) {
        collisionVehicle = &g_entities[vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER]];
    }
    if (collisionVehicle->client == NULL) {
        collisionVehicle = vehicle;
    }

    /* Check if enough time has passed for collision damage */
    if (client->lastCollisionDamageTime < level.time - VEH_COLLISION_DAMAGE_COOLDOWN) {
        vec3_t soundOrigin;

        VectorNormalize(knockback);

        /* Apply damage */
#if EMULATE_X87
        VEH_PlayerDamage(player, vehicle, x87f_store_i32_trunc(x87f_load_f32(damageImpulse)));
#elif defined(__x86_64__)
        VEH_PlayerDamage(player, vehicle, CODUO_X87_TRUNCATE_I32((long double)damageImpulse));
#else
        VEH_PlayerDamage(player, vehicle, game_compat_veh_round_to_int(damageImpulse));
#endif

        /* The original Linux body at VEH_PlayerCollision+0x52d tests bit 1
         * (PMF_PRONE), not the crouch bit, before playing hitPersonSound with
         * a speed-scaled vertical offset and looped-FX server flag. */
        if ((player->client->ps.playerStateFlags & PMF_PRONE) == 0) {
            gentity_t *soundEnt;
            vec3_t soundOffset;

            /* 0x82703..0x82757: the offset vector is built in its own float
             * slots (so the z term rounds once there) and then added to the
             * origin per component. */
            soundOffset[0] = 0.0f;
            soundOffset[1] = 0.0f;
            soundOffset[2] = 0.0f;
            soundOffset[2] += (1.0f - impactSpeedFactor) * VEH_COLLISION_SOUND_Z_SCALE;
            soundOrigin[0] = player->currentOrigin[0] + soundOffset[0];
            soundOrigin[1] = player->currentOrigin[1] + soundOffset[1];
            soundOrigin[2] = player->currentOrigin[2] + soundOffset[2];

            soundEnt = G_PlaySoundAliasAtPoint(soundOrigin, vehicleInfo->hitPersonSound);
            soundEnt->svFlags |= SVF_LOOPED_FX;
        }

        /* Update collision damage timestamp */
        client->lastCollisionDamageTime = level.time;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: VERIFY-VEHICLE-PATH-TAIL-2026-06-17 reviewed helper extracted from VEH_VerifyPosition; no standalone original body. */
static long double game_compat_veh_dot3(const vec3_t a, const vec3_t b)
{
    /* long double return: the original keeps these dot products on the
     * 80-bit x87 stack (never rounded to float) through every consuming
     * compare and multiply; mirrors the engine DotProduct idiom. */
    return ((long double)a[0] * (long double)b[0]) + ((long double)a[1] * (long double)b[1]) + ((long double)a[2] * (long double)b[2]);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_veh_copy3(vec3_t out, const vec3_t in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

/* ------------------------------------------------------------------ */
/*  0x827ab  VEH_VerifyPosition                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x827ab, 927ab_FUN_000927ab.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_VerifyPosition(gentity_t *ent, int passNumber)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const float dismountSideOffset = game_compat_veh_dismount_side_offset(vehicleInfo);
    const vec3_t traceMins = {VEH_SOLID_TRACE_MINS_XY, VEH_SOLID_TRACE_MINS_XY, VEH_SOLID_TRACE_MIN_Z};
    const vec3_t traceMaxs = {VEH_SOLID_TRACE_MAX_XY, VEH_SOLID_TRACE_MAX_XY, VEH_SOLID_TRACE_MAX_Z};
    trace_t trace;
    trace_t bestTrace;
    vec3_t initialMins;
    vec3_t initialMaxs;
    vec3_t initialOrigin;
    axis_t axis;
    float bestSweepFraction = 1.0f;
    float closestSweepFraction = 1.0f;
    float collisionInputScale = 0.0f;
    int bestSweep = 0;
    qboolean hitFound = qfalse;
    qboolean movingAwayFromCachedEnt = qfalse;
    qboolean reducedByVehicleImpulse = qfalse;

    if (ent->contents == 0) {
        return qtrue;
    }

    if (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] == ENTITYNUM_NONE &&
        vehicleState->viewClampTargetAngles[0] == vehicleState->previousPhysicsState.viewClampTargetAngles[0] &&
        vehicleState->viewClampTargetAngles[1] == vehicleState->previousPhysicsState.viewClampTargetAngles[1] &&
        vehicleState->viewClampTargetAngles[2] == vehicleState->previousPhysicsState.viewClampTargetAngles[2] &&
        vehicleState->origin[0] == vehicleState->previousPhysicsState.origin[0] &&
        vehicleState->origin[1] == vehicleState->previousPhysicsState.origin[1] &&
        vehicleState->origin[2] == vehicleState->previousPhysicsState.origin[2]) {
        return qtrue;
    }

    initialMins[2] = 0.0f;
    initialMaxs[2] = ent->maxs[2] + VEH_SOLID_INITIAL_Z_PAD;

    float xyExtent = 0.0f;
    for (int axisIndex = 0; axisIndex < 2; axisIndex++) {
        if (xyExtent < ent->maxs[axisIndex]) {
            xyExtent = ent->maxs[axisIndex];
        }
    }
    initialMins[0] = -(xyExtent + VEH_SOLID_INITIAL_PAD);
    initialMins[1] = -(xyExtent + VEH_SOLID_INITIAL_PAD);
    initialMaxs[0] = xyExtent + VEH_SOLID_INITIAL_PAD;
    initialMaxs[1] = xyExtent + VEH_SOLID_INITIAL_PAD;

    initialOrigin[0] = ent->currentOrigin[0];
    initialOrigin[1] = ent->currentOrigin[1];
    initialOrigin[2] = ent->currentOrigin[2] + VEH_SOLID_STANDOFF;
    trap_Trace(&trace, initialOrigin, initialMins, initialMaxs, initialOrigin, ent->s.number, ent->clipmask | VEH_SOLID_INITIAL_CONTENTS);
    if (trace.fraction == 1.0f && trace.startsolid == 0) {
        return qtrue;
    }

    memset(&bestTrace, 0, sizeof(bestTrace));
    AnglesToAxis(vehicleState->viewClampTargetAngles, axis);

    for (int sweep = 0; sweep < VEH_SOLID_SWEEP_COUNT; sweep++) {
        vec3_t offset;
        vec3_t sweepDelta;
        vec3_t start;
        float sweepFraction = 0.0f;
        int bump = 0;
        uint32_t ignoredContents = 0;

        /* 0x82b92..0x82dd5: the original accumulates sweepDelta with one
         * float store per term -- the (stepSize + 2.0f) axis term first
         * (before the offsets), then one += per axis row -- and the
         * stepSize term is NOT grouped with offset[2]. */
        sweepDelta[0] = 0.0f;
        sweepDelta[1] = 0.0f;
        sweepDelta[2] = 0.0f;
        sweepDelta[0] += (vehicleInfo->stepSize + 2.0f) * axis[2][0];
        sweepDelta[1] += (vehicleInfo->stepSize + 2.0f) * axis[2][1];
        sweepDelta[2] += (vehicleInfo->stepSize + 2.0f) * axis[2][2];

        offset[0] = (dismountSideOffset - VEH_SOLID_STANDOFF) * vehCheckSolidOffsets[sweep][0];
        offset[1] = (dismountSideOffset - VEH_SOLID_STANDOFF) * vehCheckSolidOffsets[sweep][1];
        offset[2] = (dismountSideOffset - VEH_SOLID_STANDOFF) * vehCheckSolidOffsets[sweep][2];

        if (vehCheckSolidOffsets[sweep][0] > 0.0f) {
            offset[0] = vehicleInfo->dismountForwardOffset;
        } else if (vehCheckSolidOffsets[sweep][0] < 0.0f) {
            offset[0] = -vehicleInfo->dismountBackOffset;
        }

        sweepDelta[0] += axis[0][0] * offset[0];
        sweepDelta[1] += axis[0][1] * offset[0];
        sweepDelta[2] += axis[0][2] * offset[0];
        sweepDelta[0] += axis[1][0] * offset[1];
        sweepDelta[1] += axis[1][1] * offset[1];
        sweepDelta[2] += axis[1][2] * offset[1];
        sweepDelta[0] += axis[2][0] * offset[2];
        sweepDelta[1] += axis[2][1] * offset[2];
        sweepDelta[2] += axis[2][2] * offset[2];

        start[0] = vehicleState->origin[0];
        start[1] = vehicleState->origin[1];
        start[2] = vehicleState->origin[2] + vehicleInfo->suspensionTravel;

        do {
            vec3_t end;
            int traceMask;

            if (sweepFraction >= 1.0f || bump >= VEH_SOLID_MAX_SLIDE_BUMPS) {
                break;
            }
            bump++;

            end[0] = start[0] + (1.0f - sweepFraction) * sweepDelta[0];
            end[1] = start[1] + (1.0f - sweepFraction) * sweepDelta[1];
            end[2] = start[2] + (1.0f - sweepFraction) * sweepDelta[2];
            traceMask = (int)((ent->clipmask | VEH_SOLID_BODY_CONTENTS) & ~ignoredContents) | (int)VEH_SOLID_TRACE_CONTENTS;
            trap_Trace(&trace, start, traceMins, traceMaxs, end, ent->s.number, traceMask);

            if (trace.fraction < 1.0f && trace.entityNum < (uint16_t)level.maxclients) {
                VEH_PlayerCollision(ent, &g_entities[trace.entityNum]);
                ignoredContents = VEH_SOLID_IGNORED_BODY_MASK;
                continue;
            }

            if (trace.fraction < 1.0f && trace.entityNum != ENTITYNUM_WORLD &&
                game_compat_veh_entity_model_classname(&g_entities[trace.entityNum]) == scr_const_script_model) {
                vec3_t inputDir;

                VectorNormalize2(vehicleState->velocity, inputDir);
                inputDir[0] = -inputDir[0];
                inputDir[1] = -inputDir[1];
                inputDir[2] = -inputDir[2];
                VEH_UpdateScriptedInput(ent, inputDir, 1.0f, 0.0f, VEH_SOLID_SCRIPT_PUSH_SPEED);
                vehicleState->scriptedInputEndTime = level.time + VEH_SOLID_SCRIPT_PUSH_TIME_MS;
                game_compat_veh_clear_vehicle_velocity(vehicleState);
                Scr_AddEntity(ent);
                Scr_Notify(&g_entities[trace.entityNum], scr_const_trigger, 1);
                return qtrue;
            }

            if (trace.entityNum == ENTITYNUM_WORLD && trace.fraction < 1.0f && vehicleState->velocity[2] > VEH_SOLID_MIN_FALL_SPEED &&
                trace.normal[2] >= VEH_SOLID_GROUND_MIN_Z) {
                sweepFraction += (1.0f - sweepFraction) * trace.fraction;
                start[0] = trace.endpos[0] + trace.normal[0];
                start[1] = trace.endpos[1] + trace.normal[1];
                start[2] = trace.endpos[2] + trace.normal[2];
                VEH_ClipVelocity(sweepDelta, trace.normal, sweepDelta);
                continue;
            }

            if (trace.fraction == 1.0f) {
                break;
            }

            if ((uint32_t)trace.entityNum == (uint32_t)vehicleState->cachedCollisionEntityNum) {
                gentity_t *cachedEnt = &g_entities[trace.entityNum];
                vec3_t cachedDelta = {cachedEnt->currentOrigin[0] - vehicleState->origin[0],
                                      cachedEnt->currentOrigin[1] - vehicleState->origin[1],
                                      cachedEnt->currentOrigin[2] - vehicleState->origin[2]};

                if (game_compat_veh_dot3(cachedDelta, vehicleState->velocity) <= 0.0f) {
                    movingAwayFromCachedEnt = qtrue;
                    break;
                }
            }

            if (sweepFraction < closestSweepFraction) {
                closestSweepFraction = sweepFraction;
            }
            if (sweepFraction < bestSweepFraction) {
                bestSweepFraction = sweepFraction;
                bestTrace = trace;
                bestSweep = sweep;
                hitFound = qtrue;
            }
        } while (trace.fraction != 0.0f);
    }

    if (hitFound) {
        gentity_t *hitEnt = NULL;
        vehicle_state_t *hitState = NULL;
        const vehicleInfo_t *hitInfo = NULL;
        vec3_t velocityDir = {0.0f, 0.0f, 0.0f};
        float impactSpeedScale;
        float impactFromNormal = 0.0f;
        vec3_t hitDirection;
        vec3_t cornerDir;
        float spinScale = 0.0f;

        trace = bestTrace;
        vehicleState->collisionSweepFraction = bestSweepFraction;
        if (trace.entityNum != ENTITYNUM_WORLD) {
            vehicleState->cachedCollisionEntityNum = trace.entityNum;
            vehicleState->cachedCollisionDistance = VectorDistance(vehicleState->origin, g_entities[trace.entityNum].currentOrigin);
        }

        memcpy(&vehicleState->origin, &vehicleState->previousPhysicsState, sizeof(vehicleState->previousPhysicsState));
        vehicleState->lastSolidTime = level.time;
        vehicleState->collisionNormal[0] = trace.normal[0];
        vehicleState->collisionNormal[1] = trace.normal[1];
        vehicleState->collisionNormal[2] = trace.normal[2];

        collisionInputScale = fabsf(vehicleState->acceleration[1]) / 100.0f;
        /* 0x834d6: sqrt takes the dot rounded to double, result to float. */
        impactSpeedScale = game_compat_veh_length3(vehicleState->velocity);
        if (game_compat_veh_float_is_non_zero_or_nan(impactSpeedScale)) {
            /* 0x83582..0x83598: maxSpeed * 0.75 stays in the 80-bit divisor
             * chain; only the quotient is rounded. */
            float speedFraction = impactSpeedScale / (vehicleInfo->maxSpeed * VEH_SOLID_SPEED_SCALE_DENOM);

            speedFraction = game_compat_veh_clamp_float(speedFraction, 0.0f, 1.0f);
            VectorNormalize2(vehicleState->velocity, velocityDir);
            /* 0x8361a..0x83697: the dot is never rounded on its own -- it
             * feeds the compare and the negate-multiply in registers, and
             * the sign test is on the dot itself (dot >= 0 yields the
             * speedFraction * 0.0f product). */
            if (game_compat_veh_dot3(velocityDir, trace.normal) < 0.0f) {
                impactFromNormal = -game_compat_veh_dot3(velocityDir, trace.normal) * speedFraction;
            } else {
                impactFromNormal = speedFraction * 0.0f;
            }
            /* 0x836a9..0x836e4: min against 0.5f; the sum is compared in
             * registers and rounded only on the store. */
            if (collisionInputScale + impactFromNormal < 0.5f) {
                collisionInputScale = collisionInputScale + impactFromNormal;
            } else {
                collisionInputScale = 0.5f;
            }
        }

        if (vehicleState->throttleScale <= 0.0f) {
            float inputScale =
                game_compat_veh_clamp_float(collisionInputScale * 4.0f, VEH_SOLID_MIN_SCRIPT_SCALE, VEH_SOLID_MAX_SCRIPT_SCALE);
            /* 0x83728..0x8375f: fabsl keeps the dot in the 80-bit chain
             * (fabs on the register, no float store before the multiply). */
            float inputSpeed = VEH_SOLID_INPUT_SPEED_BASE - fabsl(game_compat_veh_dot3(velocityDir, axis[0])) * VEH_SOLID_INPUT_SPEED_AXIS;

            VEH_UpdateScriptedInput(ent, trace.normal, inputScale, 0.0f, inputSpeed);
        }

        if (trace.entityNum != ENTITYNUM_NONE) {
            hitEnt = &g_entities[trace.entityNum];
            hitState = (vehicle_state_t *)hitEnt->vehicle;
            if (hitState != NULL) {
                hitInfo = game_compat_veh_get_vehicle_info(hitState->typeIndex);
            }
        }

        if (hitEnt != NULL && game_compat_veh_entity_model_classname(hitEnt) == scr_const_script_vehicle) {
            hitDirection[0] = hitEnt->currentOrigin[0] - vehicleState->origin[0];
            hitDirection[1] = hitEnt->currentOrigin[1] - vehicleState->origin[1];
            hitDirection[2] = hitEnt->currentOrigin[2] - vehicleState->origin[2];

            if (hitState->throttleScale == 0.0f) {
                float otherInputScale =
                    game_compat_veh_clamp_float(collisionInputScale * 2.0f, VEH_SOLID_MIN_SCRIPT_SCALE, VEH_SOLID_MAX_OTHER_SCALE);

                VectorNormalize(hitDirection);
                VEH_UpdateScriptedInput(hitEnt, hitDirection, otherInputScale, 0.0f, VEH_SOLID_SCRIPT_PUSH_SPEED);
            } else {
                VectorNormalize(hitDirection);
            }

            /* 0x839af..0x84287: inline impulse push. Only the maxSpeed
             * ratio, its *IMPULSE_BASE product, and the sqrt results are
             * rounded to float slots; the vehicle speed ratio and the
             * scaled goal stay in 80-bit registers through every compare
             * and per-component velocity update. */
            {
                float goalBase = hitInfo->maxSpeed / vehicleInfo->maxSpeed;
                float hitSpeed;
                float vehicleSpeed;
                qboolean applyImpulse;

                goalBase = VEH_SOLID_IMPULSE_BASE * goalBase;
                hitSpeed = game_compat_veh_length3(hitState->velocity);
                vehicleSpeed = game_compat_veh_length3(vehicleState->velocity);

                if (vehicleSpeed / vehicleInfo->maxSpeed < VEH_SOLID_IMPULSE_MIN_SPEED) {
                    applyImpulse = VEH_SOLID_IMPULSE_MIN_SPEED * goalBase > hitSpeed;
                } else if (vehicleSpeed / vehicleInfo->maxSpeed > 1.0f) {
                    applyImpulse = goalBase > hitSpeed;
                } else {
                    applyImpulse = vehicleSpeed / vehicleInfo->maxSpeed * goalBase > hitSpeed;
                }

                if (applyImpulse) {
                    hitDirection[0] = hitEnt->currentOrigin[0] - vehicleState->origin[0];
                    hitDirection[1] = hitEnt->currentOrigin[1] - vehicleState->origin[1];
                    hitDirection[2] = hitEnt->currentOrigin[2] - vehicleState->origin[2];
                    VectorNormalize(hitDirection);

                    for (int component = 0; component < 3; component++) {
                        if (vehicleSpeed / vehicleInfo->maxSpeed < VEH_SOLID_IMPULSE_MIN_SPEED) {
                            hitState->velocity[component] =
                                VEH_SOLID_IMPULSE_MIN_SPEED * goalBase * hitDirection[component] + hitState->velocity[component];
                        } else if (vehicleSpeed / vehicleInfo->maxSpeed > 1.0f) {
                            hitState->velocity[component] = hitDirection[component] * goalBase + hitState->velocity[component];
                        } else {
                            hitState->velocity[component] =
                                vehicleSpeed / vehicleInfo->maxSpeed * goalBase * hitDirection[component] + hitState->velocity[component];
                        }
                    }

                    /* 0x8419b..0x84284: clamp to the hit vehicle's max. */
                    hitSpeed = game_compat_veh_length3(hitState->velocity);
                    if (hitSpeed > hitInfo->maxSpeed) {
                        VectorNormalize(hitState->velocity);
                        hitState->velocity[0] = hitState->velocity[0] * hitInfo->maxSpeed;
                        hitState->velocity[1] = hitState->velocity[1] * hitInfo->maxSpeed;
                        hitState->velocity[2] = hitState->velocity[2] * hitInfo->maxSpeed;
                    }
                }
            }

            if (game_compat_veh_dot3(hitDirection, vehicleState->velocity) >= 0.0f) {
                if (hitInfo->maxSpeed <= vehicleInfo->maxSpeed) {
                    vec3_t velocityDelta = {hitState->velocity[0] - vehicleState->velocity[0],
                                            hitState->velocity[1] - vehicleState->velocity[1],
                                            hitState->velocity[2] - vehicleState->velocity[2]};
                    /* 0x84483..0x848c4: the clamped reduction is never
                     * stored -- each component's chain recomputes it in
                     * 80-bit registers (maxSpeed / 2.0f, negated, times the
                     * direction) and rounds once on the velocity store. */
                    const float deltaSpeed = game_compat_veh_length3(velocityDelta);

                    if (game_compat_veh_dot3(vehicleState->velocity, hitDirection) > 0.0f) {
                        for (int component = 0; component < 3; component++) {
                            if (deltaSpeed < 0.0f) {
                                vehicleState->velocity[component] = -0.0f * hitDirection[component] + vehicleState->velocity[component];
                            } else if (deltaSpeed > vehicleInfo->maxSpeed / 2.0f) {
                                vehicleState->velocity[component] =
                                    -(vehicleInfo->maxSpeed / 2.0f) * hitDirection[component] + vehicleState->velocity[component];
                            } else {
                                vehicleState->velocity[component] =
                                    -deltaSpeed * hitDirection[component] + vehicleState->velocity[component];
                            }
                        }
                        reducedByVehicleImpulse = qtrue;
                    }
                } else {
                    game_compat_veh_clear_vehicle_velocity(vehicleState);
                }
            }
        }

        game_compat_veh_copy3(cornerDir, vehCheckSolidOffsets[bestSweep]);
        hitDirection[0] = trace.endpos[0] - vehicleState->origin[0];
        hitDirection[1] = trace.endpos[1] - vehicleState->origin[1];
        hitDirection[2] = trace.endpos[2] - vehicleState->origin[2];
        VectorNormalize(hitDirection);

        if (game_compat_veh_dot3(hitDirection, axis[1]) > 0.1f) {
            if (game_compat_veh_dot3(hitDirection, axis[0]) < -0.1f) {
                spinScale = 1.0f;
            } else if (game_compat_veh_dot3(hitDirection, axis[0]) > 0.1f) {
                spinScale = -1.0f;
            }
        } else if (game_compat_veh_dot3(hitDirection, axis[1]) < -0.1f) {
            if (game_compat_veh_dot3(hitDirection, axis[0]) < -0.1f) {
                spinScale = -1.0f;
            } else if (game_compat_veh_dot3(hitDirection, axis[0]) > 0.1f) {
                spinScale = 1.0f;
            }
        }

        if (game_compat_veh_vector3_is_non_zero(vehicleState->velocity)) {
            VectorNormalize2(vehicleState->velocity, cornerDir);
        }
        /* 0x84b8c..0x84dc3: steeringRate/70 and the spin factor are stored
         * to double slots, the magnitude term stays in 80-bit registers
         * through the double-constant clamp compares, and the product is
         * rounded once to double, then once to float. */
        {
            const double steeringScale = vehicleInfo->steeringRate / 70.0f;
            const double spinFactor = spinScale;
            const float spinSpeed = game_compat_veh_length3(vehicleState->velocity);
            const double accelMagnitude = fabsf(vehicleState->acceleration[1]);
            double spinProduct;

            if (spinSpeed * 0.5f + accelMagnitude < 0.0) {
                spinProduct = 0.0 * steeringScale * spinFactor;
            } else if (spinSpeed * 0.5f + accelMagnitude > 80.0) {
                spinProduct = 80.0 * steeringScale * spinFactor;
            } else {
                spinProduct = (spinSpeed * 0.5f + accelMagnitude) * steeringScale * spinFactor;
            }
            spinScale = spinProduct;
        }
        if (game_compat_veh_float_is_non_zero_or_nan(spinScale)) {
            /* 0x84de2..0x84e2d: the corner dot, both fabs, and the doubling
             * stay on the x87 stack; one rounding at the store. */
            vehicleState->acceleration[1] = (1.0f - fabsl(fabsl(game_compat_veh_dot3(cornerDir, trace.normal)) - 0.5) * 2.0f) * spinScale;
        }

        if (!reducedByVehicleImpulse && (hitEnt == NULL || hitEnt->client == NULL) &&
            (hitEnt == NULL || hitInfo == NULL || hitInfo->maxSpeed < vehicleInfo->maxSpeed)) {
            vec3_t bounceDir;
            vec3_t horizontalVelocity = {vehicleState->velocity[0], vehicleState->velocity[1], 0.0f};
            /* 0x84ec0..0x84f51: sqrt of the double-rounded dot; the floor
             * test is !(len > 10) so an unordered compare also floors. */
            float bounceSpeed = game_compat_veh_length3(horizontalVelocity);

            if (!(bounceSpeed > VEH_SOLID_DEFLECT_MIN_SPEED)) {
                bounceSpeed = VEH_SOLID_DEFLECT_MIN_SPEED;
            }

            bounceDir[0] = trace.normal[0];
            bounceDir[1] = trace.normal[1];
            bounceDir[2] = trace.normal[2] * 0.5f;
            if (!game_compat_veh_vector3_is_non_zero(bounceDir)) {
                bounceDir[0] = -hitDirection[0];
                bounceDir[1] = -hitDirection[1];
                bounceDir[2] = -hitDirection[2];
            }

            if (game_compat_veh_dot3(bounceDir, vehicleState->velocity) < 0.0f) {
                VEH_ClipVelocity(vehicleState->velocity, bounceDir, vehicleState->velocity);
                /* 0x8508c..0x8528a: the push term is never stored -- each
                 * component's chain keeps it in 80-bit registers and rounds
                 * once on the velocity store. */
                for (int component = 0; component < 3; component++) {
                    if (bounceSpeed / vehicleInfo->maxSpeed * VEH_SOLID_BOUNCE_SPEED * VEH_SOLID_BOUNCE_SCALE > VEH_SOLID_BOUNCE_MIN_PUSH) {
                        vehicleState->velocity[component] =
                            bounceSpeed / vehicleInfo->maxSpeed * VEH_SOLID_BOUNCE_SPEED * VEH_SOLID_BOUNCE_SCALE * bounceDir[component] +
                            vehicleState->velocity[component];
                    } else {
                        vehicleState->velocity[component] =
                            VEH_SOLID_BOUNCE_MIN_PUSH * bounceDir[component] + vehicleState->velocity[component];
                    }
                }
            } else {
                const float clampedSpeed = game_compat_veh_clamp_float(bounceSpeed, VEH_SOLID_BOUNCE_MIN_SPEED, VEH_SOLID_BOUNCE_MAX_SPEED);

                vehicleState->velocity[0] = bounceDir[0] * clampedSpeed;
                vehicleState->velocity[1] = bounceDir[1] * clampedSpeed;
                vehicleState->velocity[2] = bounceDir[2] * clampedSpeed;
            }
        }

        vehicleState->scriptedInputEndTime = level.time + VEH_SOLID_SCRIPT_PUSH_TIME_MS;

        /* 0x85462..0x856a2: the maxSpeed * doorAltSpeed product is never
         * rounded to a float slot -- it stays in the 80-bit chain both in
         * the over-speed compare and in each component scale. */
        {
            const float solidSpeed = game_compat_veh_length3(vehicleState->velocity);
            qboolean overMaxSpeed;

            if (game_compat_veh_float_is_non_zero_or_nan(ent->doorAltSpeed)) {
                overMaxSpeed = solidSpeed > vehicleInfo->maxSpeed * ent->doorAltSpeed;
            } else {
                overMaxSpeed = solidSpeed > vehicleInfo->maxSpeed;
            }
            if (overMaxSpeed) {
                VectorNormalize(vehicleState->velocity);
                if (game_compat_veh_float_is_non_zero_or_nan(ent->doorAltSpeed)) {
                    vehicleState->velocity[0] = vehicleInfo->maxSpeed * ent->doorAltSpeed * vehicleState->velocity[0];
                    vehicleState->velocity[1] = vehicleInfo->maxSpeed * ent->doorAltSpeed * vehicleState->velocity[1];
                    vehicleState->velocity[2] = vehicleInfo->maxSpeed * ent->doorAltSpeed * vehicleState->velocity[2];
                } else {
                    vehicleState->velocity[0] *= vehicleInfo->maxSpeed;
                    vehicleState->velocity[1] *= vehicleInfo->maxSpeed;
                    vehicleState->velocity[2] *= vehicleInfo->maxSpeed;
                }
            }
        }

        if (g_vehicleEnableCollisionDamage.integer != 0 && impactFromNormal > 0.5f) {
            static const float half = 0.5f;
            /* 0x856ea..0x85721: fistp consumes the 80-bit chain directly;
             * no float rounding before the int truncation. */
#if EMULATE_X87
            const x87f halfOffset = x87f_sub(x87f_load_f32(collisionInputScale), x87f_load_f32(half));
            const int damage =
                x87f_store_i32_trunc(x87f_mul(x87f_add(halfOffset, halfOffset), x87f_load_f32(vehicleInfo->collisionDamageScale)));
#elif defined(__x86_64__)
            register long double halfOffset = (long double)collisionInputScale - (long double)half;
            const int damage = CODUO_X87_TRUNCATE_I32((halfOffset + halfOffset) * (long double)vehicleInfo->collisionDamageScale);
#else
            const int damage = (int)(((collisionInputScale - half) + (collisionInputScale - half)) * vehicleInfo->collisionDamageScale);
#endif

            G_Damage(ent, ent, ent, trace.normal, trace.endpos, damage, 0, MOD_COLLISION, 2);
        }

        if (vehicleState->lastStableTime < level.time - VEH_SOLID_STUCK_DAMAGE_DELAY_MS) {
            G_Damage(ent, ent, ent, trace.normal, trace.endpos, VEH_SOLID_STUCK_DAMAGE, 0, MOD_COLLISION, 2);
            vehicleState->lastStableTime = level.time + VEH_SOLID_STUCK_DAMAGE_GRACE_MS;
        }

        if (vehicleState->lastStableTime > level.time - VEH_SOLID_NOTIFY_RECENT_MS &&
            vehicleState->collisionNotifyTime < level.time - VEH_SOLID_NOTIFY_COOLDOWN_MS && collisionInputScale > 0.25f &&
            game_compat_veh_float_is_non_zero_or_nan(game_compat_veh_length3(trace.normal))) {
            vec3_t notifyPoint = {trace.endpos[0] - VEH_SOLID_STANDOFF * trace.normal[0],
                                  trace.endpos[1] - VEH_SOLID_STANDOFF * trace.normal[1],
                                  trace.endpos[2] - VEH_SOLID_STANDOFF * trace.normal[2]};

            Scr_AddVector(trace.normal);
            Scr_AddVector(notifyPoint);
            Scr_Notify(ent, scr_const_vehicle_collision, 2);
            vehicleState->collisionNotifyTime = level.time;
        }

        if (vehicleState->lastStableTime > level.time - VEH_SOLID_NOTIFY_RECENT_MS &&
            vehicleState->collisionSoundTime < level.time - VEH_SOLID_NOTIFY_COOLDOWN_MS && vehicleInfo->collisionSound != 0) {
            /* 0x8599d..0x85a09: the offset vector is built in its own float
             * slots (so the z term rounds once there) and then added to the
             * endpos per component. */
            vec3_t soundOffset;
            vec3_t soundOrigin;

            soundOffset[0] = 0.0f;
            soundOffset[1] = 0.0f;
            soundOffset[2] = 0.0f;
            soundOffset[2] += (1.0f - collisionInputScale) * VEH_SOLID_SOUND_Z_SCALE;
            soundOrigin[0] = trace.endpos[0] + soundOffset[0];
            soundOrigin[1] = trace.endpos[1] + soundOffset[1];
            soundOrigin[2] = trace.endpos[2] + soundOffset[2];
            gentity_t *soundEnt = G_PlaySoundAliasAtPoint(soundOrigin, vehicleInfo->collisionSound);

            soundEnt->svFlags |= SVF_LOOPED_FX;
            vehicleState->collisionSoundTime = level.time;
        }

        if (passNumber > 1) {
            vec3_t away = {vehicleState->origin[0] - trace.endpos[0], vehicleState->origin[1] - trace.endpos[1],
                           vehicleState->origin[2] - trace.endpos[2]};

            VectorNormalize(away);
            vehicleState->velocity[0] = away[0] * (vehicleInfo->maxSpeed / VEH_SOLID_SECOND_PASS_SPEED_DIVISOR);
            vehicleState->velocity[1] = away[1] * (vehicleInfo->maxSpeed / VEH_SOLID_SECOND_PASS_SPEED_DIVISOR);
            vehicleState->velocity[2] = away[2] * (vehicleInfo->maxSpeed / VEH_SOLID_SECOND_PASS_SPEED_DIVISOR);
        }

        return qfalse;
    }

    vehicleState->collisionSweepFraction = closestSweepFraction;
    if (!movingAwayFromCachedEnt && vehicleState->cachedCollisionEntityNum != ENTITYNUM_NONE) {
        vehicleState->cachedCollisionEntityNum = ENTITYNUM_NONE;
        vehicleState->cachedCollisionDistance = VEH_COLLISION_CACHE_DISTANCE_RESET;
    }

    return qtrue;
}

/* ------------------------------------------------------------------ */
/*  0x85b32  VEH_UpdateAim                                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x85b32, 95b32_FUN_00095b32.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_UpdateAim(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    gentity_t *passenger = NULL;
    vec3_t vehicleAngles;
    vec3_t passengerAngles;
    vec3_t localAimAngles;
    vec3_t currentAngles;
    vec3_t angleDelta;
    float desiredPitch;
    float desiredYaw;
    float clampedPitch;
    float clampedYaw;
    DObjSkelMat boneMatrix;

    if (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] != ENTITYNUM_NONE) {
        passenger = &g_entities[vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER]];
    }

    if (vehicleInfo->primaryTurnRate == 0.0f && vehicleInfo->primaryActiveSound != 0) {
        if (g_vehicleHorns.integer != 0) {
            if (passenger == NULL || (passenger->client->currentButtons & VEH_CLIENT_FIRE_BUTTON) == 0 ||
                vehicleState->altOverheating != 0) {
                game_compat_veh_advance_turret_state(vehicleState, VEH_TURRET_ACTIVITY_PRIMARY);
            } else {
                game_compat_veh_set_turret_activity_state(vehicleState, VEH_TURRET_ACTIVITY_PRIMARY, VEH_TURRET_STATE_ACTIVE);
                vehicleState->altHeat += 0.04f;
            }
        }
        return;
    }

    if (ent->vehiclePrimaryDisabled != 0 || vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_DRIVER] == ENTITYNUM_NONE ||
        ent->health <= 0 || vehicleInfo->primaryTurnRate == 0.0f) {
        game_compat_veh_advance_turret_state(vehicleState, VEH_TURRET_ACTIVITY_PRIMARY);
        return;
    }

    if (vehicleState->primaryTurretTagIndex < 0 || passenger == NULL) {
        return;
    }

    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->primaryTurretTagIndex, &boneMatrix);

    game_compat_veh_build_vehicle_local_angles(vehicleState, vehicleAngles);
    game_compat_veh_build_passenger_aim_angles(passenger, passengerAngles);
    game_compat_veh_aim_angles_in_vehicle_space(passengerAngles, vehicleAngles, localAimAngles);

    currentAngles[0] = ent->s.vehicleTurret.primaryPitch;
    currentAngles[1] = game_compat_veh_entity_primary_yaw(ent);
    currentAngles[2] = 0.0f;
    AnglesSubtract(localAimAngles, currentAngles, angleDelta);
    angleDelta[0] = fabsf(angleDelta[0]);
    angleDelta[1] = fabsf(angleDelta[1]);

    desiredPitch = VEH_TrackAngle(localAimAngles[0], currentAngles[0], vehicleInfo->primaryTurnRate);
    desiredYaw = VEH_TrackAngle(localAimAngles[1], currentAngles[1], vehicleInfo->primaryTurnRate);

    clampedPitch =
        game_compat_veh_clamp_float_symmetric(desiredPitch, vehicleInfo->primaryPitchLimitNeg, vehicleInfo->primaryPitchLimitPos);
    if (game_compat_veh_float_is_non_zero_or_nan(ent->vehiclePrimaryYawClamp)) {
        clampedYaw = game_compat_veh_clamp_abs(desiredYaw, fabsf(ent->vehiclePrimaryYawClamp));
    } else {
        clampedYaw = game_compat_veh_clamp_float_symmetric(desiredYaw, vehicleInfo->primaryYawLimitNeg, vehicleInfo->primaryYawLimitPos);
    }

    ent->s.vehicleTurret.primaryPitch = clampedPitch;
    game_compat_veh_set_entity_primary_yaw(ent, clampedYaw);
    game_compat_veh_update_turret_activity(vehicleState, VEH_TURRET_ACTIVITY_PRIMARY, angleDelta[0], angleDelta[1], desiredPitch,
                                           clampedPitch, desiredYaw);

    if (angleDelta[0] < VEH_TURRET_NOTIFY_EPSILON && angleDelta[1] < VEH_TURRET_NOTIFY_EPSILON) {
        Scr_Notify(ent, scr_const_turret_on_target, 0);
    }
}

/* ------------------------------------------------------------------ */
/*  0x86285  VEH_UpdateGunnerAim                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x86285, 96285_FUN_00096285.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_UpdateGunnerAim(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    gentity_t *gunner;
    vec3_t vehicleAngles;
    vec3_t passengerAngles;
    vec3_t localAimAngles;
    vec3_t currentAngles;
    vec3_t angleDelta;
    float desiredPitch;
    float desiredYaw;
    float clampedPitch;
    float clampedYaw;
    DObjSkelMat boneMatrix;

    if (vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_GUNNER] == ENTITYNUM_NONE || ent->health < 1) {
        game_compat_veh_advance_turret_state(vehicleState, VEH_TURRET_ACTIVITY_GUNNER);
        return;
    }

    if (vehicleState->gunnerTurretTagIndex < 0) {
        return;
    }

    gunner = &g_entities[vehicleState->passengerEntityNums[VEH_PASSENGER_SLOT_GUNNER]];
    G_DObjGetWorldBoneIndexMatrix(ent, vehicleState->gunnerTurretTagIndex, &boneMatrix);

    game_compat_veh_build_vehicle_local_angles(vehicleState, vehicleAngles);
    game_compat_veh_build_passenger_aim_angles(gunner, passengerAngles);
    game_compat_veh_aim_angles_in_vehicle_space(passengerAngles, vehicleAngles, localAimAngles);

    currentAngles[0] = ent->s.vehicleTurret.gunnerPitch;
    currentAngles[1] = game_compat_veh_entity_gunner_yaw_base(ent) + game_compat_veh_entity_primary_yaw(ent);
    currentAngles[2] = 0.0f;
    AnglesSubtract(localAimAngles, currentAngles, angleDelta);
    angleDelta[0] = fabsf(angleDelta[0]);
    angleDelta[1] = fabsf(angleDelta[1]);

    desiredPitch = VEH_TrackAngle(localAimAngles[0], currentAngles[0], vehicleInfo->gunnerTurnRate);
    desiredYaw = VEH_TrackAngle(localAimAngles[1], currentAngles[1], vehicleInfo->gunnerTurnRate);

    clampedPitch = game_compat_veh_clamp_float_symmetric(desiredPitch, vehicleInfo->gunnerPitchLimitNeg, vehicleInfo->gunnerPitchLimitPos);
    clampedYaw = game_compat_veh_clamp_float_symmetric(desiredYaw, vehicleInfo->gunnerYawLimitNeg, vehicleInfo->gunnerYawLimitPos);

    ent->s.vehicleTurret.gunnerPitch = clampedPitch;
    game_compat_veh_set_entity_gunner_yaw_base(ent, clampedYaw - game_compat_veh_entity_primary_yaw(ent));
    game_compat_veh_update_turret_activity(vehicleState, VEH_TURRET_ACTIVITY_GUNNER, angleDelta[0], angleDelta[1], desiredPitch,
                                           clampedPitch, desiredYaw);
}

/* ------------------------------------------------------------------ */
/*  0x86735  VEH_UpdateBody                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x86735, 96735_FUN_00096735.c, VERIFY-VEHICLE-PHYSICS-MOVE-2026-06-17): DATAFLOW_VERIFIED */
void VEH_UpdateBody(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    float forward = 0.0f;
    float vertical = 0.0f;

    if (vehicleState->throttleScale > 0.0f) {
        /* 0x86790..0x867c9: the throttle ratio is rounded to its own float
         * slot; CoduoLibm_Sin() takes brakeScale * pi / 180 with DOUBLE pi
         * (0x400921fb54442d18) and double 180, rounded to double at the
         * call, and the sin result is rounded to float before the
         * multiply. */
        const float throttleRatio = vehicleState->throttleScale / vehicleState->throttleScalePrevious;
        const float phase =
            (float)CoduoLibm_Sin((double)((long double)vehicleState->brakeScale * 3.141592653589793L / 180.0L)) * throttleRatio;

        forward = phase * vehicleState->localAccel.components.forward;
        vertical = phase * vehicleState->localAccel.components.vertical;

        vehicleState->throttleScale -= VEH_MOTION_ANIM_FRAME_SECONDS;
        if (vehicleState->throttleScale < 0.0f) {
            vehicleState->throttleScale = 0.0f;
        }
        vehicleState->brakeScale += VEH_MOTION_ANIM_PHASE_STEP;
    }

    forward += vehicleState->externalVelocity[0];
    vertical += vehicleState->externalVelocity[2];

    game_compat_veh_set_float_slot_int_bits(&ent->s.loopedFxForward[0], game_compat_veh_motion_byte_from_axis(forward));
    game_compat_veh_set_float_slot_int_bits(&ent->s.loopedFxForward[2], game_compat_veh_motion_byte_from_axis(vertical));
}

/* ------------------------------------------------------------------ */
/*  0x95b90  VP_DebugPathSegment                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x95b90, a5b90_FUN_000a5b90.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_DebugPathSegment(const vec3_t start, const vec3_t end, qboolean flushSegment)
{
    static const vec4_t color = {1.0f, 0.0f, 0.0f, 1.0f};
    vec3_t direction = {end[0] - start[0], end[1] - start[1], end[2] - start[2]};

    VectorNormalize(direction);

    if (vehDebugPathSegmentNeedsInit) {
        memcpy(vehDebugPathSegmentStart, start, sizeof(vehDebugPathSegmentStart));
        memcpy(vehDebugPathSegmentEnd, end, sizeof(vehDebugPathSegmentEnd));
        memcpy(vehDebugPathSegmentDir, direction, sizeof(vehDebugPathSegmentDir));
        vehDebugPathSegmentNeedsInit = qfalse;
        return;
    }

    /* direction . vehDebugPathSegmentDir stays 80-bit into a full-width
     * fucompp (no float spill in the ref build).  The branch at
     * 0x95c8d flushes only for ordered dot < K, so unordered values coalesce. */
#if EMULATE_X87
    if (!x87f_lt(x87f_add(x87f_add(x87f_mul(x87f_load_f32(direction[0]), x87f_load_f32(vehDebugPathSegmentDir[0])),
                                   x87f_mul(x87f_load_f32(direction[1]), x87f_load_f32(vehDebugPathSegmentDir[1]))),
                          x87f_mul(x87f_load_f32(direction[2]), x87f_load_f32(vehDebugPathSegmentDir[2]))),
                 x87f_load_f32(VEH_PATH_DEBUG_SEGMENT_DOT)) &&
        !flushSegment) {
#else
    if (!(direction[0] * vehDebugPathSegmentDir[0] + direction[1] * vehDebugPathSegmentDir[1] + direction[2] * vehDebugPathSegmentDir[2] <
          VEH_PATH_DEBUG_SEGMENT_DOT) &&
        !flushSegment) {
#endif
        memcpy(vehDebugPathSegmentEnd, end, sizeof(vehDebugPathSegmentEnd));
        return;
    }

    G_DebugLine(vehDebugPathSegmentStart, vehDebugPathSegmentEnd, color, 1, 0);
    memcpy(vehDebugPathSegmentStart, start, sizeof(vehDebugPathSegmentStart));
    memcpy(vehDebugPathSegmentEnd, end, sizeof(vehDebugPathSegmentEnd));
    memcpy(vehDebugPathSegmentDir, direction, sizeof(vehDebugPathSegmentDir));
}

/* ------------------------------------------------------------------ */
/*  0x95d7e  VP_ParseDynamicVehicleNodeField                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x95d7e, a5d7e_FUN_000a5d7e.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_ParseDynamicVehicleNodeField(const char *key, const char *value, const vehicle_path_node_t *node)
{
    int fieldType;
    uint16_t fieldName = Scr_FindField(key, &fieldType);

    if (fieldName == 0) {
        return;
    }

    if (fieldType == VEH_SCRIPT_FIELD_FLOAT) {
        Scr_AddFloat((float)atof(value));
    } else if (fieldType < VEH_SCRIPT_FIELD_INT) {
        if (fieldType != VEH_SCRIPT_FIELD_STRING) {
            return;
        }
        Scr_AddString(value);
    } else {
        if (fieldType != VEH_SCRIPT_FIELD_INT) {
            return;
        }
        Scr_AddInt(atoi(value));
    }

    Scr_SetDynamicEntityField(node->scriptObjectId, SCRIPT_OBJECT_VEHICLE_NODE, fieldName);
}

/* ------------------------------------------------------------------ */
/*  0x95e32  VP_ParseVehicleNodeField                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x95e32, a5e32_FUN_000a5e32.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_ParseVehicleNodeField(const char *key, const char *value, vehicle_path_node_t *node)
{
    const vehicle_node_spawn_field_t *field;
    uint8_t *base;

    for (field = vehicleNodeSpawnFields; field->name != 0; field++) {
        if (Q_stricmp(field->name, key) == 0) {
            break;
        }
    }

    if (field->name == 0) {
        VP_ParseDynamicVehicleNodeField(key, value, node);
        return;
    }

    base = &((uint8_t *)(void *)node)[field->offset];
    switch (field->type) {
    case 0:
        *(int32_t *)(void *)base = atoi(value);
        break;
    case 1:
        *(int16_t *)(void *)base = (int16_t)atoi(value);
        break;
    case 2:
        *base = (uint8_t)atoi(value);
        break;
    case 3:
        *(float *)(void *)base = (float)atof(value);
        break;
    case 5:
        Scr_SetString((uint16_t *)(void *)base, 0);
        *(uint16_t *)(void *)base = G_NewString(value);
        break;
    case 6: {
        vec3_t vector;

        sscanf(value, "%f %f %f", &vector[0], &vector[1], &vector[2]);
        memcpy(base, vector, sizeof(vector));
        break;
    }
    case 9: {
        vec3_t vector;

        sscanf(value, "%f %f %f", &vector[0], &vector[1], &vector[2]);
        *(float *)(void *)base = AngleNormalize360Accurate(vector[1]);
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  0x96028  VP_ParseVehicleNodeSpawnVars                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96028, a6028_FUN_000a6028.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_ParseVehicleNodeSpawnVars(vehicle_path_node_t *node)
{
    const level_locals_t *lvl = &level;
    const char *const *spawnVarPairs = (const char *const *)lvl->spawnVarPairSlots;

    for (int index = 0; index < lvl->spawnVarCount; index++) {
        const char *const *pair = &spawnVarPairs[index * 2];

        VP_ParseVehicleNodeField(pair[0], pair[1], node);
    }
}

/* ------------------------------------------------------------------ */
/*  0x96094  VP_InitNode                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96094, a6094_FUN_000a6094.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_InitNode(vehicle_path_node_t *node, int16_t scriptObjectId)
{
    Scr_SetString(&node->targetname, 0);
    Scr_SetString(&node->target, 0);
    node->scriptObjectId = scriptObjectId;
    node->useNodeAngles = qfalse;
    node->speed = -1.0f;
    node->lookAhead = -1.0f;
    node->origin[0] = 0.0f;
    node->origin[1] = 0.0f;
    node->origin[2] = 0.0f;
    node->direction[0] = 0.0f;
    node->direction[1] = 0.0f;
    node->direction[2] = 0.0f;
    node->angles[0] = VEH_PI;
    node->angles[1] = VEH_PI;
    node->angles[2] = VEH_PI;
    node->segmentLength = 0.0f;
    node->nextNodeIndex = -1;
    node->previousNodeIndex = -1;
}

/* ------------------------------------------------------------------ */
/*  0x96178  VP_CopyNode                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96178, a6178_FUN_000a6178.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_CopyNode(const vehicle_path_node_t *source, vehicle_path_node_t *dest)
{
    Scr_SetString(&dest->targetname, source->targetname);
    Scr_SetString(&dest->target, source->target);
    dest->scriptObjectId = source->scriptObjectId;
    dest->useNodeAngles = source->useNodeAngles;
    dest->speed = source->speed;
    dest->lookAhead = source->lookAhead;
    memcpy(dest->origin, source->origin, sizeof(dest->origin));
    memcpy(dest->direction, source->direction, sizeof(dest->direction));
    memcpy(dest->angles, source->angles, sizeof(dest->angles));
    dest->segmentLength = source->segmentLength;
    dest->nextNodeIndex = source->nextNodeIndex;
    dest->previousNodeIndex = source->previousNodeIndex;
}

/* ------------------------------------------------------------------ */
/*  0x96284  VP_GetNodeIndex                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96284, a6284_VP_GetNodeIndex.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int VP_GetNodeIndex(uint16_t targetname, const vec3_t origin)
{
    if (targetname == 0) {
        return -1;
    }

    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        const vehicle_path_node_t *node = game_compat_veh_path_node_at(index);

        if (node->targetname != targetname) {
            continue;
        }

        if (origin == 0) {
            return index;
        }

        if (node->origin[0] == origin[0] && node->origin[1] == origin[1] && node->origin[2] == origin[2]) {
            return index;
        }
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/*  0x9636c  VP_GetNodeSpeed                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9636c, a636c_FUN_000a636c.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
float VP_GetNodeSpeed(int16_t nodeIndex)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(nodeIndex);
    const vehicle_path_node_t *previousNode = NULL;
    const vehicle_path_node_t *nextNode = NULL;
    float previousDistance = 0.0f;
    float nextDistance = 0.0f;
    float previousSpeed = -1.0f;
    float nextSpeed = -1.0f;

    if (node->speed >= 0.0f) {
        return node->speed;
    }

    if (node->previousNodeIndex >= 0) {
        previousNode = game_compat_veh_path_node_at(node->previousNodeIndex);
        for (int count = 0; count < vehPathNodeCount; count++) {
            previousDistance += previousNode->segmentLength;
            if (previousNode->speed >= 0.0f) {
                previousSpeed = previousNode->speed;
                break;
            }
            if (previousNode->previousNodeIndex < 0 || previousNode->previousNodeIndex == nodeIndex) {
                break;
            }
            previousNode = game_compat_veh_path_node_at(previousNode->previousNodeIndex);
        }
    }

    nextNode = node;
    for (int count = 0; count < vehPathNodeCount; count++) {
        if (nextNode->speed >= 0.0f) {
            nextSpeed = nextNode->speed;
            break;
        }
        if (nextNode->nextNodeIndex < 0 || nextNode->nextNodeIndex == nodeIndex) {
            break;
        }
        nextDistance += nextNode->segmentLength;
        nextNode = game_compat_veh_path_node_at(nextNode->nextNodeIndex);
    }

    if (previousSpeed < 0.0f && nextSpeed < 0.0f) {
        return 0.0f;
    }
    if (previousSpeed < 0.0f) {
        return nextSpeed;
    }
    if (nextSpeed < 0.0f) {
        return previousSpeed;
    }
    /* 0x9656a..0x965a5: the distance sum and the fraction are rounded to
     * their own float slots before the blend. */
    {
        const float distanceSum = previousDistance + nextDistance;
        float fraction;

        if (distanceSum <= 0.0f) {
            return 0.0f;
        }
        /* distanceSum is a single add stored to a float (native); the divide and
         * the (next-prev)*fraction+prev blend are width-sensitive -> shim. */
#if EMULATE_X87
        fraction = x87f_store_f32(x87f_div(x87f_load_f32(previousDistance), x87f_load_f32(distanceSum)));
        return x87f_store_f32(x87f_add(x87f_mul(x87f_sub(x87f_load_f32(nextSpeed), x87f_load_f32(previousSpeed)), x87f_load_f32(fraction)),
                                       x87f_load_f32(previousSpeed)));
#else
        fraction = previousDistance / distanceSum;
        return (nextSpeed - previousSpeed) * fraction + previousSpeed;
#endif
    }
}

/* ------------------------------------------------------------------ */
/*  0x965b8  VP_GetNodeLookAhead                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x965b8, a65b8_FUN_000a65b8.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
float VP_GetNodeLookAhead(int16_t nodeIndex)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(nodeIndex);
    const vehicle_path_node_t *previousNode = NULL;
    const vehicle_path_node_t *nextNode = NULL;
    float previousDistance = 0.0f;
    float nextDistance = 0.0f;
    float previousLookAhead = -1.0f;
    float nextLookAhead = -1.0f;

    if (node->lookAhead >= 0.0f) {
        return node->lookAhead;
    }

    if (node->previousNodeIndex >= 0) {
        previousNode = game_compat_veh_path_node_at(node->previousNodeIndex);
        for (int count = 0; count < vehPathNodeCount; count++) {
            previousDistance += previousNode->segmentLength;
            if (previousNode->lookAhead > 0.0f) {
                previousLookAhead = previousNode->lookAhead;
                break;
            }
            if (previousNode->previousNodeIndex < 0 || previousNode->previousNodeIndex == nodeIndex) {
                break;
            }
            previousNode = game_compat_veh_path_node_at(previousNode->previousNodeIndex);
        }
    }

    nextNode = node;
    for (int count = 0; count < vehPathNodeCount; count++) {
        if (nextNode->lookAhead > 0.0f) {
            nextLookAhead = nextNode->lookAhead;
            break;
        }
        if (nextNode->nextNodeIndex < 0 || nextNode->nextNodeIndex == nodeIndex) {
            break;
        }
        nextDistance += nextNode->segmentLength;
        nextNode = game_compat_veh_path_node_at(nextNode->nextNodeIndex);
    }

    if (previousLookAhead < 0.0f && nextLookAhead < 0.0f) {
        return 0.0f;
    }
    if (previousLookAhead < 0.0f) {
        return nextLookAhead;
    }
    if (nextLookAhead < 0.0f) {
        return previousLookAhead;
    }
    /* 0x967b6..0x967f1: the distance sum and the fraction are rounded to
     * their own float slots before the blend. */
    {
        const float distanceSum = previousDistance + nextDistance;
        float fraction;

        if (distanceSum <= 0.0f) {
            return 0.0f;
        }
        /* same shape as VP_GetNodeSpeed: native add + guard, shim the divide and
         * the (next-prev)*fraction+prev blend. */
#if EMULATE_X87
        fraction = x87f_store_f32(x87f_div(x87f_load_f32(previousDistance), x87f_load_f32(distanceSum)));
        return x87f_store_f32(
            x87f_add(x87f_mul(x87f_sub(x87f_load_f32(nextLookAhead), x87f_load_f32(previousLookAhead)), x87f_load_f32(fraction)),
                     x87f_load_f32(previousLookAhead)));
#else
        fraction = previousDistance / distanceSum;
        return (nextLookAhead - previousLookAhead) * fraction + previousLookAhead;
#endif
    }
}

/* ------------------------------------------------------------------ */
/*  0x96804  VP_GetNodeAngles                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96804, a6804_FUN_000a6804.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_GetNodeAngles(int16_t nodeIndex, vec3_t outAngles)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(nodeIndex);
    const vehicle_path_node_t *previousNode;
    const vehicle_path_node_t *nextNode;
    float previousDistance = 0.0f;
    float nextDistance = 0.0f;
    vec3_t previousAngles = {0.0f, 0.0f, 0.0f};
    vec3_t nextAngles = {0.0f, 0.0f, 0.0f};

    if (game_compat_veh_path_node_has_angles(node)) {
        game_compat_veh_path_node_copy_angles(node, outAngles);
        return;
    }

    previousNode = node;
    for (int count = 0; count < vehPathNodeCount; count++) {
        if (previousNode->previousNodeIndex < 0) {
            break;
        }
        if (previousNode->previousNodeIndex == nodeIndex) {
            break;
        }

        previousNode = game_compat_veh_path_node_at(previousNode->previousNodeIndex);
        previousDistance += previousNode->segmentLength;
        if (game_compat_veh_path_node_has_angles(previousNode)) {
            game_compat_veh_path_node_copy_angles(previousNode, previousAngles);
            break;
        }
    }

    nextNode = node;
    for (int count = 0; count < vehPathNodeCount; count++) {
        if (nextNode->nextNodeIndex < 0) {
            break;
        }
        if (nextNode->nextNodeIndex == nodeIndex) {
            break;
        }

        nextDistance += nextNode->segmentLength;
        nextNode = game_compat_veh_path_node_at(nextNode->nextNodeIndex);
        if (game_compat_veh_path_node_has_angles(nextNode)) {
            game_compat_veh_path_node_copy_angles(nextNode, nextAngles);
            break;
        }
    }

    if (game_compat_veh_path_vector_is_zero(previousAngles) && game_compat_veh_path_vector_is_zero(nextAngles)) {
        game_compat_veh_path_clear_vector(outAngles);
        return;
    }

    if (game_compat_veh_path_vector_is_zero(previousAngles)) {
        game_compat_veh_path_node_copy_angles(nextNode, outAngles);
        return;
    }

    if (game_compat_veh_path_vector_is_zero(nextAngles)) {
        game_compat_veh_path_node_copy_angles(previousNode, outAngles);
        return;
    }

    /* 0x96c19..0x96c57: the distance sum and the fraction are rounded to
     * their own float slots before the lerp. */
    {
        const float distanceSum = previousDistance + nextDistance;

        if (distanceSum <= 0.0f) {
            game_compat_veh_path_clear_vector(outAngles);
            return;
        }

        /* single divide stored to the game_compat_veh_path_lerp_angles float arg -> shim */
#if EMULATE_X87
        game_compat_veh_path_lerp_angles(previousAngles, nextAngles,
                                         x87f_store_f32(x87f_div(x87f_load_f32(previousDistance), x87f_load_f32(distanceSum))), outAngles);
#else
        game_compat_veh_path_lerp_angles(previousAngles, nextAngles, previousDistance / distanceSum, outAngles);
#endif
    }
}

/* ------------------------------------------------------------------ */
/*  0x96cc2 / 0x96d3e / 0x96dbc  path cursor scalar leaves           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96cc2, a6cc2_FUN_000a6cc2.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
float VP_GetPathPositionSpeed(const vehicle_path_position_t *position)
{
    return game_compat_veh_path_position_speed_at_node(position);
}

/* VERIFIED_DECOMPILER(0x96d3e, a6d3e_FUN_000a6d3e.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
float VP_GetPathPositionLookAhead(const vehicle_path_position_t *position)
{
    return game_compat_veh_path_position_look_ahead_at_node(position);
}

/* VERIFIED_DECOMPILER(0x96dbc, a6dbc_FUN_000a6dbc.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
float VP_GetPathPositionCurveFraction(const vehicle_path_position_t *position)
{
    return game_compat_veh_path_position_curve_fraction(position);
}

/* ------------------------------------------------------------------ */
/*  0x96e9a  VP_GetPathPositionAngles                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x96e9a, a6e9a_FUN_000a6e9a.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_GetPathPositionAngles(const vehicle_path_position_t *position, vec3_t outAngles)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(position->nodeIndex);

    if (node->nextNodeIndex < 0) {
        if (node->useNodeAngles != 0) {
            game_compat_veh_path_node_copy_angles(node, outAngles);
        }
        return;
    }

    const vehicle_path_node_t *nextNode = game_compat_veh_path_node_at(node->nextNodeIndex);
    if (node->useNodeAngles == 0 && nextNode->useNodeAngles == 0) {
        return;
    }

    vec3_t startAngles;
    vec3_t endAngles;
    if (node->useNodeAngles == 0) {
        startAngles[0] = outAngles[0];
        startAngles[1] = outAngles[1];
        startAngles[2] = outAngles[2];
        game_compat_veh_path_node_copy_angles(nextNode, endAngles);
    } else if (nextNode->useNodeAngles == 0) {
        game_compat_veh_path_node_copy_angles(node, startAngles);
        endAngles[0] = outAngles[0];
        endAngles[1] = outAngles[1];
        endAngles[2] = outAngles[2];
    } else {
        game_compat_veh_path_node_copy_angles(node, startAngles);
        game_compat_veh_path_node_copy_angles(nextNode, endAngles);
    }

    for (int axis = 0; axis < 3; axis++) {
        outAngles[axis] = AngleNormalize180(LerpAngle(startAngles[axis], endAngles[axis], position->fraction));
    }
}

/* ------------------------------------------------------------------ */
/*  0x9707e  VP_CalcPathPosition                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9707e, a707e_FUN_000a707e.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_CalcPathPosition(const vehicle_path_position_t *position, vec3_t outOrigin)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(position->nodeIndex);
    /* 0x970ac..0x970c4: lookAhead*speed is spilled to its own float slot
     * before the fraction*segmentLength term is added. */
    const float lookAheadDistance = position->lookAhead * position->speed;
    /* lookAheadDistance is a single mul stored to a float (native); distance =
     * fraction*segmentLength + lookAheadDistance is mul+add kept 80-bit until the
     * store (reading the stored lookAheadDistance) -> shim. */
#if EMULATE_X87
    float distance = x87f_store_f32(
        x87f_add(x87f_mul(x87f_load_f32(position->fraction), x87f_load_f32(node->segmentLength)), x87f_load_f32(lookAheadDistance)));
#else
    float distance = position->fraction * node->segmentLength + lookAheadDistance;
#endif

    for (int16_t count = 0; count < vehPathNodeCount; count++) {
        if (node->nextNodeIndex < 0 || node->segmentLength == 0.0f) {
            distance = 0.0f;
            break;
        }

        if (distance < node->segmentLength) {
            break;
        }

        distance -= node->segmentLength;
        node = game_compat_veh_path_node_at(node->nextNodeIndex);
    }

    /* origin[i] + direction[i]*distance: mul then add, 80-bit, one store */
#if EMULATE_X87
    for (int i = 0; i < 3; i++) {
        outOrigin[i] =
            x87f_store_f32(x87f_add(x87f_load_f32(node->origin[i]), x87f_mul(x87f_load_f32(node->direction[i]), x87f_load_f32(distance))));
    }
#else
    outOrigin[0] = node->origin[0] + node->direction[0] * distance;
    outOrigin[1] = node->origin[1] + node->direction[1] * distance;
    outOrigin[2] = node->origin[2] + node->direction[2] * distance;
#endif
}

/* ------------------------------------------------------------------ */
/*  0x97194  VP_ProjectPathPosition                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x97194, a7194_FUN_000a7194.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
qboolean VP_ProjectPathPosition(vehicle_path_position_t *position, const vec3_t projectDirection, int stopNodeIndex)
{
    int16_t nodeIndex = position->nodeIndex;
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(nodeIndex);
    float fraction = position->fraction;
    qboolean passedStopNode = qfalse;

    for (int16_t count = 0; count < vehPathNodeCount; count++) {
        const vehicle_path_node_t *nextNode;
        vec3_t enterDelta;
        vec3_t exitDelta;
        float enterDistance;
        float exitDistance;

        node = game_compat_veh_path_node_at(nodeIndex);
        if (nodeIndex == stopNodeIndex) {
            passedStopNode = qtrue;
        }

        if (node->nextNodeIndex < 0 || node->segmentLength == 0.0f) {
            fraction = 0.0f;
            break;
        }

        nextNode = game_compat_veh_path_node_at(node->nextNodeIndex);
        /* 0x9726c..0x97307: the origin deltas are spilled to float slots
         * before each dot, and the dots accumulate components 0,1,2 in
         * that order. */
        enterDelta[0] = position->origin[0] - node->origin[0];
        enterDelta[1] = position->origin[1] - node->origin[1];
        enterDelta[2] = position->origin[2] - node->origin[2];
        /* deltas are single subs stored to floats (native); each dot is a
         * 3-mul/2-add chain kept 80-bit until its store -> shim. */
#if EMULATE_X87
        enterDistance = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(enterDelta[0]), x87f_load_f32(projectDirection[0])),
                                                         x87f_mul(x87f_load_f32(enterDelta[1]), x87f_load_f32(projectDirection[1]))),
                                                x87f_mul(x87f_load_f32(enterDelta[2]), x87f_load_f32(projectDirection[2]))));
#else
        enterDistance = enterDelta[0] * projectDirection[0] + enterDelta[1] * projectDirection[1] + enterDelta[2] * projectDirection[2];
#endif
        exitDelta[0] = nextNode->origin[0] - position->origin[0];
        exitDelta[1] = nextNode->origin[1] - position->origin[1];
        exitDelta[2] = nextNode->origin[2] - position->origin[2];
#if EMULATE_X87
        exitDistance = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(exitDelta[0]), x87f_load_f32(projectDirection[0])),
                                                        x87f_mul(x87f_load_f32(exitDelta[1]), x87f_load_f32(projectDirection[1]))),
                                               x87f_mul(x87f_load_f32(exitDelta[2]), x87f_load_f32(projectDirection[2]))));
#else
        exitDistance = exitDelta[0] * projectDirection[0] + exitDelta[1] * projectDirection[1] + exitDelta[2] * projectDirection[2];
#endif

        if (enterDistance == 0.0f && exitDistance == 0.0f) {
            fraction = 0.0f;
            break;
        }

        /* 0x9733f..0x9735f advances only when either projection is
         * ordered-negative.  Unordered values therefore take this fraction
         * path too; spelling this as >= would reject them. */
        if (!(enterDistance < 0.0f) && !(exitDistance < 0.0f)) {
            /* enterDistance / (enterDistance + exitDistance): the sum is an
             * inline 80-bit add, then divide, stored to fraction -> shim. */
#if EMULATE_X87
            fraction =
                x87f_store_f32(x87f_div(x87f_load_f32(enterDistance), x87f_add(x87f_load_f32(enterDistance), x87f_load_f32(exitDistance))));
#else
            fraction = enterDistance / (enterDistance + exitDistance);
#endif
            break;
        }

        nodeIndex = node->nextNodeIndex;
    }

    position->nodeIndex = nodeIndex;
    position->reachedEnd = node->nextNodeIndex < 0 ? 1 : 0;
    position->fraction = fraction;
    position->speed = VP_GetPathPositionSpeed(position);
    position->lookAhead = VP_GetPathPositionLookAhead(position);
    position->curveFraction = VP_GetPathPositionCurveFraction(position);
    return passedStopNode;
}

/* ------------------------------------------------------------------ */
/*  0x973e0 / 0x9743e  path cursor target-node refresh leaves         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x973e0, a73e0_FUN_000a73e0.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_UpdatePathPositionTargetNode(vehicle_path_position_t *position)
{
    const int16_t nodeIndex = VP_GetNodeIndex(position->targetNode.targetname, NULL);

    if (nodeIndex >= 0) {
        VP_CopyNode(&position->targetNode, game_compat_veh_path_node_at(nodeIndex));
    }
}

/* VERIFIED_DECOMPILER(0x9743e, a743e_FUN_000a743e.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VP_UpdatePathPositionCachedNode(vehicle_path_position_t *position)
{
    const int16_t nodeIndex = VP_GetNodeIndex(position->targetNode.targetname, NULL);

    if (nodeIndex >= 0) {
        VP_CopyNode(&position->cachedNode, game_compat_veh_path_node_at(nodeIndex));
    }
}

/* ------------------------------------------------------------------ */
/*  0x9749c  VP_DrawVehiclePathPosition                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9749c, a749c_FUN_000a749c.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
static void VP_DrawVehiclePathPosition(vehicle_path_position_t *position)
{
    static const vec4_t currentColor = {0.0f, 1.0f, 0.0f, 1.0f};
    static const vec4_t linkColor = {0.0f, 0.0f, 1.0f, 1.0f};
    vehicle_path_position_t previous;
    vehicle_path_position_t current;
    int lastNodeIndex = -1;
    int done = 0;
    int iterations = 0;

    memcpy(&previous, position, sizeof(previous));
    memcpy(&current, position, sizeof(current));
    vehDebugPathSegmentNeedsInit = qtrue;

    while (!done) {
        iterations++;
        if (iterations > VEH_PATH_DEBUG_MAX_ITERS) {
            Com_Printf("WARNING: Invalid vehicle path.  Possible infinite loop\n");
            break;
        }

        if (previous.nodeIndex != position->nodeIndex) {
            lastNodeIndex = position->nodeIndex;
        }

        memcpy(&previous, &current, sizeof(previous));
        done = G_VehUpdatePathPos(&current, lastNodeIndex);
        if (current.reachedEnd != 0 || done != 0) {
            done = 1;
        }

        VP_DebugPathSegment(previous.origin, current.origin, done);
    }

    iterations = 0;
    for (int16_t nodeIndex = position->nodeIndex;;) {
        const vehicle_path_node_t *node;
        vec3_t mins;
        vec3_t maxs;

        if (iterations >= vehPathNodeCount) {
            return;
        }
        iterations++;

        node = game_compat_veh_path_node_at(nodeIndex);
        maxs[0] = node->origin[0] + VEH_PATH_DEBUG_BOX_HALF_SIZE;
        maxs[1] = node->origin[1] + VEH_PATH_DEBUG_BOX_HALF_SIZE;
        maxs[2] = node->origin[2] + VEH_PATH_DEBUG_BOX_HALF_SIZE;
        mins[0] = node->origin[0] - VEH_PATH_DEBUG_BOX_HALF_SIZE;
        mins[1] = node->origin[1] - VEH_PATH_DEBUG_BOX_HALF_SIZE;
        mins[2] = node->origin[2] - VEH_PATH_DEBUG_BOX_HALF_SIZE;

        if (node == game_compat_veh_path_node_at(position->nodeIndex)) {
            G_DebugBox(maxs, mins, currentColor, 1, 0);
        } else {
            G_DebugBox(maxs, mins, linkColor, 1, 0);
        }

        if (node->nextNodeIndex < 0) {
            return;
        }
        if (node->nextNodeIndex == position->nodeIndex) {
            return;
        }
        nodeIndex = node->nextNodeIndex;
    }
}

/* ------------------------------------------------------------------ */
/*  0x977ca  G_InitVehiclePaths                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x977ca, a77ca_G_InitVehiclePaths.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_InitVehiclePaths(void)
{
    vehPathNodeCount = 0;
}

/* ------------------------------------------------------------------ */
/*  0x977e6 / 0x9787c  G_FreeVehiclePaths                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x977e6, a77e6_G_FreeVehiclePaths.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_FreeVehiclePaths(void)
{
    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_node_t *node = game_compat_veh_path_node_at(index);

        Scr_FreeEntityNum(node->scriptObjectId, SCRIPT_OBJECT_VEHICLE_NODE);
        Scr_SetString(&node->targetname, 0);
        Scr_SetString(&node->target, 0);
    }

    vehPathNodeCount = 0;
}

/* VERIFIED_DECOMPILER(0x9787c, a787c_G_FreeVehiclePathsScriptInfo.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_FreeVehiclePathsScriptInfo(void)
{
    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        Scr_FreeEntityNum(game_compat_veh_path_node_at(index)->scriptObjectId, SCRIPT_OBJECT_VEHICLE_NODE);
    }
}

/* ------------------------------------------------------------------ */
/*  0x978e0  G_SetupVehiclePaths                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x978e0, a78e0_G_SetupVehiclePaths.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_SetupVehiclePaths(void)
{
    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_node_t *node = game_compat_veh_path_node_at(index);

        if (node->target != 0) {
            node->nextNodeIndex = (int16_t)VP_GetNodeIndex(node->target, NULL);
        }

        for (int16_t otherIndex = 0; otherIndex < vehPathNodeCount; otherIndex++) {
            const vehicle_path_node_t *otherNode = game_compat_veh_path_node_at(otherIndex);

            if (index != otherIndex && node->targetname == otherNode->target) {
                node->previousNodeIndex = otherIndex;
                break;
            }
        }

        if (node->nextNodeIndex == index) {
            node->nextNodeIndex = -1;
        }
        if (node->previousNodeIndex == index) {
            node->previousNodeIndex = -1;
        }
    }

    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_node_t *node = game_compat_veh_path_node_at(index);

        if (node->nextNodeIndex >= 0) {
            const vehicle_path_node_t *nextNode = game_compat_veh_path_node_at(node->nextNodeIndex);

            node->direction[0] = nextNode->origin[0] - node->origin[0];
            node->direction[1] = nextNode->origin[1] - node->origin[1];
            node->direction[2] = nextNode->origin[2] - node->origin[2];
            node->segmentLength = VectorNormalize(node->direction);

            if (node->useNodeAngles == 0) {
                vectoangles(node->direction, node->angles);
            }
        }
    }

    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_node_t *node = game_compat_veh_path_node_at(index);

        node->speed = VP_GetNodeSpeed(index);
        node->lookAhead = VP_GetNodeLookAhead(index);
        if (node->speed < 0.0f) {
            Com_Error(1, VEH_PATH_NODE_NEGATIVE_SPEED_ERROR, node->origin[0], node->origin[1], node->origin[2]);
        }

        if (node->useNodeAngles != 0) {
            VP_GetNodeAngles(index, node->angles);
        }

        node->angles[0] = AngleNormalize180(node->angles[0]);
        node->angles[1] = AngleNormalize180(node->angles[1]);
        node->angles[2] = AngleNormalize180(node->angles[2]);

        if (node->speed <= 0.0f || node->lookAhead <= 0.0f) {
            node->nextNodeIndex = -1;
        }
        if (node->nextNodeIndex < 0) {
            if (node->speed <= 0.0f) {
                node->speed = 1.0f;
            }
            if (node->lookAhead <= 0.0f) {
                node->lookAhead = 1.0f;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x97c3c  G_DrawVehiclePaths                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x97c3c, a7c3c_G_DrawVehiclePaths.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_DrawVehiclePaths(void)
{
    const char *pathName = g_vehicleDrawPath.string;

    if (pathName[0] == '\0' || pathName[0] == '0') {
        return;
    }

    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_position_t position;

        if (strcmp(SL_ConvertToString(game_compat_veh_path_node_at(index)->targetname), pathName) != 0) {
            continue;
        }

        memset(&position, 0, sizeof(position));
        G_VehSetUpPathPos(&position, index);
        VP_DrawVehiclePathPosition(&position);
        return;
    }
}

/* ------------------------------------------------------------------ */
/*  0x97d40 / 0x97e08  path position lifecycle                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x97d40, a7d40_G_VehInitPathPos.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_VehInitPathPos(vehicle_path_position_t *position)
{
    position->nodeIndex = -1;
    position->reachedEnd = 0;
    position->fraction = 0.0f;
    position->speed = 0.0f;
    position->lookAhead = 0.0f;
    position->curveFraction = 0.0f;
    game_compat_veh_path_clear_vector(position->origin);
    game_compat_veh_path_clear_vector(position->currentAngles);
    game_compat_veh_path_clear_vector(position->lookAheadOrigin);
    VP_InitNode(&position->targetNode, -1);
    VP_InitNode(&position->cachedNode, -1);
}

/* VERIFIED_DECOMPILER(0x97e08, a7e08_G_VehFreePathPos.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_VehFreePathPos(vehicle_path_position_t *position)
{
    Scr_SetString(&position->targetNode.targetname, 0);
    Scr_SetString(&position->targetNode.target, 0);
    Scr_SetString(&position->cachedNode.targetname, 0);
    Scr_SetString(&position->cachedNode.target, 0);
}

/* ------------------------------------------------------------------ */
/*  0x97e78 / 0x97fa4 / 0x98154  path position update                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x97e78, a7e78_G_VehSetUpPathPos.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_VehSetUpPathPos(vehicle_path_position_t *position, int16_t nodeIndex)
{
    const vehicle_path_node_t *node = game_compat_veh_path_node_at(nodeIndex);

    position->nodeIndex = nodeIndex;
    position->reachedEnd = 0;
    position->fraction = 0.0f;
    position->speed = node->speed;
    position->lookAhead = node->lookAhead;
    position->curveFraction = node->useNodeAngles != 0 ? 1.0f : 0.0f;
    memcpy(position->origin, node->origin, sizeof(position->origin));
    memcpy(position->currentAngles, node->angles, sizeof(position->currentAngles));
    memcpy(position->lookAheadOrigin, node->origin, sizeof(position->lookAheadOrigin));
    VP_InitNode(&position->targetNode, -1);
    VP_InitNode(&position->cachedNode, -1);
}

/* VERIFIED_DECOMPILER(0x97fa4, a7fa4_G_VehUpdatePathPos.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int G_VehUpdatePathPos(vehicle_path_position_t *position, int16_t lastNodeIndex)
{
    qboolean reachedLastNode = qfalse;

    if (position->reachedEnd != 0) {
        return qfalse;
    }

    VP_UpdatePathPositionTargetNode(position);
    VP_CalcPathPosition(position, position->lookAheadOrigin);

    {
        vec3_t pathDelta;

        pathDelta[0] = position->lookAheadOrigin[0] - position->origin[0];
        pathDelta[1] = position->lookAheadOrigin[1] - position->origin[1];
        pathDelta[2] = position->lookAheadOrigin[2] - position->origin[2];

        if (VectorNormalize(pathDelta) > 0.0f) {
            vectoangles(pathDelta, position->currentAngles);
            position->currentAngles[0] = AngleNormalize180(position->currentAngles[0]);
            position->currentAngles[1] = AngleNormalize180(position->currentAngles[1]);
            position->currentAngles[2] = AngleNormalize180(position->currentAngles[2]);

            position->origin[0] += position->speed * VEH_PHYSICS_FRAME_SECONDS * pathDelta[0];
            position->origin[1] += position->speed * VEH_PHYSICS_FRAME_SECONDS * pathDelta[1];
            position->origin[2] += position->speed * VEH_PHYSICS_FRAME_SECONDS * pathDelta[2];

            reachedLastNode = VP_ProjectPathPosition(position, pathDelta, lastNodeIndex);
            VP_GetPathPositionAngles(position, position->currentAngles);
        } else {
            position->reachedEnd = 1;
        }
    }

    VP_UpdatePathPositionCachedNode(position);
    return reachedLastNode;
}

/* VERIFIED_DECOMPILER(0x98154, a8154_G_VehSetSwitchNode.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_VehSetSwitchNode(vehicle_path_position_t *position, int16_t fromNodeIndex, int16_t toNodeIndex)
{
    VP_InitNode(&position->targetNode, -1);
    VP_InitNode(&position->cachedNode, -1);

    if (fromNodeIndex >= 0 && toNodeIndex >= 0) {
        const vehicle_path_node_t *fromNode = game_compat_veh_path_node_at(fromNodeIndex);
        const vehicle_path_node_t *toNode = game_compat_veh_path_node_at(toNodeIndex);

        VP_CopyNode(fromNode, &position->targetNode);
        VP_CopyNode(fromNode, &position->cachedNode);
        position->targetNode.nextNodeIndex = toNodeIndex;
        position->targetNode.direction[0] = toNode->origin[0] - fromNode->origin[0];
        position->targetNode.direction[1] = toNode->origin[1] - fromNode->origin[1];
        position->targetNode.direction[2] = toNode->origin[2] - fromNode->origin[2];
        position->targetNode.segmentLength = VectorNormalize(position->targetNode.direction);
    }
}

/* ------------------------------------------------------------------ */
/*  0x98272  SP_info_vehicle_node                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x98272, a8272_SP_info_vehicle_node.c, VERIFY-P1-SPAWN-2026-06-17): DATAFLOW_VERIFIED - node limit, node initialization, spawn-var parse, angle-source flag store, targetname check, and speed scaling checked against current decompiler output. */
void SP_info_vehicle_node(qboolean useNodeAngles)
{
    vehicle_path_node_t *node;

    if (vehPathNodeCount >= VEH_MAX_PATH_NODES) {
        Com_Error(1, VEH_PATH_NODE_MAX_COUNT_ERROR, VEH_MAX_PATH_NODES);
    }

    node = game_compat_veh_path_node_at(vehPathNodeCount);
    VP_InitNode(node, vehPathNodeCount);
    vehPathNodeCount++;

    VP_ParseVehicleNodeSpawnVars(node);
    node->useNodeAngles = useNodeAngles;

    if (node->targetname == 0) {
        Com_Error(1, VEH_PATH_NODE_NO_NAME_ERROR, node->origin[0], node->origin[1], node->origin[2]);
    }

    if (node->speed >= 0.0f) {
        node->speed *= VEH_INFO_SPEED_SCALE;
    }
}

/* ------------------------------------------------------------------ */
/*  0x9836c  GScr_GetVehicleNodeIndex                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9836c, a836c_GScr_GetVehicleNodeIndex.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int16_t GScr_GetVehicleNodeIndex(uint32_t paramIndex)
{
    int classnum;
    uint32_t objectNum = Scr_GetEntityNum(paramIndex, &classnum);

    if (classnum == SCRIPT_OBJECT_VEHICLE_NODE && objectNum < (uint32_t)vehPathNodeCount) {
        return (int16_t)objectNum;
    }

    Scr_ParamError(paramIndex, VEH_NOT_VEHICLE_NODE_ERROR);
    return -1;
}

/* ------------------------------------------------------------------ */
/*  0x983d4  GScr_AddFieldsForVehicleNode                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x983d4, a83d4_GScr_AddFieldsForVehicleNode.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void GScr_AddFieldsForVehicleNode(void)
{
    const uint16_t classnum = g_scr_data.classMap[SCRIPT_OBJECT_VEHICLE_NODE].classnum;

    for (uint16_t index = 0; vehicleNodeSpawnFields[index].name != 0; index++) {
        const vehicle_node_spawn_field_t *field = &vehicleNodeSpawnFields[index];

        switch (field->type) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 9:
            Scr_AddClassField(classnum, field->name, index);
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x9846c  GScr_GetVehicleNodeField                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9846c, a846c_GScr_GetVehicleNodeField.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void GScr_GetVehicleNodeField(int entityNum, int fieldIndex)
{
    const vehicle_node_spawn_field_t *field = &vehicleNodeSpawnFields[fieldIndex];

    Scr_GetGenericField(game_compat_veh_path_node_at(entityNum), field->type, field->offset);
}

/* ------------------------------------------------------------------ */
/*  0x984d2  GScr_GetVehicleNode                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x984d2, a84d2_GScr_GetVehicleNode.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void GScr_GetVehicleNode(void)
{
    uint16_t value = Scr_GetConstString(0);
    const char *fieldName = Scr_GetString(1);
    int fieldIndex = (int16_t)Scr_GetOffset(g_scr_data.classMap[SCRIPT_OBJECT_VEHICLE_NODE].classnum, fieldName);
    vehicle_path_node_t *match = NULL;

    if (fieldIndex < 0) {
        return;
    }

    if (vehicleNodeSpawnFields[fieldIndex].type != 5) {
        Scr_ParamError(1, VEH_NODE_KEY_NOT_STRING_ERROR);
    }

    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_node_t *node = game_compat_veh_path_node_at(index);
        uint16_t fieldValue = *(uint16_t *)(void *)&((uint8_t *)(void *)node)[vehicleNodeSpawnFields[fieldIndex].offset];

        if (fieldValue == 0 || fieldValue != value) {
            continue;
        }

        if (match != NULL) {
            Scr_Error(VEH_NODE_MULTIPLE_MATCH_ERROR);
        }
        match = node;
    }

    if (match != NULL) {
        Scr_AddEntityNum(match->scriptObjectId, SCRIPT_OBJECT_VEHICLE_NODE);
    }
}

/* ------------------------------------------------------------------ */
/*  0x9860a  GScr_GetVehicleNodeArray                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x9860a, a860a_GScr_GetVehicleNodeArray.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void GScr_GetVehicleNodeArray(void)
{
    uint16_t value = Scr_GetConstString(0);
    const char *fieldName = Scr_GetString(1);
    int fieldIndex = (int16_t)Scr_GetOffset(g_scr_data.classMap[SCRIPT_OBJECT_VEHICLE_NODE].classnum, fieldName);

    if (fieldIndex < 0) {
        return;
    }

    if (vehicleNodeSpawnFields[fieldIndex].type != 5) {
        Scr_ParamError(1, VEH_NODE_KEY_NOT_STRING_ERROR);
    }

    Scr_MakeArray();
    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        vehicle_path_node_t *node = game_compat_veh_path_node_at(index);
        uint16_t fieldValue = *(uint16_t *)(void *)&((uint8_t *)(void *)node)[vehicleNodeSpawnFields[fieldIndex].offset];

        if (fieldValue != 0 && fieldValue == value) {
            Scr_AddEntityNum(node->scriptObjectId, SCRIPT_OBJECT_VEHICLE_NODE);
            Scr_AddArray();
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x98726  GScr_GetAllVehicleNodes                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x98726, a8726_GScr_GetAllVehicleNodes.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void GScr_GetAllVehicleNodes(void)
{
    Scr_MakeArray();
    for (int16_t index = 0; index < vehPathNodeCount; index++) {
        Scr_AddEntityNum(game_compat_veh_path_node_at(index)->scriptObjectId, SCRIPT_OBJECT_VEHICLE_NODE);
        Scr_AddArray();
    }
}

/* ------------------------------------------------------------------ */
/*  0x88561  VEH_UpdatePath                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x88561, 98561_FUN_00098561.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void VEH_UpdatePath(gentity_t *ent)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)ent->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    vehicle_path_position_t *cursor = game_compat_veh_path_cursor(vehicleState);
    vehicle_path_position_t nextPosition;
    const float previousSpeed = game_compat_veh_entity_current_speed(ent);
    float *cursorFraction = game_compat_veh_path_cursor_fraction_slot(vehicleState);
    qboolean reachedWaitNode = qfalse;

    memcpy(&nextPosition, cursor, sizeof(nextPosition));

    if (cursor->nodeIndex < 0) {
        return;
    }

    if (game_compat_veh_path_speed_mode(vehicleState) == VEH_PATH_SPEED_MODE_NONE) {
        game_compat_veh_set_entity_current_speed(ent, cursor->speed);
    } else {
        float targetSpeed;

        if (game_compat_veh_path_speed_mode(vehicleState) == VEH_PATH_SPEED_MODE_PATH) {
            targetSpeed = cursor->speed;
        } else {
            targetSpeed = vehicleState->viewState[3];
        }

        /* 0x88634..0x886e8: inline approach-step, NOT the VEH_TrackValue
         * (0x7c525) algorithm -- no epsilon and no fabs; the overshoot is
         * clamped back to the target after the step, and the rate*frame
         * product stays in the 80-bit chain of each step. */
        if (targetSpeed > game_compat_veh_entity_current_speed(ent)) {
            /* rate*frame + speed / speed - rate*frame: mul then add/sub kept
             * 80-bit until the stored float arg -> shim. */
#if EMULATE_X87
            game_compat_veh_set_entity_current_speed(
                ent, x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(vehicleState->viewState[4]), x87f_load_f32(VEH_PHYSICS_FRAME_SECONDS)),
                                             x87f_load_f32(game_compat_veh_entity_current_speed(ent)))));
#else
            game_compat_veh_set_entity_current_speed(ent, vehicleState->viewState[4] * VEH_PHYSICS_FRAME_SECONDS +
                                                              game_compat_veh_entity_current_speed(ent));
#endif
            if (game_compat_veh_entity_current_speed(ent) > targetSpeed) {
                game_compat_veh_set_entity_current_speed(ent, targetSpeed);
            }
        } else {
#if EMULATE_X87
            game_compat_veh_set_entity_current_speed(ent, x87f_store_f32(x87f_sub(x87f_load_f32(game_compat_veh_entity_current_speed(ent)),
                                                                                  x87f_mul(x87f_load_f32(vehicleState->viewState[4]),
                                                                                           x87f_load_f32(VEH_PHYSICS_FRAME_SECONDS)))));
#else
            game_compat_veh_set_entity_current_speed(ent, game_compat_veh_entity_current_speed(ent) -
                                                              vehicleState->viewState[4] * VEH_PHYSICS_FRAME_SECONDS);
#endif
            if (targetSpeed > game_compat_veh_entity_current_speed(ent)) {
                game_compat_veh_set_entity_current_speed(ent, targetSpeed);
            }
        }

        if (game_compat_veh_path_speed_mode(vehicleState) == VEH_PATH_SPEED_MODE_PATH &&
            game_compat_veh_entity_current_speed(ent) == targetSpeed) {
            game_compat_veh_set_path_speed_mode(vehicleState, VEH_PATH_SPEED_MODE_NONE);
        }
    }

    if (cursor->speed > 0.0f) {
        /* speed/cursor->speed divide then add-into, 80-bit, one store -> shim */
#if EMULATE_X87
        *cursorFraction =
            x87f_store_f32(x87f_add(x87f_load_f32(*cursorFraction),
                                    x87f_div(x87f_load_f32(game_compat_veh_entity_current_speed(ent)), x87f_load_f32(cursor->speed))));
#else
        *cursorFraction += game_compat_veh_entity_current_speed(ent) / cursor->speed;
#endif
    }

    G_VehUpdatePathPos(&nextPosition, vehicleState->pathNodeIndex);
    while (*cursorFraction > 1.0f) {
        *cursorFraction -= 1.0f;
        memcpy(cursor, &nextPosition, sizeof(*cursor));

        if (G_VehUpdatePathPos(&nextPosition, vehicleState->pathNodeIndex)) {
            reachedWaitNode = qtrue;
        }
    }

    if (cursor->reachedEnd != 0) {
        game_compat_veh_set_entity_current_speed(ent, 0.0f);
    }

    for (int axis = 0; axis < 3; axis++) {
        vehicleState->origin[axis] = cursor->origin[axis] + (nextPosition.origin[axis] - cursor->origin[axis]) * *cursorFraction;
        vehicleState->viewClampTargetAngles[axis] =
            LerpAngle(cursor->currentAngles[axis], nextPosition.currentAngles[axis], *cursorFraction);
    }

    vehicleState->viewClampTargetAngles[0] =
        VEH_LerpAngle(vehicleState->viewClampTargetAngles[0], vehicleState->previousAngles[0], VEH_PATH_ANGLE_SMOOTH_PITCH);
    vehicleState->viewClampTargetAngles[1] =
        VEH_LerpAngle(vehicleState->viewClampTargetAngles[1], vehicleState->previousAngles[1], VEH_PATH_ANGLE_SMOOTH_YAW);
    vehicleState->viewClampTargetAngles[2] =
        VEH_LerpAngle(vehicleState->viewClampTargetAngles[2], vehicleState->previousAngles[2], VEH_PATH_ANGLE_SMOOTH_PITCH);

    if (g_vehicleDebug.integer != 0) {
        VEH_DebugBox(cursor->lookAheadOrigin, 8.0f, 0.0f, 1.0f, 1.0f);
    }

    /* 0x889a8..0x88a0d: the deltas are stored to the velocity slots first,
     * then scaled in place; the multiplier is the exact float constant
     * 20.0f (0x41a00000), not the folded 1.0f/0.05f quotient. */
    for (int axis = 0; axis < 3; axis++) {
        vehicleState->velocity[axis] = vehicleState->origin[axis] - vehicleState->previousOrigin[axis];
    }
    for (int axis = 0; axis < 3; axis++) {
        vehicleState->velocity[axis] *= 20.0f;
    }
    for (int axis = 0; axis < 3; axis++) {
        vehicleState->angularVelocity[axis] = 0.0f;
    }

    vehicleState->angularVelocity[0] = game_compat_veh_entity_current_speed(ent);
    AnglesSubtract(vehicleState->viewClampTargetAngles, vehicleState->previousAngles, vehicleState->acceleration);

    /* 0x88a5f..0x88a92: multiplier is the exact float constant 20.0f
     * (0x41a00000), not the folded 1.0f/0.05f quotient. */
    for (int axis = 0; axis < 3; axis++) {
        vehicleState->acceleration[axis] *= 20.0f;
    }

    if (reachedWaitNode && vehicleState->pathNodeIndex >= 0) {
        Scr_Notify(ent, scr_const_reached_wait_node, 0);
    }
    if (cursor->reachedEnd != 0) {
        Scr_Notify(ent, scr_const_reached_end_node, 0);
    }

    {
        /* single divide stored to a float -> shim */
#if EMULATE_X87
        float speedFraction =
            x87f_store_f32(x87f_div(x87f_load_f32(game_compat_veh_entity_current_speed(ent)), x87f_load_f32(vehicleInfo->pathSpeedDenom)));
#else
        float speedFraction = game_compat_veh_entity_current_speed(ent) / vehicleInfo->pathSpeedDenom;
#endif

        speedFraction = game_compat_veh_clamp_float(speedFraction, 0.0f, 1.0f);
        vehicleState->runSoundBlendRepeatDelay = speedFraction;
        vehicleState->idleSoundBlendRepeatDelay = 1.0f - speedFraction;
    }

    if (vehicleState->waitNodeSpeedThreshold >= 0.0f) {
        const float threshold = vehicleState->waitNodeSpeedThreshold;
        const float currentSpeed = game_compat_veh_entity_current_speed(ent);

        if ((previousSpeed <= threshold && threshold <= currentSpeed) || (currentSpeed <= threshold && threshold <= previousSpeed)) {
            Scr_Notify(ent, scr_const_reached_wait_node_threshold, 0);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x88c78  VEH_FindNextPassengerSlot                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x88c78, 98c78_FUN_00098c78.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int VEH_FindNextPassengerSlot(const vehicle_state_t *vehicleState, int currentSlot, qboolean reverse)
{
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    const int step = reverse ? -1 : 1;
    int slot = currentSlot + step;

    for (;;) {
        if (slot == currentSlot) {
            return 0;
        }
        if (slot == 0) {
            slot += step;
            continue;
        }
        if (slot > VEH_PASSENGER_SLOT_LAST) {
            slot = -step;
            slot += step;
            continue;
        }
        if (slot <= 0) {
            slot = VEH_PASSENGER_SLOT_WRAP;
            slot += step;
            continue;
        }

        if ((slot == VEH_PASSENGER_SLOT_GUNNER && vehicleInfo->gunnerSeatEnabled == 0) ||
            (slot > VEH_PASSENGER_SLOT_GUNNER && slot <= VEH_PASSENGER_SLOT_LAST &&
             vehicleInfo->extraPassengerCount <= slot - VEH_PASSENGER_EXTRA_SLOT_FIRST) ||
            vehicleState->passengerEntityNums[slot] != ENTITYNUM_NONE) {
            slot += step;
            continue;
        }

        return slot;
    }
}

/* ------------------------------------------------------------------ */
/*  0x88d5c  VEH_GetPassengerTagBoneIndex                             */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x88d5c, 98d5c_FUN_00098d5c.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int VEH_GetPassengerTagBoneIndex(const vehicle_state_t *vehicleState, uint32_t passengerSlot)
{
    if (passengerSlot == VEH_PASSENGER_SLOT_DRIVER) {
        return vehicleState->driverTagIndex;
    }

    if (passengerSlot == VEH_PASSENGER_SLOT_GUNNER) {
        return vehicleState->gunnerTagIndex;
    }

    if (passengerSlot > VEH_PASSENGER_SLOT_GUNNER && passengerSlot < VEH_PASSENGER_SLOT_COUNT) {
        return vehicleState->passengerTagIndices[passengerSlot - VEH_PASSENGER_EXTRA_SLOT_FIRST];
    }

    Com_Error(1, VEH_PATH_INVALID_POSITION_ERROR);
    return -1;
}

/* ------------------------------------------------------------------ */
/*  0x88dee  VEH_FindValidDismountSpot                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x88dee, 98dee_VEH_FindValidDismountSpot.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
qboolean VEH_FindValidDismountSpot(gentity_t *vehicleEnt, const float *origin, const float *mins, const float *maxs, float *outOrigin,
                                   int passEntityNum)
{
    vehicle_state_t *vehicleState = (vehicle_state_t *)vehicleEnt->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    trace_t trace;

    if (vehicleInfo->maxSpeed > 0.0f) {
        vec3_t forward;
        vec3_t right;
        vec3_t direction;
        float localForward;
        float localRight;
        float forwardOffset;
        float rightOffset;
        float forwardLimit;
        float rightLimit;
        uint32_t attempt;

        AngleVectors(vehicleState->viewClampTargetAngles, forward, right, NULL);

        if (game_compat_veh_length3(vehicleState->velocity) > 0.0f) {
            direction[0] = -vehicleState->velocity[0];
            direction[1] = -vehicleState->velocity[1];
        } else {
            direction[0] = origin[0] - vehicleState->origin[0];
            direction[1] = origin[1] - vehicleState->origin[1];
        }
        direction[2] = 0.0f;

        if (game_compat_veh_length3(direction) < VEH_DISMOUNT_MIN_DIR_LENGTH) {
            direction[0] = 1.0f;
            direction[1] = 0.0f;
        }

        /* right/forward . direction dots: 3-mul/2-add chains kept 80-bit until
         * the store -> shim. */
#if EMULATE_X87
        localRight = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(right[0]), x87f_load_f32(direction[0])),
                                                      x87f_mul(x87f_load_f32(right[1]), x87f_load_f32(direction[1]))),
                                             x87f_mul(x87f_load_f32(right[2]), x87f_load_f32(direction[2]))));
        localForward = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(forward[0]), x87f_load_f32(direction[0])),
                                                        x87f_mul(x87f_load_f32(forward[1]), x87f_load_f32(direction[1]))),
                                               x87f_mul(x87f_load_f32(forward[2]), x87f_load_f32(direction[2]))));
        /* SideOffset + maxs[1] + PAD: 2-add chain kept 80-bit, one store -> shim */
        rightLimit =
            x87f_store_f32(x87f_add(x87f_add(x87f_load_f32(game_compat_veh_dismount_side_offset(vehicleInfo)), x87f_load_f32(maxs[1])),
                                    x87f_load_f32(VEH_DISMOUNT_SIDE_PAD)));
#else
        localRight = right[0] * direction[0] + right[1] * direction[1] + right[2] * direction[2];
        localForward = forward[0] * direction[0] + forward[1] * direction[1] + forward[2] * direction[2];
        rightLimit = game_compat_veh_dismount_side_offset(vehicleInfo) + maxs[1] + VEH_DISMOUNT_SIDE_PAD;
#endif
        /* 0x89009..0x8905a: maxs[0] + max(fwd,back) is rounded to its own
         * float slot before the pad is added (pad + sum order). */
        /* 0x88fef..0x89034 is an ordered-greater branch, not fmaxf:
         * unordered inputs select the forward offset. */
        const float longitudinalOffset = vehicleInfo->dismountBackOffset > vehicleInfo->dismountForwardOffset
                                             ? vehicleInfo->dismountBackOffset
                                             : vehicleInfo->dismountForwardOffset;
        forwardLimit = maxs[0] + longitudinalOffset;
        forwardLimit = VEH_DISMOUNT_FORWARD_PAD + forwardLimit;

        if (fabsf(localForward) < fabsf(localRight)) {
            /* single divide (fabsf is exact) -> shim; *= scale is single mul (native) */
#if EMULATE_X87
            const float scale = x87f_store_f32(x87f_div(x87f_load_f32(rightLimit), x87f_load_f32(fabsf(localRight))));
#else
            const float scale = rightLimit / fabsf(localRight);
#endif

            localRight *= scale;
            localForward *= scale;
            if (localForward < 0.0f) {
                if (localForward > -maxs[0]) {
                    localForward = -maxs[0];
                }
            } else if (localForward < maxs[0]) {
                localForward = maxs[0];
            }
        } else {
#if EMULATE_X87
            const float scale = x87f_store_f32(x87f_div(x87f_load_f32(forwardLimit), x87f_load_f32(fabsf(localForward))));
#else
            const float scale = forwardLimit / fabsf(localForward);
#endif

            localRight *= scale;
            localForward *= scale;
        }

        rightOffset = game_compat_veh_clamp_float(localRight, -rightLimit, rightLimit);
        forwardOffset = game_compat_veh_clamp_float(localForward, -forwardLimit, forwardLimit);
        memset(&trace, 0, sizeof(trace_t));

        for (attempt = 0; attempt < VEH_DISMOUNT_DYNAMIC_RETRIES; attempt++) {
            vec3_t base;
            float xOffset;
            float yOffset;

            /* 0x892bc..0x893b6: outOrigin starts as the {0, 0, z-raise}
             * offset added to the origin per component, and the forward and
             * right terms are accumulated as separate stores. */
            outOrigin[0] = 0.0f;
            outOrigin[1] = 0.0f;
            outOrigin[2] = VEH_DISMOUNT_BASE_Z_RAISE;
            outOrigin[0] = vehicleState->origin[0] + outOrigin[0];
            outOrigin[1] = vehicleState->origin[1] + outOrigin[1];
            outOrigin[2] = vehicleState->origin[2] + outOrigin[2];
            /* outOrigin += forward*forwardOffset then += right*rightOffset:
             * mul then add-into, 80-bit, one store per accumulate -> shim. */
#if EMULATE_X87
            for (int i = 0; i < 3; i++) {
                outOrigin[i] = x87f_store_f32(
                    x87f_add(x87f_load_f32(outOrigin[i]), x87f_mul(x87f_load_f32(forward[i]), x87f_load_f32(forwardOffset))));
            }
            for (int i = 0; i < 3; i++) {
                outOrigin[i] =
                    x87f_store_f32(x87f_add(x87f_load_f32(outOrigin[i]), x87f_mul(x87f_load_f32(right[i]), x87f_load_f32(rightOffset))));
            }
#else
            outOrigin[0] += forward[0] * forwardOffset;
            outOrigin[1] += forward[1] * forwardOffset;
            outOrigin[2] += forward[2] * forwardOffset;
            outOrigin[0] += right[0] * rightOffset;
            outOrigin[1] += right[1] * rightOffset;
            outOrigin[2] += right[2] * rightOffset;
#endif

            base[0] = outOrigin[0];
            base[1] = outOrigin[1];
            base[2] = outOrigin[2];

            for (xOffset = VEH_DISMOUNT_SEARCH_MIN; xOffset < VEH_DISMOUNT_SEARCH_MAX; xOffset += VEH_DISMOUNT_SEARCH_STEP) {
                for (yOffset = VEH_DISMOUNT_SEARCH_MIN; yOffset < VEH_DISMOUNT_SEARCH_MAX; yOffset += VEH_DISMOUNT_SEARCH_STEP) {
                    for (int zAttempt = 0; zAttempt < 2; zAttempt++) {
                        trap_TraceCapsule(&trace, outOrigin, mins, maxs, outOrigin, passEntityNum, VEH_DISMOUNT_TRACE_MASK_CLEAR);
                        if (trace.fraction == 1.0f && trace.startsolid == 0) {
                            vec3_t ground = {outOrigin[0], outOrigin[1], outOrigin[2] - VEH_DISMOUNT_GROUND_DROP};

                            trap_TraceCapsule(&trace, outOrigin, mins, maxs, ground, ENTITYNUM_NONE, VEH_DISMOUNT_TRACE_MASK_GROUND);
                            if (trace.startsolid == 0 && trace.allsolid == 0 && trace.fraction < 1.0f) {
                                gentity_t *passenger = &g_entities[passEntityNum];
                                gclient_t *client = passenger->client;
                                vec3_t losMins;
                                vec3_t losMaxs;
                                vec3_t losStart;
                                vec3_t losEnd;

                                losMins[0] = client->ps.playerMins[0];
                                losMins[1] = client->ps.playerMins[1];
                                losMins[2] = client->ps.playerMins[2];
                                losMaxs[0] = client->ps.playerMaxs[0];
                                losMaxs[1] = client->ps.playerMaxs[1];
                                losMaxs[2] = client->ps.playerMaxs[2] - VEH_DISMOUNT_LOS_Z_RAISE;
                                losEnd[0] = outOrigin[0];
                                losEnd[1] = outOrigin[1];
                                losEnd[2] = outOrigin[2] + VEH_DISMOUNT_LOS_Z_RAISE;
                                losStart[0] = vehicleState->origin[0];
                                losStart[1] = vehicleState->origin[1];
                                losStart[2] = vehicleState->origin[2] + VEH_DISMOUNT_LOS_Z_RAISE;

                                trap_Trace(&trace, losStart, losMins, losMaxs, losEnd, passenger->passEntityNum,
                                           VEH_DISMOUNT_TRACE_MASK_GROUND);
                                if (trace.startsolid == 0 &&
                                    (trace.fraction == 1.0f || trace.entityNum == (uint16_t)vehicleEnt->s.number)) {
                                    return qtrue;
                                }
                            }
                        }

                        outOrigin[2] += VEH_DISMOUNT_Z_RETRY_STEP;
                    }

                    outOrigin[0] = base[0] + xOffset;
                    outOrigin[1] = base[1] + yOffset;
                    outOrigin[2] = base[2];
                }
            }

            if ((attempt & 1u) == 0u) {
                forwardOffset = -forwardOffset;
            } else {
                rightOffset = -rightOffset;
            }
        }

        return qfalse;
    }

    if (vehicleState->detachTagIndex < 0) {
        Com_Printf("VEH_UnlinkPlayer: Warning - no [tag_detach] on vehicle\n");
        outOrigin[0] = vehicleEnt->currentOrigin[0];
        outOrigin[1] = vehicleEnt->currentOrigin[1];
        /* 0x8984b..0x8985d: grouped as (collisionMaxs[2] + pad) + origin. */
        outOrigin[2] = vehicleInfo->collisionMaxs[2] + VEH_DISMOUNT_FALLBACK_Z_PAD + vehicleEnt->currentOrigin[2];
    } else {
        DObjSkelMat detachMatrix;

        G_DObjGetWorldBoneIndexMatrix(vehicleEnt, vehicleState->detachTagIndex, &detachMatrix);
        outOrigin[0] = detachMatrix.origin[0];
        outOrigin[1] = detachMatrix.origin[1];
        outOrigin[2] = detachMatrix.origin[2];
    }

    trap_TraceCapsule(&trace, outOrigin, mins, maxs, outOrigin, passEntityNum, VEH_DISMOUNT_TRACE_MASK_DETACH);
    for (int retry = 1; (trace.fraction < 1.0f || trace.startsolid != 0) && retry < VEH_DISMOUNT_FALLBACK_RETRIES; retry++) {
        outOrigin[2] += VEH_DISMOUNT_FALLBACK_STEP;
        trap_TraceCapsule(&trace, outOrigin, mins, maxs, outOrigin, passEntityNum, VEH_DISMOUNT_TRACE_MASK_DETACH);
    }

    /* 0x898ed..0x8997e returns success when the retry condition stopped
     * because fraction is not ordered-less-than one and startsolid is clear.
     * It does not require fraction to equal one. */
    return (!(trace.fraction < 1.0f) && trace.startsolid == 0) ? qtrue : qfalse;
}

/* ------------------------------------------------------------------ */
/*  0x8b893  G_IsVehicleImmune                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b893, 9b893_G_IsVehicleImmune.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
qboolean G_IsVehicleImmune(gentity_t *vehicle, int meansOfDeath)
{
    const vehicle_state_t *vehicleState = (const vehicle_state_t *)vehicle->vehicle;
    const vehicleInfo_t *vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    switch (meansOfDeath) {
    case MOD_PISTOL_BULLET:
    case MOD_RIFLE_BULLET:
        if (g_vehicleForceBulletDamage.integer != 0) {
            return qfalse;
        }
        return vehicleInfo->bulletDamageEnabled == 0;

    case MOD_GRENADE:
    case MOD_GRENADE_SPLASH:
        if (g_vehicleForceGrenadeDamage.integer != 0) {
            return qfalse;
        }
        return vehicleInfo->grenadeDamageEnabled == 0;

    case MOD_PROJECTILE:
    case MOD_PROJECTILE_SPLASH:
    case MOD_ARTILLERY:
    case MOD_ARTILLERY_SPLASH:
        return vehicleInfo->explosiveDamageEnabled == 0;

    case MOD_WATER:
    case MOD_EXPLOSIVE:
    case MOD_COLLISION:
        return qfalse;

    default:
        return qtrue;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_player_is_linked_vehicle_occupant(const gentity_t *player)
{
    if (player->client == NULL) {
        return qfalse;
    }

    if ((player->client->ps.entityStateFlags & EF_IN_VEHICLE) == 0 || (player->client->ps.entityStateFlags & EF_VEHICLE_POPOUT) != 0) {
        return qfalse;
    }

    if (player->passEntityNum == ENTITYNUM_NONE) {
        return qfalse;
    }

    return g_entities[player->passEntityNum].vehicle != NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static const vehicle_state_t *game_compat_veh_player_linked_vehicle_state(const gentity_t *player)
{
    return (const vehicle_state_t *)g_entities[player->passEntityNum].vehicle;
}

/* ------------------------------------------------------------------ */
/*  0x8b96b  G_IsVehicleOccupantInvulnerable                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8b96b, 9b96b_G_IsVehicleOccupantInvulnerable.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int G_IsVehicleOccupantInvulnerable(gentity_t *player)
{
    const vehicle_state_t *vehicleState;
    const vehicleInfo_t *vehicleInfo;

    if (!game_compat_veh_player_is_linked_vehicle_occupant(player)) {
        return qfalse;
    }

    vehicleState = game_compat_veh_player_linked_vehicle_state(player);
    vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    return vehicleInfo->type == VEHICLE_TYPE_TANK && player->client->ps.vehiclePosition == VEH_PASSENGER_SLOT_DRIVER;
}

/* ------------------------------------------------------------------ */
/*  0x8ba86  G_VehicleOccupantRadiusDamageScale                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8ba86, 9ba86_G_VehicleOccupantRadiusDamageScale.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
float G_VehicleOccupantRadiusDamageScale(gentity_t *player)
{
    const vehicle_state_t *vehicleState;
    const vehicleInfo_t *vehicleInfo;

    if (!game_compat_veh_player_is_linked_vehicle_occupant(player)) {
        return 1.0f;
    }

    vehicleState = game_compat_veh_player_linked_vehicle_state(player);
    vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);

    if (vehicleInfo->type == VEHICLE_TYPE_ARTILLERY) {
        return 0.2f;
    }

    if (vehicleInfo->type == VEHICLE_TYPE_4_WHEEL) {
        return 0.1f;
    }

    return 0.1f;
}

/* ------------------------------------------------------------------ */
/*  0x8bbb0  G_VehiclePopOut                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8bbb0, 9bbb0_G_VehiclePopOut.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
void G_VehiclePopOut(gentity_t *player)
{
    gclient_t *client = player->client;
    gentity_t *vehicleEnt = NULL;
    vehicle_state_t *vehicleState;
    const char *tagName;
    int boneIndex;
    DObjSkelMat boneMatrix;

    Com_Error(1, VEH_POPOUT_DISABLED_ERROR);

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) == 0 || player->passEntityNum == ENTITYNUM_NONE) {
        return;
    }

    vehicleEnt = &g_entities[player->passEntityNum];
    if (vehicleEnt->passEntityNum == ENTITYNUM_NONE || (vehicleEnt->scriptContents & VEH_USABLE_SCRIPT_CONTENTS) != 0) {
        return;
    }

    vehicleState = (vehicle_state_t *)vehicleEnt->vehicle;
    if ((client->ps.entityStateFlags & EF_VEHICLE_POPOUT) == 0) {
        client->ps.entityStateFlags |= EF_VEHICLE_POPOUT;
        boneIndex = vehicleState->popoutTagIndex;
        tagName = VEH_TAG_POPOUT;
    } else {
        client->ps.entityStateFlags &= ~EF_VEHICLE_POPOUT;
        boneIndex = vehicleState->driverTagIndex;
        tagName = VEH_TAG_PLAYER;
    }

    if (boneIndex < 0) {
        Com_Error(1, VEH_POPOUT_BONE_ERROR, tagName);
    }

    G_EntUnlink(player);
    G_DObjGetWorldBoneIndexMatrix(vehicleEnt, boneIndex, &boneMatrix);
    SetClientOrigin(player, boneMatrix.origin);

    if (!G_EntLinkToWithOffset(player, vehicleEnt, tagName, vec3_origin, vec3_origin)) {
        Com_Error(1, VEH_POPOUT_LINK_ERROR, tagName);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_tank_state_is_active(const vehicle_state_t *vehicleState)
{
    const vehicleInfo_t *vehicleInfo;

    if (vehicleState->entityNum == ENTITYNUM_NONE) {
        return qfalse;
    }

    vehicleInfo = game_compat_veh_get_vehicle_info(vehicleState->typeIndex);
    return vehicleInfo->type == VEHICLE_TYPE_TANK && game_compat_veh_float_slot_int_bits(&vehicleState->turretRoll) != 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static qboolean game_compat_veh_tank_entity_is_valid(const gentity_t *ent)
{
    return ent->vehicle != NULL && ent->health >= 1 && ent->passEntityNum == ENTITYNUM_NONE;
}

/* ------------------------------------------------------------------ */
/*  0x8bd83  G_GetTankIndex                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8bd83, 9bd83_G_GetTankIndex.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int G_GetTankIndex(int entNum)
{
    gentity_t *ent = &g_entities[entNum];
    vehicle_state_t *vehicleState;

    if (!game_compat_veh_tank_entity_is_valid(ent)) {
        return -1;
    }

    vehicleState = (vehicle_state_t *)ent->vehicle;
    if (!game_compat_veh_tank_state_is_active(vehicleState)) {
        return -1;
    }

    return (int)(vehicleState - level.vehicleStateBase);
}

/* ------------------------------------------------------------------ */
/*  0x8be7e  G_GetTankEntNum                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x8be7e, 9be7e_G_GetTankEntNum.c, VERIFY-VEHICLE-PATH-TAIL-2026-06-17): DATAFLOW_VERIFIED */
int G_GetTankEntNum(int tankIndex)
{
    const vehicle_state_t *vehicleState = &level.vehicleStateBase[tankIndex];
    gentity_t *ent;

    if (!game_compat_veh_tank_state_is_active(vehicleState)) {
        return ENTITYNUM_NONE;
    }

    ent = &g_entities[vehicleState->entityNum];
    if (!game_compat_veh_tank_entity_is_valid(ent)) {
        return ENTITYNUM_NONE;
    }

    return ent->s.number;
}
