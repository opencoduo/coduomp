
#ifndef RECOVERED_GAME_H
#define RECOVERED_GAME_H

#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include "qcommon/asset_type_names.h"
#include "bg/bg_animation.h"
#include "qcommon/bg_animation_types.h"
#include "qcommon/bg_item_types.h"
#include "bg/bg_pmove.h"
#include "bg/bg_vehicle.h"
#include "bg/bg_weapon.h"
#include "qcommon/client_info_types.h"
#include "qcommon/client_state_types.h"
#include "compat/coduo_int32_bits.h"
#include "compat/crt/random_compat.h"
#include "qcommon/dobj_types.h"
#include "qcommon/entity_event_types.h"
#include "qcommon/entity_state_types.h"
#include "qcommon/filesystem_types.h"
#include "qcommon/fx_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/pmove_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_limits.h"
#include "qcommon/q_vector_types.h"
#include "qcommon/script_types.h"
#include "qcommon/server_types.h"
#include "qcommon/sound_types.h"
#include "qcommon/trajectory_types.h"
#include "qcommon/vehicle_types.h"
#include "qcommon/weapon_types.h"
#include "qcommon/xanim_types.h"
#include "math/q_math.h"

/* NOT_FROM_ORIGINAL_SOURCE: defined host conversion matching an original x87
 * fistp under truncate mode, without introducing an emulated-x87 path. */
static inline int32_t game_compat_int32_from_long_double_trunc(long double value)
{
    if (!(value >= -2147483648.0L && value < 2147483648.0L)) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

/* NOT_FROM_ORIGINAL_SOURCE: float-input convenience form of the defined
 * truncating conversion above. */
static inline int32_t game_compat_int32_from_float_trunc(float value)
{
    return game_compat_int32_from_long_double_trunc((long double)value);
}

#define COM_ERROR_MARKER "\x15"
#define BG_ITEMLIST_SLOT_COUNT 135u /* logical table element count, not byte count */
#define GAME_COMPLAINT_WINDOW_MSEC 20500
/* Collision syscalls use signed -1 for "do not exclude an entity"; this is a
 * different domain from the 10-bit ENTITYNUM_NONE network sentinel. */
#define PASS_ENTITY_NONE (-1)
/* SV_GameSendServerCommand maps signed -1 to the broadcast NULL-client path. */
#define SERVER_COMMAND_ALL_CLIENTS (-1)

/* Game entity flags (`gentity_t.flags`). */
#define FL_NOCLIENT 0x00001000u
#define FL_SUPPORTS_LINKTO 0x00002000u

/* Recovered contents bits and composite query masks used by script queries
 * and traces. */
#define CONTENTS_TRIGGER_TOUCH_VEHICLE 0x00000008u
#define CONTENTS_TELEFRAG_BLOCKER 0x00000080u
#define CONTENTS_TRIGGER_DAMAGE 0x00400000u
#define CONTENTS_STUCK_ALT 0x04000000u
#define CONTENTS_TRIGGER_TOUCH_CLIENT 0x40000000u
#define MASK_GRENADE_TRACE 0x00000011u
#define MASK_BULLETTRACE 0x02802031u
#define MASK_MOVER_PUSH 0x02000180u
#define MASK_TRIGGER 0x405c0008u
#define MASK_CLEAR_VEHICLE_POSITION 0x04000100u

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;
typedef struct turret_state_s turret_state_t;
typedef struct vehicle_state_s vehicle_state_t;
typedef struct vehicleInfo_s vehicleInfo_t;

typedef void (*gentity_touch_t)(gentity_t *self, gentity_t *other, int traceMode);
typedef void (*gentity_use_t)(gentity_t *self, gentity_t *other, gentity_t *activator);
typedef void (*gentity_mover_reached_t)(gentity_t *self);
typedef void (*gentity_mover_blocked_t)(gentity_t *self, gentity_t *blocked);
typedef void (*gentity_controller_t)(gentity_t *self, uint32_t *partBits);
typedef void (*gentity_pain_t)(gentity_t *self, gentity_t *attacker, int damage, const float *point, int meansOfDeath,
                               const float *direction, int hitLocation);
typedef void (*gentity_die_t)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath, int weapon,
                              const float *direction, int hitLocation);
typedef enum {
    SESS_STATE_PLAYING = 0,
    SESS_STATE_DEAD = 1,
    SESS_STATE_SPECTATOR = 2,
    SESS_STATE_INTERMISSION = 3
} sessionState_t;

typedef enum {
    SESS_SQUAD_NONE = 0,
    SESS_SQUAD_ALPHA = 1,
    SESS_SQUAD_BRAVO = 2
} sessionSquad_t;

typedef enum {
    SCRIPT_CLIENT_NAME_MODE_AUTO = 0,
    SCRIPT_CLIENT_NAME_MODE_MANUAL = 1
} scriptClientNameMode_t;

typedef enum {
    CON_DISCONNECTED = 0,
    CON_CONNECTING = 1,
    CON_CONNECTED = 2
} clientConnectedState_t;

/*
 * Means of Death (MOD) enumeration.
 * RECOVERED(UO-GAME-UNK-0173): Values observed in G_Damage, G_RadiusDamage,
 * VEH_PlayerDamage, and other damage-related functions.
 */
typedef enum {
    MOD_UNKNOWN = 0,
    MOD_PISTOL_BULLET = 1,
    MOD_RIFLE_BULLET = 2,
    MOD_GRENADE = 3,
    MOD_GRENADE_SPLASH = 4,
    MOD_PROJECTILE = 5,
    MOD_PROJECTILE_SPLASH = 6,
    MOD_MELEE = 7,
    MOD_HEAD_SHOT = 8,
    MOD_MORTAR = 9,
    MOD_MORTAR_SPLASH = 10,
    MOD_DYNAMITE = 11,
    MOD_DYNAMITE_SPLASH = 12,
    MOD_ARTILLERY = 13,
    MOD_ARTILLERY_SPLASH = 14,
    MOD_WATER = 15,
    MOD_CRUSH = 16,
    MOD_CRUSH_TANK = 17,
    MOD_CRUSH_JEEP = 18,
    MOD_TELEFRAG = 19,
    MOD_FALLING = 20,
    MOD_SUICIDE = 21,
    MOD_TRIGGER_HURT = 22,
    MOD_EXPLOSIVE = 23,
    MOD_COLLISION = 24,
    MOD_FLAME = 25,
    MOD_MELEE_BINOCULARS = 26,
    MOD_COUNT = 27,
    MOD_VEHICLE_COLLISION_TYPE1 = MOD_CRUSH_TANK,
    MOD_VEHICLE_COLLISION_TYPE2 = MOD_CRUSH_JEEP,
    MOD_RADIUS_DAMAGE = MOD_EXPLOSIVE,
    MOD_VEHICLE_CRUSH = MOD_SUICIDE
} meansOfDeath_t;

/* Damage flags */
typedef enum damageFlags_e {
    DAMAGE_RADIUS = 1 << 0,          /* Damage from radius explosion */
    DAMAGE_NO_KNOCKBACK = 1 << 1,    /* No knockback applied */
    DAMAGE_HALF_KNOCKBACK = 1 << 2,  /* Reduced knockback */
    DAMAGE_NO_PROTECTION = 1 << 4    /* Bypass protection for forced kills */
} damageFlags_t;

typedef enum hitLocation_e {
    HITLOC_NONE = 0,
    HITLOC_HELMET,
    HITLOC_HEAD,
    HITLOC_NECK,
    HITLOC_TORSO_UPPER,
    HITLOC_TORSO_LOWER,
    HITLOC_RIGHT_ARM_UPPER,
    HITLOC_LEFT_ARM_UPPER,
    HITLOC_RIGHT_ARM_LOWER,
    HITLOC_LEFT_ARM_LOWER,
    HITLOC_RIGHT_HAND,
    HITLOC_LEFT_HAND,
    HITLOC_RIGHT_LEG_UPPER,
    HITLOC_LEFT_LEG_UPPER,
    HITLOC_RIGHT_LEG_LOWER,
    HITLOC_LEFT_LEG_LOWER,
    HITLOC_RIGHT_FOOT,
    HITLOC_LEFT_FOOT,
    HITLOC_GUN,
    HITLOC_COUNT
} hitLocation_t;

typedef struct weapon_muzzle_s {
    vec3_t forward;                  /* +0x00 */
    vec3_t right;                    /* +0x0c */
    vec3_t up;                       /* +0x18 */
    vec3_t origin;                   /* +0x24 */
    /* The turret producer seeds this packet lane with a copy of forward. */
    vec3_t extraVector;              /* +0x30 */
    const weaponInfo_t *weaponInfo;    /* +0x3c */
} weapon_muzzle_t;

/* The scripted-input path passes the complete +0x2a4/+0x2a8 pair directly to
 * VectorNormalize2D (game.mp.uo.i386.so 0x0007d5de..0x0007d5e6).  Preserve
 * both its vector identity and the individually named control components. */
typedef union vehicle_local_accel_u {
    vec2_t vector;
    struct vehicle_local_accel_components_s {
        float forward;
        float vertical;
    } components;
} vehicle_local_accel_t;

/* ---------------------------------------------------------------------------
 * HUD element structures.
 *
 * The first 0x7c bytes are copied verbatim into the current/archived client HUD
 * snapshots. The final three dwords are server-side routing/state fields.
 * --------------------------------------------------------------------------- */

typedef struct game_hudElem_s {
    hudElem_t client;                              /* +0x000..+0x07b */
    /* Server-side fields (not included in client snapshot) */
    int32_t clientNum;                             /* +0x07c */
    int32_t team;                                  /* +0x080 */
    int32_t archived;                              /* +0x084 */
} game_hudElem_t;                                  /* ABI total size: 0x88 bytes */

typedef struct vehicle_path_node_s {
    uint16_t targetname;                 /* +0x00 */
    uint16_t target;                     /* +0x02 */
    int16_t scriptObjectId;              /* +0x04 */
    uint16_t padding06;                  /* +0x06..+0x07, aligns useNodeAngles. */
    qboolean useNodeAngles; /* +0x08, info_vehicle_node_rotate flag; binary stores 0/1 and consumers only test nonzero. */
    float speed; /* +0x0c */
    float lookAhead; /* +0x10 */
    vec3_t origin; /* +0x14 */
    vec3_t direction; /* +0x20 */
    vec3_t angles; /* +0x2c */
    float segmentLength; /* +0x38 */
    int16_t nextNodeIndex; /* +0x3c */
    int16_t previousNodeIndex; /* +0x3e */
} vehicle_path_node_t;

typedef struct vehicle_path_position_s {
    int16_t nodeIndex; /* +0x00 */
    int16_t reachedEnd; /* +0x02 */
    float fraction; /* +0x04 */
    float speed; /* +0x08 */
    float lookAhead; /* +0x0c */
    float curveFraction; /* +0x10 */
    vec3_t origin; /* +0x14 */
    vec3_t currentAngles; /* +0x20 */
    vec3_t lookAheadOrigin; /* +0x2c */
    vehicle_path_node_t targetNode; /* +0x38 */
    vehicle_path_node_t cachedNode; /* +0x78 */
} vehicle_path_position_t;

/* Stock VEH_Backup copies this exact 0xb8-byte lane from vehicle_state_t+0xb8,
 * and the collision rollback path keeps a second copy at +0x170. Individual
 * physics consumers prove every member through +0xb7. */
typedef struct vehicle_physics_state_s {
    vec3_t origin; /* +0x00 */
    vec3_t previousOrigin; /* +0x0c */
    vec3_t viewClampTargetAngles; /* +0x18 */
    vec3_t previousAngles; /* +0x24 */
    vec3_t velocity; /* +0x30 */
    vec3_t angularVelocity; /* +0x3c */
    vec3_t acceleration; /* +0x48 */
    float steerAngle; /* +0x54 */
    vec3_t angularAcceleration; /* +0x58 */
    vec3_t externalVelocity; /* +0x64 */
    float wheelVerticalVelocity[6]; /* +0x70 */
    float wheelGroundZ[6]; /* +0x88 */
    float wheelMaterial[6]; /* +0xa0 */
} vehicle_physics_state_t;

struct vehicle_state_s {
    /* Remaining gap ranges were checked in UO-GAME-TASK-0345 against
     * vehicle.c offset helpers and left only where no maintained access exists. */
    vehicle_path_position_t pathPosition; /* +0x000, vehicle path-position cursor */
    vec3_t origin; /* +0x0b8 */
    vec3_t previousOrigin; /* +0x0c4, initialized from entity origin */
    vec3_t viewClampTargetAngles; /* +0x0d0, vehicle angles copied into pmove clamp target */
    vec3_t previousAngles; /* +0x0dc, initialized from entity angles */
    vec3_t velocity; /* +0x0e8 */
    vec3_t angularVelocity; /* +0x0f4 */
    vec3_t acceleration; /* +0x100 */
    float steerAngle; /* +0x10c, 4-wheel steering overlay */
    vec3_t angularAcceleration; /* +0x110, includes roll/steer velocity overlays */
    vec3_t externalVelocity; /* +0x11c, includes roll-angle steering overlays */
    float wheelVerticalVelocity[6]; /* +0x128, cleared by suspension solve */
    float wheelGroundZ[6]; /* +0x140, wheel trace/support height */
    float wheelMaterial[6]; /* +0x158, decoded surface material id / ground flags */
    vehicle_physics_state_t previousPhysicsState; /* +0x170, copy of +0x0b8..+0x16f */
    int32_t entityNum; /* +0x228, owning vehicle entity number */
    int16_t typeIndex; /* +0x22c */
    char padding22e[2]; /* +0x22e..+0x22f, aligns slotIndex. */
    int32_t slotIndex; /* +0x230, script vehicle slot; VEH_UpdateClient reads the full word */
    int32_t hintStringIndex; /* +0x234, G_GetHintStringIndex output */
    int16_t pathNodeIndex; /* +0x238, initialized to -1 */
    char padding23a[2]; /* +0x23a..+0x23b, aligns waitNodeSpeedThreshold. */
    float waitNodeSpeedThreshold; /* +0x23c, wait-node speed threshold */
    int32_t primaryFireTime; /* +0x240, dword countdown copied from weaponInfo->fireTime */
    int32_t altFireTime; /* +0x244, dword countdown copied from weaponInfo->fireTime */
    int32_t primaryFlashSelector; /* +0x248, primary turret muzzle flash selector */
    float turretYaw; /* +0x24c, also primary turret activity state */
    float turretRoll; /* +0x250, also tank/compass active flag */
    int32_t gunnerTurretState; /* +0x254 */
    int32_t gunnerFireTime; /* +0x258, dword countdown copied from weaponInfo->fireTime */
    float altHeat; /* +0x25c */
    float gunnerHeat; /* +0x260 */
    int32_t altOverheating; /* +0x264 */
    int32_t gunnerOverheating; /* +0x268 */
    float viewState[7]; /* +0x26c */
    int32_t gunnerEntityNum; /* +0x288, initialized to ENTITYNUM_NONE */
    float motionControl[6]; /* +0x28c */
    vehicle_local_accel_t localAccel; /* +0x2a4 */
    float throttleScalePrevious; /* +0x2ac */
    float throttleScale; /* +0x2b0 */
    float brakeScale; /* +0x2b4 */
    float scriptedMaxSpeed; /* +0x2b8 */
    float scriptedAcceleration; /* +0x2bc */
    int32_t suspensionEnabled; /* +0x2c0 */
    int32_t soundBlendEntityNums[4]; /* +0x2c4 */
    float idleSoundBlendRepeatDelay; /* +0x2d4, G_SetSoundBlend repeat delay payload */
    float runSoundBlendRepeatDelay; /* +0x2d8, G_SetSoundBlend repeat delay payload */
    int32_t altWeaponFireSound; /* +0x2dc, zero-extended sustained-fire sound alias word */
    int32_t altWeaponStopSound; /* +0x2e0, zero-extended sustained-fire stop sound alias word */
    int32_t altWeaponSoundTime; /* +0x2e4 */
    int32_t gunnerWeaponFireSound; /* +0x2e8, zero-extended sustained-fire sound alias word */
    int32_t gunnerWeaponStopSound; /* +0x2ec, zero-extended sustained-fire stop sound alias word */
    int32_t gunnerWeaponSoundTime; /* +0x2f0 */
    int32_t driverTagIndex; /* +0x2f4 */
    int32_t detachTagIndex; /* +0x2f8 */
    int32_t popoutTagIndex; /* +0x2fc */
    int32_t bodyTagIndex; /* +0x300 */
    int32_t primaryBaseTagIndex; /* +0x304 */
    int32_t primaryTurretTagIndex; /* +0x308 */
    int32_t primaryAltTurretTagIndex; /* +0x30c */
    int32_t gunnerTagIndex; /* +0x310 */
    int32_t gunnerTurretTagIndex; /* +0x314 */
    int32_t secondaryBaseTagIndex; /* +0x318 */
    int32_t chasecamTagIndex; /* +0x31c */
    int32_t aimDownBarrelTagIndex; /* +0x320 */
    int32_t primaryFlashTagIndices[4]; /* +0x324 */
    int32_t altFireTagIndices[4]; /* +0x334 */
    int32_t secondaryFlashTagIndices[4]; /* +0x344 */
    int32_t wheelTagIndices[6]; /* +0x354 */
    int32_t passengerTagIndices[4]; /* +0x36c, tag_passenger..tag_passenger4 */
    int32_t primaryAimSightTraceResult; /* +0x37c, trap_SightTrace result scratch for VEH_UpdateAim turret_on_vistarget path */
    int32_t animLeftSource; /* +0x380 */
    int32_t animRightSource; /* +0x384 */
    int32_t animLeftTime; /* +0x388 */
    int32_t animRightTime; /* +0x38c */
    int32_t scriptedDriverEndTime; /* +0x390 */
    int32_t animLeftTarget; /* +0x394 */
    int32_t animRightTarget; /* +0x398 */
    int32_t passengerEntityNums[7]; /* +0x39c, initialized to ENTITYNUM_NONE */
    float collisionSweepFraction; /* +0x3b8, cached collision sweep fraction */
    int32_t scriptedInputEndTime; /* +0x3bc */
    int32_t lastInputTime; /* +0x3c0 */
    float wheelBaseLength; /* +0x3c4 */
    int32_t collisionNotifyTime; /* +0x3c8 */
    int32_t collisionSoundTime; /* +0x3cc */
    int32_t cachedCollisionEntityNum; /* +0x3d0, cached collision trace entity */
    float cachedCollisionDistance; /* +0x3d4, cached collision trace distance */
    int32_t lastStableTime; /* +0x3d8 */
    int32_t lastSolidTime; /* +0x3dc */
    vec3_t collisionNormal; /* +0x3e0 */
};

struct vehicleInfo_s {
    char name[0x040]; /* +0x000, compared by vehicle info lookup */
    int16_t type; /* +0x040, VEH_ParseSpecificField writes vehicle type */
    char padding042[2]; /* +0x042..+0x043, aligns steerWheels. */
    int32_t steerWheels; /* +0x044, vehicle key "steerWheels" */
    int32_t textureScroll; /* +0x048, vehicle key "texureScroll" */
    int32_t primaryDualFlash; /* +0x04c, vehicle key "quadBarrel" */
    int32_t bulletDamageEnabled; /* +0x050 */
    int32_t grenadeDamageEnabled; /* +0x054 */
    int32_t explosiveDamageEnabled; /* +0x058 */
    int32_t gunnerSeatEnabled; /* +0x05c */
    int32_t extraPassengerCount; /* +0x060 */
    float textureScrollScale; /* +0x064, vehicle key "texureScrollScale" */
    float maxSpeed; /* +0x068, physics speed cap */
    float acceleration; /* +0x06c, local acceleration clamp */
    float steeringLimit; /* +0x070, angular clamp */
    float steeringRate; /* +0x074, angular acceleration clamp */
    float forwardInputScale; /* +0x078, vehicle input acceleration scale */
    float verticalInputScale; /* +0x07c, vehicle input acceleration scale */
    float collisionDamageScale; /* +0x080 */
    float pathSpeed; /* +0x084, scaled by vehicle loader */
    float suspensionTravel; /* +0x088, vehicle key "suspensionTravel"; ground probe depth */
    char turretWeapon[0x040]; /* +0x08c, vehicle spawn primary weapon */
    char turretAltWeapon[0x040]; /* +0x0cc, vehicle spawn alt-turret weapon */
    float primaryYawLimitPos; /* +0x10c */
    float primaryYawLimitNeg; /* +0x110 */
    float primaryPitchLimitNeg; /* +0x114 */
    float primaryPitchLimitPos; /* +0x118 */
    float primaryTurnRate; /* +0x11c */
    char turretGunnerWeapon[0x040]; /* +0x120, vehicle spawn gunner weapon */
    float gunnerYawLimitPos; /* +0x160 */
    float gunnerYawLimitNeg; /* +0x164 */
    float gunnerPitchLimitNeg; /* +0x168 */
    float gunnerPitchLimitPos; /* +0x16c */
    float gunnerTurnRate; /* +0x170 */
    char soundNames[7][0x040]; /* +0x174, parsed sound alias names */
    uint8_t idleBlendSound0; /* +0x334 */
    uint8_t idleBlendSound1; /* +0x335 */
    uint8_t runBlendSound0; /* +0x336 */
    uint8_t runBlendSound1; /* +0x337 */
    uint8_t primaryActiveSound; /* +0x338 */
    uint8_t primaryStopSound; /* +0x339 */
    uint8_t collisionSound; /* +0x33a */
    uint8_t hitPersonSound; /* +0x33b */
    float pathSpeedDenom; /* +0x33c */
    float damageScaleFront; /* +0x340 */
    float damageScaleSide; /* +0x344 */
    float damageScaleRear; /* +0x348 */
    float damageScaleTop; /* +0x34c */
    float damageScaleBullet; /* +0x350 */
    vec3_t collisionMins; /* +0x354, movement trace bounds */
    vec3_t collisionMaxs; /* +0x360, movement trace bounds */
    float collisionBoundsSource[6]; /* +0x36c, parser bounds before half-scale */
    float rollInputScale; /* +0x384 */
    float steeringRollScale; /* +0x388 */
    float rollLimit; /* +0x38c */
    float stepSize; /* +0x390, vehicle key "stepsize"; step trace depth */
    float dismountForwardOffset; /* +0x394 */
    float dismountBackOffset; /* +0x398 */
    char hintString[0x040]; /* +0x39c */
};

typedef struct entityLinkInfo_s {
    gentity_t *parent; /* +0x000 */
    gentity_t *nextChild; /* +0x004 */
    uint16_t tagStringId;
    uint16_t padding0a; /* +0x00a..+0x00b, aligns parentTagIndex. */
    int32_t parentTagIndex;
    matrix43_t relAxis;
    matrix43_t parentRelAxis;
    /* RECOVERED(UO-GAME-UNK-0002): record size is 0x70; link helper fields are now modeled. */
} entityLinkInfo_t;

#define MAX_TURRETS 32
struct turret_state_s {
    int32_t inUse; /* +0x000, fixed turret-state pool slot flag */
    uint32_t flags; /* +0x004, client-state refresh flags */
    int32_t fireTimeRemaining; /* +0x008, client turret shot countdown */
    float topArc; /* +0x00c, RECOVERED(UO-GAME-UNK-0101):
                                            script settoparc writes negative clamp */
    float rightArc; /* +0x010, RECOVERED(UO-GAME-UNK-0101):
                                            script setrightarc writes negative clamp */
    float bottomArc; /* +0x014, RECOVERED(UO-GAME-UNK-0101):
                                            script setbottomarc writes positive clamp */
    float leftArc; /* +0x018, RECOVERED(UO-GAME-UNK-0101):
                                            script setleftarc writes positive clamp */
    float restPitch; /* +0x01c, turret return-to-rest pitch */
    int32_t useMode; /* +0x020, copied weapon-info stance/use pose */
    int32_t stopUseEventType; /* +0x024, stop-use player event selector */
    int32_t fireSoundTime; /* +0x028, sustained firing sound countdown */
    vec3_t stopUseOrigin; /* +0x02c, player return origin on turret exit */
    float restPitchClamp; /* +0x038, return-to-rest pitch clamp */
    uint8_t sustainedFireLoopSound; /* +0x03c, clientSound while fireSoundTime runs */
    uint8_t sustainedFireStopSound; /* +0x03d, one-shot sound when sustained fire ends */
    char padding3e[2]; /* +0x03e..+0x03f, aligns heat. */
    float heat; /* +0x040, RECOVERED(UO-GAME-UNK-0101):
                                            script getturretheat source */
    int32_t overheating; /* +0x044, RECOVERED(UO-GAME-UNK-0101):
                                            script getturretoverheating source */
};

/*
 * Client structure (gclient_t).
 * Fields are ordered by original binary offset and proved across
 * client_lifecycle.c, script_player.c, items.c, client_frame.c, animation.c,
 * and pmove.c.
 *
 * The original i386 binary stride per client is 0x4734 bytes.
 * Runtime access uses these typed fields; explicit byte access is retained
 * only where the original ABI genuinely exposes a packed or untyped record.
 *
 * The original source shape is preserved here: playerState_t ps is the
 * first member, and client-session fields follow it.
 * UO-GAME-TASK-0345 checked remaining pad ranges against maintained client and
 * vehicle accessors; newly observed vehicle overlays are named below.
 */
struct gclient_s {
    playerState_t ps; /* +0x000..+0x4503, original playerState_t ps prefix */

    /* === Session / scoring / configstring (offset range 0x4504-0x455f) === */
    sessionState_t sessionState; /* +0x4504, script-facing sessionstate field */
    int32_t followClient; /* +0x4508, RECOVERED(UO-GAME-UNK-0018):
                                                also spawnCount (UO-GAME-UNK-0187) */
    int32_t statusIcon; /* +0x450c */
    int32_t archiveTime; /* +0x4510, RECOVERED(UO-GAME-UNK-0018) */
    int32_t score; /* +0x4514 */
    int32_t deaths; /* +0x4518 */
    uint16_t pers; /* +0x451c */
    char padding451e[2]; /* +0x451e..+0x451f, aligns connectedState. */
    clientConnectedState_t connectedState; /* +0x4520 */
    usercmd_t command; /* +0x4524..+0x453b, current engine-supplied user command */
    usercmd_t oldPmoveCommand; /* +0x453c..+0x4550, previous pmove command */
    int32_t complaintDisabled; /* +0x4554, inactivity exemption;
                                                RECOVERED(UO-GAME-UNK-0024) */
    int32_t predictItems; /* +0x4558 */
    int32_t pmoveFixed; /* +0x455c, per-client pmove_fixed override used by ClientThink_real */

    /* === Name / handicap / speed (offset range 0x4560-0x459f) === */
    char cleanName[0x20]; /* +0x4560 */
    int32_t handicap; /* +0x4580 */
    int32_t normalMaxHealth; /* +0x4584 */
    int32_t maxSpeed; /* +0x4588 */
    int32_t enterTime; /* +0x458c */
    char
        abiGap_4590[0x004]; /* +0x4590, size 0x004; client scoring audit found no maintained access before calledVotes; retained for ABI. */
    int32_t calledVotes; /* +0x4594, Cmd_CallVote_f max-called gate */
    char abiGap_4598
        [0x004]; /* +0x4598, size 0x004; client complaint audit found no maintained access before complaintCount; retained for ABI. */
    int32_t complaintCount; /* +0x459c, RECOVERED(UO-GAME-UNK-0024) */

    /* === Complaint / viewmodel / team (offset range 0x45a0-0x4617) === */
    int32_t pendingComplaintClient; /* +0x45a0 */
    int32_t pendingComplaintTime; /* +0x45a4 */
    int32_t viewModelIndex; /* +0x45a8, full-width script model index copied
                                                to playerState_t::viewModelIndex;
                                                RESOLVED(UO-GAME-UNK-0097) */
    uint32_t spectateTeamDenyMask; /* +0x45ac, deny bits read by G_ClientCanSpectateTeam */
    char abiGap_45b0
        [0x004]; /* +0x45b0, size 0x004; client team/model audit found no maintained access before sessionSquad; retained for ABI. */
    int16_t sessionSquad; /* +0x45b4 */
    char padding45b6[2]; /* +0x45b6..+0x45b7, aligns clientNum. */
    int32_t clientNum; /* +0x45b8 */
    int32_t sessionTeam; /* +0x45bc, RECOVERED(UO-GAME-UNK-0024) */
    int32_t baseModelIndex; /* +0x45c0 */
    int32_t attachModelIndices[6]; /* +0x45c4 */
    int32_t attachTagIndices[6]; /* +0x45dc */
    char userInfoName[0x20]; /* +0x45f4 */

    /* === Spectator / buttons / damage (offset range 0x4614-0x469f) === */
    int32_t spectatorActivityState; /* +0x4614 */
    int32_t archiveClient; /* +0x4618 */
    qboolean noclip; /* +0x461c, RECOVERED(UO-GAME-UNK-0017) */
    qboolean ufo; /* +0x4620, RECOVERED(UO-GAME-UNK-0017) */
    qboolean controlsFrozen; /* +0x4624, RESOLVED(UO-GAME-UNK-0006) */
    int32_t lastUsercmdTime; /* +0x4628 */
    uint32_t currentButtons; /* +0x462c, spectator buttons;
                                                RECOVERED(UO-GAME-UNK-0094) */
    uint32_t oldButtons; /* +0x4630 */
    uint32_t latchedButtons; /* +0x4634, ~oldButtons & currentButtons */
    uint32_t spectatorWbuttons; /* +0x4638, spectator weapon buttons /
                                                flame-active flags overlay */
    uint32_t oldWbuttons; /* +0x463c */
    uint32_t latchedWbuttons; /* +0x4640, ~oldWbuttons & spectatorWbuttons */
    vec3_t spectatorSnapshotOrigin; /* +0x4644, ClientThink_real pre-pmove origin snapshot */
    float spectatorSnapshotAngle0; /* +0x4650 */
    float spectatorSnapshotAngle1; /* +0x4654 */
    int32_t damageTaken; /* +0x4658 */
    vec3_t damageFrom; /* +0x465c */
    int32_t damageFromWorld; /* +0x4668 */
    char abiGap_466c_4677
        [0x00c]; /* +0x466c..0x4677, size 0x00c; client damage/inactivity audit found no maintained access before inactivity timers; retained for ABI. */
    int32_t inactivityTime; /* +0x4678 */
    int32_t spectatorInactivityTime; /* +0x467c */
    int32_t inactivityWarningSent; /* +0x4680 */
    int32_t spectatorInactivityWarning; /* +0x4684 */
    char abiGap_4688
        [0x004]; /* +0x4688, size 0x004; client vehicle-control audit found no maintained access before vehicleControlTime; retained for ABI. */
    int32_t vehicleControlTime; /* +0x468c, vehicle link/use timing gate */
    int32_t vehicleDelayIgnoreTime; /* +0x4690, vehicle re-entry delay override */
    int32_t driverUnlinkRequested; /* +0x4694, driver requested vehicle exit */

    /* === Damage feedback floats (offset range 0x4698-0x46d4) === */
    float damageAlphaFraction; /* +0x4698 */
    char abiGap_469c_46a3
        [0x008]; /* +0x469c..0x46a3, size 0x008; client damage-feedback audit found no maintained access before endFrameTransient46a4; retained for ABI. */
    int32_t endFrameTransient46a4; /* +0x46a4 */
    char abiGap_46a8_46b7
        [0x010]; /* +0x46a8..0x46b7, size 0x010; neither game.mp.uo.i386.so nor uo_game_mp_x86.dll contains a gclient-relative access in this range; both resume at lookAtEntity +0x46b8. Quake III's gclient tail does not align here, so no inherited field names are justified. */
    gentity_t *lookAtEntity; /* +0x46b8, RESOLVED(UO-GAME-UNK-0009) */
    int32_t nonpvsFriendlyClient; /* +0x46bc */
    int32_t pingEndTime; /* +0x46c0, RECOVERED(UO-GAME-UNK-0096) */
    int32_t nonpvsTankClient; /* +0x46c4 */
    int32_t damageTime; /* +0x46c8 */
    float damageRoll; /* +0x46cc */
    float damagePitch; /* +0x46d0 */
    vec3_t weaponPreviousViewAngles; /* +0x46d4, BG_CalculateWeaponPosition_Sway input */
    vec3_t weaponSwayOffsets; /* +0x46e0, BG_CalculateWeaponPosition_Sway output */
    vec3_t weaponSwayAngles; /* +0x46ec, weapon angle base pitch/yaw plus spare */

    /* === Misc late fields (offset range 0x46f8-0x4788) === */
    vec3_t weaponMoveOffset; /* +0x46f8, BG_CalculateWeaponPosition_BasePosition_angles accumulator */
    float weaponIdleScale; /* +0x4704, BG_CalculateWeaponPosition_IdleAngles accumulator */
    vec3_t weaponRecoilAngles; /* +0x4708, BG_CalculateWeaponPosition_GunRecoil accumulator */
    float fireRecoilVelocity[2]; /* +0x4714, G_PlayerEvent recoil accumulator */
    int32_t weaponRecoilState; /* +0x471c, ClientThink_real/BG_CalculateWeaponAngles stack context word */
    int32_t flameDamageTime; /* +0x4720, RECOVERED(UO-GAME-UNK-0022) */
    int32_t flameDamageInflictor; /* +0x4724, RECOVERED(UO-GAME-UNK-0022) */
    int32_t lastCollisionDamageTime; /* +0x4728, vehicle collision damage cooldown */
    int32_t vehicleProneDamageTime; /* +0x472c, ClientThink_real prone vehicle damage cooldown gate */
    int32_t vehicleExitState; /* +0x4730, vehicle unlink/exit state */
};

/*
 * Partial gentity view for recovered fields.
 * Offsets are binary offsets, not a complete original source struct.
 * Fields are ordered by original binary offset and proved across entity
 * management, items, missiles, triggers, script methods, and vehicle code.
 * UO-GAME-TASK-0345 checked remaining pad ranges against maintained entity and
 * vehicle accessors; newly observed overlays are named below.
 *
 * Host builds use native pointer widths, so fields after native pointer slots
 * are not a reliable binary-offset view on 64-bit.  Use fixed-offset helpers
 * for byte-exact overlay access that must match the original i386 layout.
 */
struct gentity_s {
    entityState_t s; /* +0x000..+0x0f3 */
    int32_t linkedState; /* +0x0f4 */

    /* === Server flags / linked state (0x0f8-0x107) === */
    uint32_t svFlags; /* +0x0f8, server/link flags;
                                                RECOVERED(UO-GAME-UNK-0002) */
    int32_t tempClientNumFc; /* +0x0fc */
    int32_t soundTime; /* +0x100, RECOVERED(UO-GAME-UNK-0002) */
    int32_t contents; /* +0x104, RECOVERED(UO-GAME-UNK-0002) */

    /* === Bounds (offset range 0x108-0x13f) === */
    vec3_t mins; /* +0x108 */
    vec3_t maxs; /* +0x114 */
    int32_t scriptContents; /* +0x120, script-visible contents;
                                                RECOVERED(UO-GAME-UNK-0063) */
    vec3_t absMin; /* +0x124 */
    vec3_t absMax; /* +0x130, absolute bounds maximum */

    /* === Current origin / angles (offset range 0x13c-0x15f) === */
    vec3_t currentOrigin; /* +0x13c, RECOVERED(UO-GAME-UNK-0002) */
    vec3_t currentAngles; /* +0x148, RECOVERED(UO-GAME-UNK-0025) */
    int32_t passEntityNum; /* +0x154, vehicle-owner overlay */
    int32_t eventTime2; /* +0x158, mirrored by G_AddEvent */
    int32_t worldspawnSpawnflagsScratch; /* +0x15c, SP_worldspawn temporary copy */
    gclient_t *client; /* +0x160 */
    vehicle_state_t *vehicle; /* +0x164, vehicle state owned by script_vehicle entities */
    turret_state_t *turretState; /* +0x168, RECOVERED(UO-GAME-UNK-0101) */

    /* === Linked / model / flags (0x16c-0x1ab) === */
    uint8_t linked; /* +0x16c, linked-state byte;
                                                RECOVERED(UO-GAME-UNK-0016) */
    uint8_t linkedByte16d; /* +0x16d */
    uint8_t doorSoundCloseEnd; /* +0x16e */
    uint8_t doorSoundOpening; /* +0x16f */
    uint8_t doorSoundClosing; /* +0x170 */
    uint8_t doorSoundOpenEnd; /* +0x171 */
    uint8_t doorMovingClientSound; /* +0x172, copied to clientSound while
                                                binary doors move. */
    uint8_t doorSoundOpenLoop; /* +0x173 */
    uint8_t doorSoundCloseLoop; /* +0x174 */
    uint8_t doorSoundLocked; /* +0x175 */
    uint8_t doorSoundOpeningQuiet; /* +0x176 */
    uint8_t doorSoundOpenQuietEnd; /* +0x177 */
    uint8_t doorSoundClosingQuiet; /* +0x178 */
    uint8_t doorSoundCloseQuietEnd; /* +0x179 */
    uint8_t itemSoundAlias; /* +0x17a */
    uint8_t clientSpawnStateByte; /* +0x17b */
    uint8_t clientSpawnResetByte; /* +0x17c, ClientSpawn clears this byte */
    uint8_t takeDamage; /* +0x17d, settakedamage/G_Damage gate;
                                                also reused by movers as an activation flag. */
    uint8_t activeState; /* +0x17e, item auto-touch and mover/vehicle active state */
    uint8_t
        reserved_17f; /* +0x17f, size 0x001; gentity state-byte audit found no maintained access between activeState and moverState; retained for ABI. */
    uint8_t moverState; /* +0x180 */
    uint8_t modelIndex; /* +0x181, G_SetModel / DObj model index */
    uint8_t
        attachIgnoreCollision; /* +0x182, AUDITED_DECOMPILER(0x77f6a, 77f6a_script_method_scriptbuiltin_getattachignorecollision.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): attach-ignore collision bitset. */
    uint8_t padding183; /* +0x183, aligns scriptClassname. */
    uint16_t scriptClassname; /* +0x184, script classname string id. */
    char padding186[2]; /* +0x186..+0x187, aligns spawnflags. */
    int32_t spawnflags; /* +0x188, item/spawn entity flags */
    uint32_t flags; /* +0x18c, RECOVERED(UO-GAME-UNK-0017) */
    int32_t lastThinkTime; /* +0x190, also mirrored by G_AddEvent */
    int32_t skipTypeDispatch; /* +0x194, timeout frees without type dispatch */
    int32_t unlinkOnTimeout; /* +0x198, no-respawn item pickup unlink flag */
    float bounceFactor; /* +0x19c, G_BounceItem velocity scale */
    int32_t clipmask; /* +0x1a0 */
    int32_t lastFrameNum; /* +0x1a4, RECOVERED(UO-GAME-UNK-0148) */
    gentity_t *entityRef; /* +0x1a8, RECOVERED(UO-GAME-UNK-0002) */
    gentity_t *targetLocationNext; /* +0x1ac, target_location link list;
                                                also binary-mover linked trigger sentinel */
    char abiGap_1b0
        [0x004]; /* +0x1b0, size 0x004; gentity target-location/mover audit found no maintained access before moverPos1; retained for ABI. */
    vec3_t moverPos1; /* +0x1b4, binary mover position 1 /
                                                script mover position linear start */
    vec3_t moverPos2; /* +0x1c0, binary mover position 2 /
                                                script mover position decel start */

    /* === Damage point / speed (offset range 0x1cc-0x1ff) === */
    vec3_t damagePoint; /* +0x1cc, RECOVERED(UO-GAME-UNK-0179);
                                                also script mover position target. */
    uint16_t targetLocationMessage; /* +0x1d8, target_location message string id */
    uint16_t padding1da; /* +0x1da..+0x1db, aligns dynaSinkEndTime. */
    int32_t dynaSinkEndTime; /* +0x1dc, DynaSink free deadline */
    float doorYawOffset; /* +0x1e0, func_door_rotating yaw /
                                                script mover angular linear time */
    uint16_t target; /* +0x1e4, script target string id */
    uint16_t targetname; /* +0x1e6, script_vehicle_node lookup name */
    uint16_t teamName; /* +0x1e8, script "team" string */
    char padding1ea[2]; /* +0x1ea..+0x1eb, aligns the following dword lane. */
    char abiGap_1ec[4]; /* +0x1ec..+0x1ef; no original game-module access found. */
    float maxSpeed; /* +0x1f0, vehicle maximum speed;
                                                also binary/script mover speed float. */
    float doorAltSpeed; /* +0x1f4, script mover angular speed */
    vec3_t moverDir; /* +0x1f8, script mover angular linear start */
    int32_t moverDuration; /* +0x204 */
    int32_t moverAltDuration; /* +0x208 */

    /* === Think / callbacks (offset range 0x20c-0x22f) === */
    int32_t nextthink; /* +0x20c */
    void (*think)(gentity_t *ent); /* +0x210 */
    gentity_mover_reached_t moverReached; /* +0x214, script mover reached callback */
    gentity_mover_blocked_t moverBlocked; /* +0x218, mover blocked callback */
    gentity_touch_t touch; /* +0x21c, including item touch callbacks */
    gentity_use_t use; /* +0x220, including item use callbacks */
    gentity_pain_t pain; /* +0x224 */
    gentity_die_t die; /* +0x228 */
    char abiGap_22c
        [0x004]; /* +0x22c, size 0x004; callback audit found no maintained access between die callback and controller; retained for ABI. */
    gentity_controller_t controller; /* +0x230, DObj controller callback */
    int32_t painEventTime; /* +0x234 */
    char abiGap_238_23f
        [0x008]; /* +0x238..0x23f, size 0x008; pain/health audit found no maintained access before health; retained for ABI. */
    int32_t health; /* +0x240, RECOVERED(UO-GAME-UNK-0013) */
    int32_t maxHealth; /* +0x244, RECOVERED(UO-GAME-UNK-0002) */
    int32_t damage; /* +0x248, missile direct damage */
    int32_t splashDamage; /* +0x24c, missile radius max damage */
    int32_t splashMinDamage; /* +0x250, missile radius min damage */
    int32_t splashRadius; /* +0x254, missile radius */
    int32_t methodOfDeath; /* +0x258, missile direct MOD */
    int32_t splashMethodOfDeath; /* +0x25c, missile radius MOD */

    /* === Health / item data (offset range 0x260-0x29f) === */
    int32_t itemCount; /* +0x260 */
    char abiGap_264[0x004]; /* +0x264, size 0x004; item/death audit found no maintained access before attacker; retained for ABI. */
    gentity_t *attacker; /* +0x268, death attacker;
                                                RECOVERED(UO-GAME-UNK-0179) */
    gentity_t *triggerActivator; /* +0x26c, trigger_multiple activator overlay */
    gentity_t *teamChain; /* +0x270 */
    gentity_t *teamMaster; /* +0x274 */
    float itemWait; /* +0x278, item wait time /
                                                script mover position linear time */
    float itemRandom; /* +0x27c, item random time /
                                                script mover angular decel time */
    int32_t enemyScanRadius; /* +0x280, miscGunnerEnemyScan radius */
    float concussiveFxEndTime; /* +0x284, Concussive_fx lifetime deadline;
                                                also trigger_use/static pain delay and
                                                script mover position decel time. */
    char abiGap_288
        [0x004]; /* +0x288, size 0x004; item/trigger/mover audit found no maintained access before ownerIconDelaySeconds; retained for ABI. */
    float ownerIconDelaySeconds; /* +0x28c, script_vehicle_owner_icon delay */
    vec3_t damageDir; /* +0x290, RECOVERED(UO-GAME-UNK-0179);
                                                also script mover angular decel start. */
    vec3_t scriptMoverAngleTarget; /* +0x29c, script mover angular target */
    gitem_t *itemInfo; /* +0x2a8, item definition for item entities */
    char abiGap_2ac_2b7
        [0x00c]; /* +0x2ac..0x2b7, size 0x00c; item/door audit found no maintained access between itemInfo and doorLocked; retained for ABI. */
    int32_t doorLocked; /* +0x2b8, RECOVERED(UO-GAME-UNK-0002) */
    float vehiclePrimaryYawClamp; /* +0x2bc, script vehicle primary yaw clamp */
    float scriptVehiclePrimaryPitchClamp; /* +0x2c0, "varc" spawn field */
    int32_t missionLevel; /* +0x2c4, "missionlevel" spawn field */
    char abiGap_2c8_2cb
        [0x004]; /* +0x2c8..0x2cb, size 0x004; vehicle/spawner audit found no maintained access before size spawn fields; retained for ABI. */
    int32_t startSize; /* +0x2cc, "start_size" spawn field */
    int32_t endSize; /* +0x2d0, "end_size" spawn field */
    char abiGap_2d4_2d7
        [0x004]; /* +0x2d4..0x2d7, size 0x004; vehicle/spawner audit found no maintained access before spawnItem; retained for ABI. */
    uint16_t spawnItem; /* +0x2d8, misc_spawner spawnitem key */
    uint16_t padding2da; /* +0x2da..+0x2db, aligns droppedClipCount. */

    /* === Dropped item clip (offset 0x2dc) === */
    int32_t droppedClipCount; /* +0x2dc */
    char abiGap_2e0_2eb
        [0x00c]; /* +0x2e0..0x2eb, size 0x00c; dropped-item/link audit found no maintained access before track; retained for ABI. */
    uint16_t scriptTrack; /* +0x2ec, "track" spawn field string id */
    char padding2ee[2]; /* +0x2ee..+0x2ef, aligns the following dword lane. */
    char abiGap_2f0[4]; /* +0x2f0..+0x2f3; no original game-module access found. */

    /* === Link info / children / attach (offset range 0x2f4-0x347) === */
    entityLinkInfo_t *linkInfo; /* +0x2f4 */
    gentity_t *firstChild; /* +0x2f8 */
    uint8_t attachModelIndex[6]; /* +0x2fc */
    uint16_t attachTagIndex[6]; /* +0x302, script string ids */
    char padding30e[2]; /* +0x30e..+0x30f, aligns validationToken. */
    int32_t validationToken; /* +0x310, RECOVERED(UO-GAME-UNK-0136) */
    uint16_t vehicleSpawnName; /* +0x314, script_vehicle spawn vehicle name */
    char padding316[2]; /* +0x316..+0x317, aligns vehiclePrimaryDisabled. */
    int32_t vehiclePrimaryDisabled; /* +0x318, also driver cooldown overlay */
    gentity_t *vehicleOwner; /* +0x31c */
    int32_t vehicleReenterTime; /* +0x320 */
    int32_t lastVehicleEntityNum; /* +0x324 */
    gentity_t *nextFree; /* +0x328, RECOVERED(UO-GAME-UNK-0149) */
    int32_t missileFuseTime; /* +0x32c, grenade stored fuse deadline */
    int32_t parentEntityNum; /* +0x330, RECOVERED(UO-GAME-UNK-0180) */
    char abiGap_334
        [0x004]; /* +0x334, size 0x004; missile/spawnvar audit found no maintained access before saved spawn vars; retained for ABI. */
    int32_t savedSpawnVarCount; /* +0x338, RECOVERED(UO-GAME-UNK-0121) */
    char **savedSpawnVarPairs; /* +0x33c, RECOVERED(UO-GAME-UNK-0121) */
    int32_t savedSpawnTextLength; /* +0x340, RECOVERED(UO-GAME-UNK-0121) */
    char *savedSpawnText; /* +0x344, RECOVERED(UO-GAME-UNK-0121) */
    uint8_t dobjDirty; /* +0x348 */
    char padding349[3]; /* +0x349..+0x34b, gentity_t tail padding. */
};

/* NOT_FROM_ORIGINAL_SOURCE: maintained typed predicate over recovered gentity_t fields. */
static inline qboolean game_compat_gentity_can_take_damage(const gentity_t *ent)
{
    return ent->takeDamage != 0 ? qtrue : qfalse;
}

#define GAME_STATIC_ASSERT(name, condition) typedef char game_static_assert_##name[(condition) ? 1 : -1]

#if UINTPTR_MAX == UINT32_MAX
#define GAME_I386_LAYOUT_ASSERT(name, condition) GAME_STATIC_ASSERT(name, condition)
#else
#define GAME_I386_LAYOUT_ASSERT(name, condition) typedef char game_i386_layout_assert_skipped_##name[1]
#endif

#ifndef GAME_REQUIRE_X87_LONG_DOUBLE
#define GAME_REQUIRE_X87_LONG_DOUBLE 1
#endif

#define GAME_HOST_LONG_DOUBLE_IS_X87 (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384)

#if !GAME_REQUIRE_X87_LONG_DOUBLE && !defined(GAME_ALLOW_NON_EQUIVALENT_LONG_DOUBLE)
#error \
    "GAME_REQUIRE_X87_LONG_DOUBLE=0 is only allowed with GAME_ALLOW_NON_EQUIVALENT_LONG_DOUBLE for explicit non-equivalence analysis builds"
#endif

GAME_STATIC_ASSERT(recovered_char_bit_width, CHAR_BIT == 8);
GAME_STATIC_ASSERT(recovered_int_abi_width, sizeof(int) == 4);
GAME_STATIC_ASSERT(recovered_int32_width, sizeof(int32_t) == 4);
GAME_STATIC_ASSERT(recovered_uint32_width, sizeof(uint32_t) == 4);
GAME_STATIC_ASSERT(recovered_float_radix, FLT_RADIX == 2);
GAME_STATIC_ASSERT(recovered_float_binary32, FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128);
GAME_STATIC_ASSERT(recovered_double_binary64, DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024);
GAME_STATIC_ASSERT(recovered_long_double_x87_when_required, !GAME_REQUIRE_X87_LONG_DOUBLE || GAME_HOST_LONG_DOUBLE_IS_X87);

GAME_STATIC_ASSERT(player_state_size, sizeof(playerState_t) == 0x4504);
GAME_STATIC_ASSERT(gclient_player_state_offset, offsetof(gclient_t, ps) == 0x000);
GAME_STATIC_ASSERT(gclient_ps_command_time_offset, offsetof(gclient_t, ps.commandTime) == 0x000);
GAME_STATIC_ASSERT(gclient_pmType_offset, offsetof(gclient_t, ps.pmType) == 0x004);
GAME_STATIC_ASSERT(gclient_player_state_flags_offset, offsetof(gclient_t, ps.playerStateFlags) == 0x00c);
GAME_STATIC_ASSERT(gclient_pm_time_offset, offsetof(gclient_t, ps.pmTime) == 0x010);
GAME_STATIC_ASSERT(gclient_weapon_time_offset, offsetof(gclient_t, ps.weaponTime) == 0x02c);
GAME_STATIC_ASSERT(gclient_weapon_delay_offset, offsetof(gclient_t, ps.weaponDelay) == 0x030);
GAME_STATIC_ASSERT(gclient_grenade_time_left_offset, offsetof(gclient_t, ps.grenadeTimeLeft) == 0x034);
GAME_STATIC_ASSERT(gclient_foliage_sound_time_offset, offsetof(gclient_t, ps.foliageSoundTime) == 0x038);
GAME_STATIC_ASSERT(gclient_fatigue_sound_time_offset, offsetof(gclient_t, ps.fatigueSoundTime) == 0x03c);
GAME_STATIC_ASSERT(gclient_gravity_offset, offsetof(gclient_t, ps.gravity) == 0x040);
GAME_STATIC_ASSERT(gclient_lean_fraction_offset, offsetof(gclient_t, ps.leanFraction) == 0x044);
GAME_STATIC_ASSERT(gclient_speed_offset, offsetof(gclient_t, ps.speed) == 0x048);
GAME_STATIC_ASSERT(gclient_origin_offset, offsetof(gclient_t, ps.psOrigin) == 0x014);
GAME_STATIC_ASSERT(gclient_velocity_offset, offsetof(gclient_t, ps.velocity) == 0x020);
GAME_STATIC_ASSERT(gclient_ground_offset, offsetof(gclient_t, ps.groundEntityNum) == 0x058);
GAME_STATIC_ASSERT(gclient_ladder_normal_offset, offsetof(gclient_t, ps.ladderNormal) == 0x05c);
GAME_STATIC_ASSERT(gclient_ladder_normal_y_offset, offsetof(gclient_t, ps.ladderNormal[1]) == 0x060);
GAME_STATIC_ASSERT(gclient_ladder_normal_z_offset, offsetof(gclient_t, ps.ladderNormal[2]) == 0x064);
GAME_STATIC_ASSERT(gclient_movement_dir_offset, offsetof(gclient_t, ps.movementDir) == 0x080);
GAME_STATIC_ASSERT(gclient_entity_state_flags_offset, offsetof(gclient_t, ps.entityStateFlags) == 0x084);
GAME_STATIC_ASSERT(gclient_event_index_offset, offsetof(gclient_t, ps.eventIndex) == 0x088);
GAME_STATIC_ASSERT(gclient_events_offset, offsetof(gclient_t, ps.events) == 0x08c);
GAME_STATIC_ASSERT(gclient_predictable_event_parms_offset, offsetof(gclient_t, ps.eventParms) == 0x09c);
GAME_STATIC_ASSERT(gclient_event_parms_offset, offsetof(gclient_t, ps.eventParms) == 0x09c);
GAME_STATIC_ASSERT(gclient_legs_timer_offset, offsetof(gclient_t, ps.legsTimer) == 0x070);
GAME_STATIC_ASSERT(gclient_legs_anim_offset, offsetof(gclient_t, ps.legsAnim) == 0x074);
GAME_STATIC_ASSERT(gclient_torso_timer_offset, offsetof(gclient_t, ps.torsoTimer) == 0x078);
GAME_STATIC_ASSERT(gclient_torso_anim_offset, offsetof(gclient_t, ps.torsoAnim) == 0x07c);
GAME_STATIC_ASSERT(gclient_ps_client_num_offset, offsetof(gclient_t, ps.psClientNum) == 0x0d4);
GAME_STATIC_ASSERT(gclient_current_weapon_offset, offsetof(gclient_t, ps.currentWeapon) == 0x0d8);
GAME_STATIC_ASSERT(gclient_weapon_state_offset, offsetof(gclient_t, ps.weaponState) == 0x0dc);
GAME_STATIC_ASSERT(gclient_ads_fraction_offset, offsetof(gclient_t, ps.adsFraction) == 0x0e0);
GAME_STATIC_ASSERT(gclient_ps_view_model_index_offset, offsetof(gclient_t, ps.viewModelIndex) == 0x0e4);
GAME_STATIC_ASSERT(gclient_view_angles_offset, offsetof(gclient_t, ps.viewAngles) == 0x0e8);
GAME_STATIC_ASSERT(gclient_view_height_target_offset, offsetof(gclient_t, ps.viewHeightTarget) == 0x0f4);
GAME_STATIC_ASSERT(gclient_view_height_lerp_pos_adj_offset, offsetof(gclient_t, ps.viewHeightLerpPosAdj) == 0x108);
GAME_STATIC_ASSERT(gclient_damage_yaw_offset, offsetof(gclient_t, ps.damageYaw) == 0x110);
GAME_STATIC_ASSERT(gclient_damage_pitch_offset, offsetof(gclient_t, ps.damagePitch) == 0x114);
GAME_STATIC_ASSERT(gclient_player_state_health_offset, offsetof(gclient_t, ps.stats[STAT_HEALTH]) == 0x11c);
GAME_STATIC_ASSERT(gclient_death_yaw_offset, offsetof(gclient_t, ps.stats[STAT_DEAD_YAW]) == 0x120);
GAME_STATIC_ASSERT(gclient_max_health_offset, offsetof(gclient_t, ps.stats[STAT_MAX_HEALTH]) == 0x124);
GAME_STATIC_ASSERT(gclient_ident_client_num_offset, offsetof(gclient_t, ps.stats[STAT_IDENT_CLIENT_NUM]) == 0x128);
GAME_STATIC_ASSERT(gclient_ident_client_health_offset, offsetof(gclient_t, ps.stats[STAT_IDENT_CLIENT_HEALTH]) == 0x12c);
GAME_STATIC_ASSERT(gclient_spawn_count_offset, offsetof(gclient_t, ps.stats[STAT_SPAWN_COUNT]) == 0x130);
GAME_STATIC_ASSERT(gclient_prone_direction_offset, offsetof(gclient_t, ps.proneDirection) == 0x5a4);
GAME_STATIC_ASSERT(gclient_prone_direction_pitch_offset, offsetof(gclient_t, ps.proneDirectionPitch) == 0x5a8);
GAME_STATIC_ASSERT(gclient_ammo_offset, offsetof(gclient_t, ps.ammo) == 0x134);
GAME_STATIC_ASSERT(gclient_clips_offset, offsetof(gclient_t, ps.clips) == 0x334);
GAME_STATIC_ASSERT(gclient_weapon_bits_offset, offsetof(gclient_t, ps.weaponBits) == 0x534);
GAME_STATIC_ASSERT(gclient_weapon_slots_offset, offsetof(gclient_t, ps.weaponSlots) == 0x544);
GAME_STATIC_ASSERT(gclient_weapon_rechamber_bits_offset, offsetof(gclient_t, ps.weaponRechamberBits) == 0x54c);
GAME_STATIC_ASSERT(gclient_player_mins_offset, offsetof(gclient_t, ps.playerMins) == 0x55c);
GAME_STATIC_ASSERT(gclient_player_mins_z_offset, offsetof(gclient_t, ps.playerMins[2]) == 0x564);
GAME_STATIC_ASSERT(gclient_prone_view_height_offset, offsetof(gclient_t, ps.proneViewHeight) == 0x574);
GAME_STATIC_ASSERT(gclient_crouch_view_height_offset, offsetof(gclient_t, ps.crouchViewHeight) == 0x578);
GAME_STATIC_ASSERT(gclient_stand_view_height_offset, offsetof(gclient_t, ps.standViewHeight) == 0x57c);
GAME_STATIC_ASSERT(gclient_dead_view_height_offset, offsetof(gclient_t, ps.deadViewHeight) == 0x580);
GAME_STATIC_ASSERT(gclient_walk_speed_scale_offset, offsetof(gclient_t, ps.walkSpeedScale) == 0x584);
GAME_STATIC_ASSERT(gclient_run_speed_scale_offset, offsetof(gclient_t, ps.runSpeedScale) == 0x588);
GAME_STATIC_ASSERT(gclient_sprint_speed_scale_offset, offsetof(gclient_t, ps.sprintSpeedScale) == 0x58c);
GAME_STATIC_ASSERT(gclient_prone_speed_scale_offset, offsetof(gclient_t, ps.proneSpeedScale) == 0x590);
GAME_STATIC_ASSERT(gclient_crouch_speed_scale_offset, offsetof(gclient_t, ps.crouchSpeedScale) == 0x594);
GAME_STATIC_ASSERT(gclient_strafe_speed_scale_offset, offsetof(gclient_t, ps.strafeSpeedScale) == 0x598);
GAME_STATIC_ASSERT(gclient_back_speed_scale_offset, offsetof(gclient_t, ps.backSpeedScale) == 0x59c);
GAME_STATIC_ASSERT(gclient_lean_speed_scale_offset, offsetof(gclient_t, ps.leanSpeedScale) == 0x5a0);
GAME_STATIC_ASSERT(gclient_friction_offset, offsetof(gclient_t, ps.friction) == 0x5c0);
GAME_STATIC_ASSERT(gclient_fatigue_scale_offset, offsetof(gclient_t, ps.fatigueScale) == 0x5b0);
GAME_STATIC_ASSERT(gclient_last_sprint_time_offset, offsetof(gclient_t, ps.lastSprintTime) == 0x5b4);
GAME_STATIC_ASSERT(gclient_torso_height_offset, offsetof(gclient_t, ps.torsoHeight) == 0x608);
GAME_STATIC_ASSERT(gclient_torso_pitch_offset, offsetof(gclient_t, ps.torsoPitch) == 0x60c);
GAME_STATIC_ASSERT(gclient_waist_pitch_offset, offsetof(gclient_t, ps.waistPitch) == 0x610);
GAME_STATIC_ASSERT(gclient_weapon_anim_offset, offsetof(gclient_t, ps.weaponAnim) == 0x624);
GAME_STATIC_ASSERT(gclient_motion_state_offset, offsetof(gclient_t, ps.motionState) == 0x62c);
GAME_STATIC_ASSERT(gclient_external_velocity_offset, offsetof(gclient_t, ps.motionState.externalVelocity) == 0x62c);
GAME_STATIC_ASSERT(gclient_shellshock_start_offset, offsetof(gclient_t, ps.motionState.shellshock.time) == 0x630);
GAME_I386_LAYOUT_ASSERT(objective_size, sizeof(objective_t) == 0x1c);
GAME_STATIC_ASSERT(gclient_objectives_offset, offsetof(gclient_t, ps.objectives) == 0x638);
GAME_STATIC_ASSERT(gclient_hud_elems_current_offset, offsetof(gclient_t, ps.hudCurrent) == 0x7f8);
GAME_STATIC_ASSERT(gclient_hud_elems_archived_offset, offsetof(gclient_t, ps.hudArchival) == 0x267c);
GAME_STATIC_ASSERT(gclient_delta_time_offset, offsetof(gclient_t, ps.deltaTime) == 0x4500);
GAME_STATIC_ASSERT(gclient_session_state_offset, offsetof(gclient_t, sessionState) == 0x4504);
GAME_STATIC_ASSERT(gclient_script_follow_client_offset, offsetof(gclient_t, followClient) == 0x4508);
GAME_STATIC_ASSERT(gclient_script_archive_time_offset, offsetof(gclient_t, archiveTime) == 0x4510);
GAME_STATIC_ASSERT(gclient_script_score_offset, offsetof(gclient_t, score) == 0x4514);
GAME_STATIC_ASSERT(gclient_script_deaths_offset, offsetof(gclient_t, deaths) == 0x4518);
GAME_STATIC_ASSERT(gclient_script_pers_offset, offsetof(gclient_t, pers) == 0x451c);
GAME_STATIC_ASSERT(gclient_command_offset, offsetof(gclient_t, command) == 0x4524);
GAME_STATIC_ASSERT(gclient_command_time_offset, offsetof(gclient_t, command.commandTime) == 0x4524);
GAME_STATIC_ASSERT(gclient_command_buttons_offset, offsetof(gclient_t, command.buttons) == 0x4528);
GAME_STATIC_ASSERT(gclient_command_wbuttons_offset, offsetof(gclient_t, command.wbuttons) == 0x4529);
GAME_STATIC_ASSERT(gclient_command_weapon_offset, offsetof(gclient_t, command.weapon) == 0x452a);
GAME_STATIC_ASSERT(gclient_command_angles_offset, offsetof(gclient_t, command.angles) == 0x452c);
GAME_STATIC_ASSERT(gclient_command_forwardmove_offset, offsetof(gclient_t, command.forwardmove) == 0x4538);
GAME_STATIC_ASSERT(gclient_old_pmove_command_offset, offsetof(gclient_t, oldPmoveCommand) == 0x453c);
GAME_STATIC_ASSERT(gclient_complaint_disabled_offset, offsetof(gclient_t, complaintDisabled) == 0x4554);
GAME_STATIC_ASSERT(gclient_predict_items_offset, offsetof(gclient_t, predictItems) == 0x4558);
GAME_STATIC_ASSERT(gclient_pmove_fixed_offset, offsetof(gclient_t, pmoveFixed) == 0x455c);
GAME_STATIC_ASSERT(gclient_clean_name_offset, offsetof(gclient_t, cleanName) == 0x4560);
GAME_STATIC_ASSERT(gclient_script_handicap_offset, offsetof(gclient_t, handicap) == 0x4580);
GAME_STATIC_ASSERT(gclient_script_normal_max_health_offset, offsetof(gclient_t, normalMaxHealth) == 0x4584);
GAME_STATIC_ASSERT(gclient_script_max_speed_offset, offsetof(gclient_t, maxSpeed) == 0x4588);
GAME_STATIC_ASSERT(gclient_called_votes_offset, offsetof(gclient_t, calledVotes) == 0x4594);
GAME_STATIC_ASSERT(gclient_view_model_index_offset, offsetof(gclient_t, viewModelIndex) == 0x45a8);
GAME_STATIC_ASSERT(gclient_session_squad_offset, offsetof(gclient_t, sessionSquad) == 0x45b4);
GAME_STATIC_ASSERT(gclient_client_num_offset, offsetof(gclient_t, clientNum) == 0x45b8);
GAME_STATIC_ASSERT(gclient_session_team_offset, offsetof(gclient_t, sessionTeam) == 0x45bc);
GAME_STATIC_ASSERT(gclient_base_model_index_offset, offsetof(gclient_t, baseModelIndex) == 0x45c0);
GAME_STATIC_ASSERT(gclient_attach_model_indices_offset, offsetof(gclient_t, attachModelIndices) == 0x45c4);
GAME_STATIC_ASSERT(gclient_attach_tag_indices_offset, offsetof(gclient_t, attachTagIndices) == 0x45dc);
GAME_STATIC_ASSERT(gclient_script_userinfo_name_offset, offsetof(gclient_t, userInfoName) == 0x45f4);
GAME_STATIC_ASSERT(gclient_script_no_inactivity_kick_offset, offsetof(gclient_t, spectatorActivityState) == 0x4614);
GAME_STATIC_ASSERT(gclient_archive_client_offset, offsetof(gclient_t, archiveClient) == 0x4618);
GAME_STATIC_ASSERT(gclient_latched_buttons_offset, offsetof(gclient_t, latchedButtons) == 0x4634);
GAME_STATIC_ASSERT(gclient_latched_wbuttons_offset, offsetof(gclient_t, latchedWbuttons) == 0x4640);
GAME_STATIC_ASSERT(gclient_spectator_snapshot_origin_offset, offsetof(gclient_t, spectatorSnapshotOrigin) == 0x4644);
GAME_STATIC_ASSERT(gclient_inactivity_time_offset, offsetof(gclient_t, inactivityTime) == 0x4678);
GAME_STATIC_ASSERT(gclient_spectator_inactivity_time_offset, offsetof(gclient_t, spectatorInactivityTime) == 0x467c);
GAME_STATIC_ASSERT(gclient_inactivity_warning_sent_offset, offsetof(gclient_t, inactivityWarningSent) == 0x4680);
GAME_STATIC_ASSERT(gclient_spectator_inactivity_warning_offset, offsetof(gclient_t, spectatorInactivityWarning) == 0x4684);
GAME_STATIC_ASSERT(gclient_vehicle_control_time_offset, offsetof(gclient_t, vehicleControlTime) == 0x468c);
GAME_STATIC_ASSERT(gclient_vehicle_delay_ignore_time_offset, offsetof(gclient_t, vehicleDelayIgnoreTime) == 0x4690);
GAME_STATIC_ASSERT(gclient_driver_unlink_requested_offset, offsetof(gclient_t, driverUnlinkRequested) == 0x4694);
GAME_STATIC_ASSERT(vehicle_state_type_offset, offsetof(vehicle_state_t, typeIndex) == 0x22c);
GAME_STATIC_ASSERT(vehicle_physics_state_size, sizeof(vehicle_physics_state_t) == 0xb8);
GAME_STATIC_ASSERT(vehicle_physics_state_view_clamp_target_offset, offsetof(vehicle_physics_state_t, viewClampTargetAngles) == 0x18);
GAME_STATIC_ASSERT(vehicle_physics_state_velocity_offset, offsetof(vehicle_physics_state_t, velocity) == 0x30);
GAME_STATIC_ASSERT(vehicle_physics_state_wheel_material_offset, offsetof(vehicle_physics_state_t, wheelMaterial) == 0xa0);
GAME_STATIC_ASSERT(vehicle_state_origin_offset, offsetof(vehicle_state_t, origin) == 0x0b8);
GAME_STATIC_ASSERT(vehicle_state_view_clamp_target_offset, offsetof(vehicle_state_t, viewClampTargetAngles) == 0x0d0);
GAME_STATIC_ASSERT(vehicle_state_velocity_offset, offsetof(vehicle_state_t, velocity) == 0x0e8);
GAME_STATIC_ASSERT(vehicle_state_angular_velocity_offset, offsetof(vehicle_state_t, angularVelocity) == 0x0f4);
GAME_STATIC_ASSERT(vehicle_state_acceleration_offset, offsetof(vehicle_state_t, acceleration) == 0x100);
GAME_STATIC_ASSERT(vehicle_state_steer_angle_offset, offsetof(vehicle_state_t, steerAngle) == 0x10c);
GAME_STATIC_ASSERT(vehicle_state_previous_physics_offset, offsetof(vehicle_state_t, previousPhysicsState) == 0x170);
GAME_STATIC_ASSERT(vehicle_state_entity_num_offset, offsetof(vehicle_state_t, entityNum) == 0x228);
GAME_STATIC_ASSERT(vehicle_state_slot_index_offset, offsetof(vehicle_state_t, slotIndex) == 0x230);
GAME_STATIC_ASSERT(vehicle_state_hint_string_index_offset, offsetof(vehicle_state_t, hintStringIndex) == 0x234);
GAME_STATIC_ASSERT(vehicle_state_wait_node_speed_threshold_offset, offsetof(vehicle_state_t, waitNodeSpeedThreshold) == 0x23c);
GAME_STATIC_ASSERT(vehicle_state_primary_fire_time_offset, offsetof(vehicle_state_t, primaryFireTime) == 0x240);
GAME_STATIC_ASSERT(vehicle_state_alt_fire_time_offset, offsetof(vehicle_state_t, altFireTime) == 0x244);
GAME_STATIC_ASSERT(vehicle_state_primary_flash_selector_offset, offsetof(vehicle_state_t, primaryFlashSelector) == 0x248);
GAME_STATIC_ASSERT(vehicle_state_gunner_turret_state_offset, offsetof(vehicle_state_t, gunnerTurretState) == 0x254);
GAME_STATIC_ASSERT(vehicle_state_gunner_fire_time_offset, offsetof(vehicle_state_t, gunnerFireTime) == 0x258);
GAME_STATIC_ASSERT(vehicle_state_alt_heat_offset, offsetof(vehicle_state_t, altHeat) == 0x25c);
GAME_STATIC_ASSERT(vehicle_state_gunner_heat_offset, offsetof(vehicle_state_t, gunnerHeat) == 0x260);
GAME_STATIC_ASSERT(vehicle_state_alt_overheating_offset, offsetof(vehicle_state_t, altOverheating) == 0x264);
GAME_STATIC_ASSERT(vehicle_state_gunner_overheating_offset, offsetof(vehicle_state_t, gunnerOverheating) == 0x268);
GAME_STATIC_ASSERT(vehicle_state_local_forward_accel_offset, offsetof(vehicle_state_t, localAccel.components.forward) == 0x2a4);
GAME_STATIC_ASSERT(vehicle_state_local_vertical_accel_offset, offsetof(vehicle_state_t, localAccel.components.vertical) == 0x2a8);
GAME_STATIC_ASSERT(vehicle_local_accel_size, sizeof(vehicle_local_accel_t) == 0x08);
GAME_STATIC_ASSERT(vehicle_state_sound_blends_offset, offsetof(vehicle_state_t, soundBlendEntityNums) == 0x2c4);
GAME_STATIC_ASSERT(vehicle_state_idle_sound_repeat_delay_offset, offsetof(vehicle_state_t, idleSoundBlendRepeatDelay) == 0x2d4);
GAME_STATIC_ASSERT(vehicle_state_run_sound_repeat_delay_offset, offsetof(vehicle_state_t, runSoundBlendRepeatDelay) == 0x2d8);
GAME_STATIC_ASSERT(vehicle_state_alt_weapon_fire_sound_offset, offsetof(vehicle_state_t, altWeaponFireSound) == 0x2dc);
GAME_STATIC_ASSERT(vehicle_state_alt_weapon_stop_sound_offset, offsetof(vehicle_state_t, altWeaponStopSound) == 0x2e0);
GAME_STATIC_ASSERT(vehicle_state_alt_weapon_sound_time_offset, offsetof(vehicle_state_t, altWeaponSoundTime) == 0x2e4);
GAME_STATIC_ASSERT(vehicle_state_gunner_weapon_fire_sound_offset, offsetof(vehicle_state_t, gunnerWeaponFireSound) == 0x2e8);
GAME_STATIC_ASSERT(vehicle_state_gunner_weapon_stop_sound_offset, offsetof(vehicle_state_t, gunnerWeaponStopSound) == 0x2ec);
GAME_STATIC_ASSERT(vehicle_state_gunner_weapon_sound_time_offset, offsetof(vehicle_state_t, gunnerWeaponSoundTime) == 0x2f0);
GAME_STATIC_ASSERT(vehicle_state_driver_tag_offset, offsetof(vehicle_state_t, driverTagIndex) == 0x2f4);
GAME_STATIC_ASSERT(vehicle_state_detach_tag_offset, offsetof(vehicle_state_t, detachTagIndex) == 0x2f8);
GAME_STATIC_ASSERT(vehicle_state_popout_tag_offset, offsetof(vehicle_state_t, popoutTagIndex) == 0x2fc);
GAME_STATIC_ASSERT(vehicle_state_body_tag_offset, offsetof(vehicle_state_t, bodyTagIndex) == 0x300);
GAME_STATIC_ASSERT(vehicle_state_primary_base_tag_offset, offsetof(vehicle_state_t, primaryBaseTagIndex) == 0x304);
GAME_STATIC_ASSERT(vehicle_state_primary_turret_tag_offset, offsetof(vehicle_state_t, primaryTurretTagIndex) == 0x308);
GAME_STATIC_ASSERT(vehicle_state_primary_alt_turret_tag_offset, offsetof(vehicle_state_t, primaryAltTurretTagIndex) == 0x30c);
GAME_STATIC_ASSERT(vehicle_state_gunner_tag_offset, offsetof(vehicle_state_t, gunnerTagIndex) == 0x310);
GAME_STATIC_ASSERT(vehicle_state_gunner_turret_tag_offset, offsetof(vehicle_state_t, gunnerTurretTagIndex) == 0x314);
GAME_STATIC_ASSERT(vehicle_state_secondary_base_tag_offset, offsetof(vehicle_state_t, secondaryBaseTagIndex) == 0x318);
GAME_STATIC_ASSERT(vehicle_state_primary_flash_tags_offset, offsetof(vehicle_state_t, primaryFlashTagIndices) == 0x324);
GAME_STATIC_ASSERT(vehicle_state_alt_fire_tags_offset, offsetof(vehicle_state_t, altFireTagIndices) == 0x334);
GAME_STATIC_ASSERT(vehicle_state_secondary_flash_tags_offset, offsetof(vehicle_state_t, secondaryFlashTagIndices) == 0x344);
GAME_STATIC_ASSERT(vehicle_state_wheel_tags_offset, offsetof(vehicle_state_t, wheelTagIndices) == 0x354);
GAME_STATIC_ASSERT(vehicle_state_passenger_tags_offset, offsetof(vehicle_state_t, passengerTagIndices) == 0x36c);
GAME_STATIC_ASSERT(vehicle_state_primary_aim_sight_trace_result_offset, offsetof(vehicle_state_t, primaryAimSightTraceResult) == 0x37c);
GAME_STATIC_ASSERT(vehicle_state_scripted_driver_time_offset, offsetof(vehicle_state_t, scriptedDriverEndTime) == 0x390);
GAME_STATIC_ASSERT(vehicle_state_passenger_entity_nums_offset, offsetof(vehicle_state_t, passengerEntityNums) == 0x39c);
GAME_STATIC_ASSERT(vehicle_state_collision_fraction_offset, offsetof(vehicle_state_t, collisionSweepFraction) == 0x3b8);
GAME_STATIC_ASSERT(vehicle_state_last_input_time_offset, offsetof(vehicle_state_t, lastInputTime) == 0x3c0);
GAME_STATIC_ASSERT(vehicle_state_collision_notify_time_offset, offsetof(vehicle_state_t, collisionNotifyTime) == 0x3c8);
GAME_STATIC_ASSERT(vehicle_state_collision_sound_time_offset, offsetof(vehicle_state_t, collisionSoundTime) == 0x3cc);
GAME_STATIC_ASSERT(vehicle_state_collision_entity_offset, offsetof(vehicle_state_t, cachedCollisionEntityNum) == 0x3d0);
GAME_STATIC_ASSERT(vehicle_state_collision_distance_offset, offsetof(vehicle_state_t, cachedCollisionDistance) == 0x3d4);
GAME_STATIC_ASSERT(vehicle_state_last_stable_time_offset, offsetof(vehicle_state_t, lastStableTime) == 0x3d8);
GAME_STATIC_ASSERT(vehicle_state_last_solid_time_offset, offsetof(vehicle_state_t, lastSolidTime) == 0x3dc);
GAME_STATIC_ASSERT(vehicle_state_collision_normal_offset, offsetof(vehicle_state_t, collisionNormal) == 0x3e0);
GAME_STATIC_ASSERT(vehicle_info_type_offset, offsetof(vehicleInfo_t, type) == 0x040);
GAME_STATIC_ASSERT(vehicle_info_steer_wheels_offset, offsetof(vehicleInfo_t, steerWheels) == 0x044);
GAME_STATIC_ASSERT(vehicle_info_texture_scroll_offset, offsetof(vehicleInfo_t, textureScroll) == 0x048);
GAME_STATIC_ASSERT(vehicle_info_primary_dual_flash_offset, offsetof(vehicleInfo_t, primaryDualFlash) == 0x04c);
GAME_STATIC_ASSERT(vehicle_info_bullet_damage_offset, offsetof(vehicleInfo_t, bulletDamageEnabled) == 0x050);
GAME_STATIC_ASSERT(vehicle_info_grenade_damage_offset, offsetof(vehicleInfo_t, grenadeDamageEnabled) == 0x054);
GAME_STATIC_ASSERT(vehicle_info_explosive_damage_offset, offsetof(vehicleInfo_t, explosiveDamageEnabled) == 0x058);
GAME_STATIC_ASSERT(vehicle_info_gunner_seat_offset, offsetof(vehicleInfo_t, gunnerSeatEnabled) == 0x05c);
GAME_STATIC_ASSERT(vehicle_info_extra_passenger_count_offset, offsetof(vehicleInfo_t, extraPassengerCount) == 0x060);
GAME_STATIC_ASSERT(vehicle_info_texture_scroll_scale_offset, offsetof(vehicleInfo_t, textureScrollScale) == 0x064);
GAME_STATIC_ASSERT(vehicle_info_forward_input_scale_offset, offsetof(vehicleInfo_t, forwardInputScale) == 0x078);
GAME_STATIC_ASSERT(vehicle_info_acceleration_offset, offsetof(vehicleInfo_t, acceleration) == 0x06c);
GAME_STATIC_ASSERT(vehicle_info_collision_damage_scale_offset, offsetof(vehicleInfo_t, collisionDamageScale) == 0x080);
GAME_STATIC_ASSERT(vehicle_info_path_speed_offset, offsetof(vehicleInfo_t, pathSpeed) == 0x084);
GAME_STATIC_ASSERT(vehicle_info_suspension_travel_offset, offsetof(vehicleInfo_t, suspensionTravel) == 0x088);
GAME_STATIC_ASSERT(vehicle_info_turret_weapon_offset, offsetof(vehicleInfo_t, turretWeapon) == 0x08c);
GAME_STATIC_ASSERT(vehicle_info_turret_alt_weapon_offset, offsetof(vehicleInfo_t, turretAltWeapon) == 0x0cc);
GAME_STATIC_ASSERT(vehicle_info_primary_turret_limits_offset, offsetof(vehicleInfo_t, primaryYawLimitPos) == 0x10c);
GAME_STATIC_ASSERT(vehicle_info_turret_gunner_weapon_offset, offsetof(vehicleInfo_t, turretGunnerWeapon) == 0x120);
GAME_STATIC_ASSERT(vehicle_info_gunner_turret_limits_offset, offsetof(vehicleInfo_t, gunnerYawLimitPos) == 0x160);
GAME_STATIC_ASSERT(vehicle_info_sound_names_offset, offsetof(vehicleInfo_t, soundNames) == 0x174);
GAME_STATIC_ASSERT(vehicle_info_idle_blend_sound_offset, offsetof(vehicleInfo_t, idleBlendSound0) == 0x334);
GAME_STATIC_ASSERT(vehicle_info_primary_active_sound_offset, offsetof(vehicleInfo_t, primaryActiveSound) == 0x338);
GAME_STATIC_ASSERT(vehicle_info_collision_sound_offset, offsetof(vehicleInfo_t, collisionSound) == 0x33a);
GAME_STATIC_ASSERT(vehicle_info_path_speed_denom_offset, offsetof(vehicleInfo_t, pathSpeedDenom) == 0x33c);
GAME_STATIC_ASSERT(vehicle_info_damage_scale_front_offset, offsetof(vehicleInfo_t, damageScaleFront) == 0x340);
GAME_STATIC_ASSERT(vehicle_info_damage_scale_bullet_offset, offsetof(vehicleInfo_t, damageScaleBullet) == 0x350);
GAME_STATIC_ASSERT(vehicle_info_collision_mins_offset, offsetof(vehicleInfo_t, collisionMins) == 0x354);
GAME_STATIC_ASSERT(vehicle_info_collision_maxs_offset, offsetof(vehicleInfo_t, collisionMaxs) == 0x360);
GAME_STATIC_ASSERT(vehicle_info_collision_bounds_source_offset, offsetof(vehicleInfo_t, collisionBoundsSource) == 0x36c);
GAME_STATIC_ASSERT(vehicle_info_roll_limit_offset, offsetof(vehicleInfo_t, rollLimit) == 0x38c);
GAME_STATIC_ASSERT(vehicle_info_step_size_offset, offsetof(vehicleInfo_t, stepSize) == 0x390);
GAME_STATIC_ASSERT(vehicle_info_dismount_forward_offset, offsetof(vehicleInfo_t, dismountForwardOffset) == 0x394);
GAME_STATIC_ASSERT(vehicle_info_dismount_back_offset, offsetof(vehicleInfo_t, dismountBackOffset) == 0x398);
GAME_STATIC_ASSERT(vehicle_info_hint_string_offset, offsetof(vehicleInfo_t, hintString) == 0x39c);
GAME_STATIC_ASSERT(vehicle_state_size, sizeof(vehicle_state_t) == 0x3ec);
GAME_STATIC_ASSERT(vehicle_info_size, sizeof(vehicleInfo_t) == 0x3dc);
GAME_STATIC_ASSERT(effective_stance_enum_abi_size, sizeof(effectiveStance_t) == 4);
GAME_STATIC_ASSERT(bg_player_anim_pm_type_offset, offsetof(clientInfo_t, moduleState.pmType) == 0x004);
GAME_STATIC_ASSERT(clientinfo_client_num_anim_offset, offsetof(clientInfo_t, clientNum) == 0x008);
GAME_STATIC_ASSERT(bg_player_anim_name_alias_offset, offsetof(clientInfo_t, name) == 0x00c);
GAME_STATIC_ASSERT(bg_player_anim_team_alias_offset, offsetof(clientInfo_t, team) == 0x02c);
GAME_STATIC_ASSERT(bg_player_anim_base_model_name_alias_offset, offsetof(clientInfo_t, modelName) == 0x040);
GAME_STATIC_ASSERT(bg_player_anim_attach_model_names_alias_offset, offsetof(clientInfo_t, attachModelNames) == 0x080);
GAME_STATIC_ASSERT(bg_player_anim_attach_tag_names_alias_offset, offsetof(clientInfo_t, attachTagNames) == 0x200);
GAME_STATIC_ASSERT(clientinfo_info_valid_offset, offsetof(clientInfo_t, infoValid) == 0x000);
GAME_STATIC_ASSERT(clientinfo_client_num_offset, offsetof(clientInfo_t, clientNum) == 0x008);
GAME_STATIC_ASSERT(clientinfo_attach_tag_names_slot5_tail_offset,
                   offsetof(clientInfo_t, attachTagNames) + 5 * CLIENT_INFO_MODEL_NAME_SIZE + 0x3c == 0x37c);
GAME_I386_LAYOUT_ASSERT(clientinfo_dobj_saved_model_offset, offsetof(clientInfo_t, dobjSavedModel) == 0x4c8);
GAME_I386_LAYOUT_ASSERT(clientinfo_dobj_version_offset, offsetof(clientInfo_t, dobjVersion) == 0x4cc);
GAME_STATIC_ASSERT(bg_player_anim_legs_slot_offset, offsetof(clientInfo_t, legsYawAngle) == 0x380);
GAME_STATIC_ASSERT(bg_player_anim_torso_slot_offset, offsetof(clientInfo_t, torsoYawAngle) == 0x3b0);
GAME_STATIC_ASSERT(bg_player_anim_view_pitch_offset, offsetof(clientInfo_t, viewPitch) == 0x3e8);
GAME_STATIC_ASSERT(bg_player_anim_view_yaw_offset, offsetof(clientInfo_t, viewYaw) == 0x3ec);
GAME_STATIC_ASSERT(bg_player_anim_view_roll_offset, offsetof(clientInfo_t, viewRoll) == 0x3f0);
GAME_STATIC_ASSERT(bg_player_anim_legs_timer_offset, offsetof(clientInfo_t, legsTimer) == 0x388);
GAME_STATIC_ASSERT(bg_player_anim_legs_anim_offset, offsetof(clientInfo_t, legsAnim) == 0x38c);
GAME_STATIC_ASSERT(bg_player_anim_override_angle_offset, offsetof(clientInfo_t, turretOverrideAngles[1]) == 0x3f8);
GAME_STATIC_ASSERT(bg_player_anim_override_pitch_offset, offsetof(clientInfo_t, turretOverrideAngles[0]) == 0x3f4);
GAME_STATIC_ASSERT(bg_player_anim_override_yaw_offset, offsetof(clientInfo_t, turretOverrideAngles[1]) == 0x3f8);
GAME_STATIC_ASSERT(bg_player_anim_override_roll_offset, offsetof(clientInfo_t, turretOverrideAngles[2]) == 0x3fc);
GAME_STATIC_ASSERT(bg_player_anim_gun_hand_left_offset, offsetof(clientInfo_t, gunHandLeft) == 0x400);
GAME_STATIC_ASSERT(bg_player_anim_dobj_needs_update_alias_offset, offsetof(clientInfo_t, dobjNeedsUpdate) == 0x404);
GAME_STATIC_ASSERT(bg_player_anim_controller_angles_offset, offsetof(clientInfo_t, controllerAngles) == 0x408);
GAME_STATIC_ASSERT(bg_player_anim_local_tag_angles_offset, offsetof(clientInfo_t, localTagAngles) == 0x450);
GAME_STATIC_ASSERT(bg_player_anim_local_tag_offset, offsetof(clientInfo_t, localTagOffset) == 0x45c);
GAME_STATIC_ASSERT(bg_player_anim_condition_words_offset, offsetof(clientInfo_t, conditionWords) == 0x468);
GAME_STATIC_ASSERT(bg_player_anim_transition_time_offset, offsetof(clientInfo_t, animTransitionTime) == 0x4c0);
GAME_I386_LAYOUT_ASSERT(bg_player_anim_tree_offset, offsetof(clientInfo_t, animTree) == 0x4c4);
GAME_I386_LAYOUT_ASSERT(clientinfo_stride_size, sizeof(clientInfo_t) == 0x04d0u);
/* Native pointers make the full entity/client ABI assertions meaningful only
 * for an i386-targeted build. Fixed-width subviews above remain checked on all
 * hosts; post-pointer binary access should use explicit offset helpers. */
#if UINTPTR_MAX == 0xffffffffu
GAME_STATIC_ASSERT(entity_link_info_tag_offset, offsetof(entityLinkInfo_t, tagStringId) == 0x08);
GAME_STATIC_ASSERT(entity_link_info_parent_offset, offsetof(entityLinkInfo_t, parent) == 0x00);
GAME_STATIC_ASSERT(entity_link_info_next_child_offset, offsetof(entityLinkInfo_t, nextChild) == 0x04);
GAME_STATIC_ASSERT(entity_link_info_parent_tag_offset, offsetof(entityLinkInfo_t, parentTagIndex) == 0x0c);
GAME_STATIC_ASSERT(entity_link_info_rel_axis_offset, offsetof(entityLinkInfo_t, relAxis) == 0x10);
GAME_STATIC_ASSERT(entity_link_info_offset_column_offset, offsetof(entityLinkInfo_t, relAxis.origin) == 0x34);
GAME_STATIC_ASSERT(entity_link_info_parent_rel_axis_offset, offsetof(entityLinkInfo_t, parentRelAxis) == 0x40);
GAME_STATIC_ASSERT(entity_link_info_size, sizeof(entityLinkInfo_t) == 0x70);
GAME_STATIC_ASSERT(gclient_size, sizeof(gclient_t) == 0x4734u);
GAME_STATIC_ASSERT(hudelem_size, sizeof(game_hudElem_t) == 0x88);
GAME_STATIC_ASSERT(hudelem_client_snapshot_size, sizeof(hudElem_t) == 0x7c);
GAME_STATIC_ASSERT(hudelem_x_offset, offsetof(game_hudElem_t, client.x) == 0x04);
GAME_STATIC_ASSERT(hudelem_y_offset, offsetof(game_hudElem_t, client.y) == 0x08);
GAME_STATIC_ASSERT(hudelem_align_x_offset, offsetof(game_hudElem_t, client.alignX) == 0x14);
GAME_STATIC_ASSERT(hudelem_align_y_offset, offsetof(game_hudElem_t, client.alignY) == 0x18);
GAME_STATIC_ASSERT(hudelem_color_offset, offsetof(game_hudElem_t, client.color) == 0x1c);
GAME_STATIC_ASSERT(hudelem_width_offset, offsetof(game_hudElem_t, client.width) == 0x30);
GAME_STATIC_ASSERT(hudelem_height_offset, offsetof(game_hudElem_t, client.height) == 0x34);
GAME_STATIC_ASSERT(hudelem_move_from_x_offset, offsetof(game_hudElem_t, client.moveFromX) == 0x4c);
GAME_STATIC_ASSERT(hudelem_move_from_y_offset, offsetof(game_hudElem_t, client.moveFromY) == 0x50);
GAME_STATIC_ASSERT(hudelem_move_start_time_offset, offsetof(game_hudElem_t, client.moveStartTime) == 0x54);
GAME_STATIC_ASSERT(hudelem_move_time_offset, offsetof(game_hudElem_t, client.moveTime) == 0x58);
GAME_STATIC_ASSERT(hudelem_text_offset, offsetof(game_hudElem_t, client.text) == 0x68);
GAME_STATIC_ASSERT(hudelem_sort_offset, offsetof(game_hudElem_t, client.sortKey) == 0x6c);
GAME_STATIC_ASSERT(hudelem_unused_78_offset, offsetof(game_hudElem_t, client.unused78) == 0x78);
GAME_STATIC_ASSERT(hudelem_client_snapshot_unused_78_offset, offsetof(hudElem_t, unused78) == 0x78);
GAME_STATIC_ASSERT(hudelem_client_num_offset, offsetof(game_hudElem_t, clientNum) == 0x7c);
GAME_STATIC_ASSERT(hudelem_archived_offset, offsetof(game_hudElem_t, archived) == 0x84);
GAME_STATIC_ASSERT(gclient_controls_frozen_offset, offsetof(gclient_t, controlsFrozen) == 0x4624);
GAME_STATIC_ASSERT(gclient_look_at_entity_offset, offsetof(gclient_t, lookAtEntity) == 0x46b8);
GAME_STATIC_ASSERT(gclient_cursor_hint_offset, offsetof(gclient_t, ps.serverCursorHint) == 0x5c4);
GAME_STATIC_ASSERT(gclient_cursor_hint_entity_offset, offsetof(gclient_t, ps.cursorHintEntNum) == 0x5f8);
GAME_STATIC_ASSERT(gclient_nonpvs_friendly_info_offset, offsetof(gclient_t, ps.compassFriendInfo) == 0x600);
GAME_STATIC_ASSERT(gclient_nonpvs_tank_info_offset, offsetof(gclient_t, ps.compassTankInfo) == 0x604);
GAME_STATIC_ASSERT(gclient_vehicle_anim_pitch_offset, offsetof(gclient_t, ps.vehicleMotion) == 0x61c);
GAME_STATIC_ASSERT(gclient_spectator_snapshot_angle0_offset, offsetof(gclient_t, spectatorSnapshotAngle0) == 0x4650);
GAME_STATIC_ASSERT(gclient_spectator_snapshot_angle1_offset, offsetof(gclient_t, spectatorSnapshotAngle1) == 0x4654);
GAME_STATIC_ASSERT(gclient_endframe_transient_46a4_offset, offsetof(gclient_t, endFrameTransient46a4) == 0x46a4);
GAME_STATIC_ASSERT(gclient_nonpvs_friendly_client_offset, offsetof(gclient_t, nonpvsFriendlyClient) == 0x46bc);
GAME_STATIC_ASSERT(gclient_ping_end_time_offset, offsetof(gclient_t, pingEndTime) == 0x46c0);
GAME_STATIC_ASSERT(gclient_nonpvs_tank_client_offset, offsetof(gclient_t, nonpvsTankClient) == 0x46c4);
GAME_STATIC_ASSERT(gclient_weapon_previous_view_angles_offset, offsetof(gclient_t, weaponPreviousViewAngles) == 0x46d4);
GAME_STATIC_ASSERT(gclient_weapon_sway_offsets_offset, offsetof(gclient_t, weaponSwayOffsets) == 0x46e0);
GAME_STATIC_ASSERT(gclient_weapon_sway_angles_offset, offsetof(gclient_t, weaponSwayAngles) == 0x46ec);
GAME_STATIC_ASSERT(gclient_weapon_move_offset_offset, offsetof(gclient_t, weaponMoveOffset) == 0x46f8);
GAME_STATIC_ASSERT(gclient_weapon_idle_scale_offset, offsetof(gclient_t, weaponIdleScale) == 0x4704);
GAME_STATIC_ASSERT(gclient_weapon_recoil_angles_offset, offsetof(gclient_t, weaponRecoilAngles) == 0x4708);
GAME_STATIC_ASSERT(gclient_fire_recoil_velocity_offset, offsetof(gclient_t, fireRecoilVelocity) == 0x4714);
GAME_STATIC_ASSERT(gclient_weapon_recoil_state_offset, offsetof(gclient_t, weaponRecoilState) == 0x471c);
GAME_STATIC_ASSERT(gclient_flame_damage_time_offset, offsetof(gclient_t, flameDamageTime) == 0x4720);
GAME_STATIC_ASSERT(gclient_flame_damage_inflictor_offset, offsetof(gclient_t, flameDamageInflictor) == 0x4724);
GAME_STATIC_ASSERT(gclient_vehicle_collision_time_offset, offsetof(gclient_t, lastCollisionDamageTime) == 0x4728);
GAME_STATIC_ASSERT(gclient_vehicle_prone_damage_time_offset, offsetof(gclient_t, vehicleProneDamageTime) == 0x472c);
GAME_STATIC_ASSERT(gclient_connected_state_offset, offsetof(gclient_t, connectedState) == 0x4520);
GAME_STATIC_ASSERT(gclient_current_buttons_offset, offsetof(gclient_t, currentButtons) == 0x462c);
GAME_STATIC_ASSERT(turret_state_in_use_offset, offsetof(turret_state_t, inUse) == 0x000);
GAME_STATIC_ASSERT(turret_state_flags_offset, offsetof(turret_state_t, flags) == 0x004);
GAME_STATIC_ASSERT(turret_state_fire_time_offset, offsetof(turret_state_t, fireTimeRemaining) == 0x008);
GAME_STATIC_ASSERT(turret_state_top_arc_offset, offsetof(turret_state_t, topArc) == 0x00c);
GAME_STATIC_ASSERT(turret_state_rest_pitch_offset, offsetof(turret_state_t, restPitch) == 0x01c);
GAME_STATIC_ASSERT(turret_state_use_mode_offset, offsetof(turret_state_t, useMode) == 0x020);
GAME_STATIC_ASSERT(turret_state_stop_use_event_type_offset, offsetof(turret_state_t, stopUseEventType) == 0x024);
GAME_STATIC_ASSERT(turret_state_fire_sound_time_offset, offsetof(turret_state_t, fireSoundTime) == 0x028);
GAME_STATIC_ASSERT(turret_state_stop_use_origin_offset, offsetof(turret_state_t, stopUseOrigin) == 0x02c);
GAME_STATIC_ASSERT(turret_state_rest_pitch_clamp_offset, offsetof(turret_state_t, restPitchClamp) == 0x038);
GAME_STATIC_ASSERT(turret_state_sustained_fire_loop_sound_offset, offsetof(turret_state_t, sustainedFireLoopSound) == 0x03c);
GAME_STATIC_ASSERT(turret_state_sustained_fire_stop_sound_offset, offsetof(turret_state_t, sustainedFireStopSound) == 0x03d);
GAME_STATIC_ASSERT(turret_state_heat_offset, offsetof(turret_state_t, heat) == 0x040);
GAME_STATIC_ASSERT(turret_state_size, sizeof(turret_state_t) == 0x48);
GAME_STATIC_ASSERT(gentity_mover_reached_offset, offsetof(gentity_t, moverReached) == 0x214);
GAME_STATIC_ASSERT(gentity_mover_blocked_offset, offsetof(gentity_t, moverBlocked) == 0x218);
GAME_STATIC_ASSERT(gentity_touch_offset, offsetof(gentity_t, touch) == 0x21c);
GAME_STATIC_ASSERT(gentity_use_offset, offsetof(gentity_t, use) == 0x220);
GAME_STATIC_ASSERT(gentity_controller_offset, offsetof(gentity_t, controller) == 0x230);
GAME_STATIC_ASSERT(gentity_entity_state_offset, offsetof(gentity_t, s) == 0x000);
GAME_STATIC_ASSERT(gentity_current_origin_offset, offsetof(gentity_t, currentOrigin) == 0x13c);
GAME_STATIC_ASSERT(gentity_current_angles_offset, offsetof(gentity_t, currentAngles) == 0x148);
GAME_STATIC_ASSERT(gentity_pass_entity_num_offset, offsetof(gentity_t, passEntityNum) == 0x154);
GAME_STATIC_ASSERT(gentity_client_offset, offsetof(gentity_t, client) == 0x160);
GAME_STATIC_ASSERT(entity_type_player_corpse_value, ET_PLAYER_CORPSE == 2);
GAME_STATIC_ASSERT(entity_type_turret_value, ET_TURRET == 11);
GAME_STATIC_ASSERT(entity_type_vehicle_value, ET_VEHICLE == 12);
GAME_STATIC_ASSERT(gentity_sv_flags_offset, offsetof(gentity_t, svFlags) == 0x0f8);
GAME_STATIC_ASSERT(gentity_contents_offset, offsetof(gentity_t, contents) == 0x104);
GAME_STATIC_ASSERT(gentity_script_contents_offset, offsetof(gentity_t, scriptContents) == 0x120);
GAME_STATIC_ASSERT(gentity_client_spawn_state_byte_offset, offsetof(gentity_t, clientSpawnStateByte) == 0x17b);
GAME_STATIC_ASSERT(gentity_client_spawn_reset_byte_offset, offsetof(gentity_t, clientSpawnResetByte) == 0x17c);
GAME_STATIC_ASSERT(gentity_take_damage_offset, offsetof(gentity_t, takeDamage) == 0x17d);
GAME_STATIC_ASSERT(gentity_active_state_offset, offsetof(gentity_t, activeState) == 0x17e);
GAME_STATIC_ASSERT(gentity_reserved_state_byte_offset, offsetof(gentity_t, reserved_17f) == 0x17f);
GAME_STATIC_ASSERT(gentity_vehicle_offset, offsetof(gentity_t, vehicle) == 0x164);
GAME_STATIC_ASSERT(gentity_clipmask_offset, offsetof(gentity_t, clipmask) == 0x1a0);
GAME_STATIC_ASSERT(gentity_ref_offset, offsetof(gentity_t, entityRef) == 0x1a8);
GAME_STATIC_ASSERT(gentity_target_location_next_offset, offsetof(gentity_t, targetLocationNext) == 0x1ac);
GAME_STATIC_ASSERT(gentity_event_time2_offset, offsetof(gentity_t, eventTime2) == 0x158);
GAME_STATIC_ASSERT(gentity_worldspawn_spawnflags_scratch_offset, offsetof(gentity_t, worldspawnSpawnflagsScratch) == 0x15c);
GAME_STATIC_ASSERT(gentity_door_sound_opening_offset, offsetof(gentity_t, doorSoundOpening) == 0x16f);
GAME_STATIC_ASSERT(gentity_linked_offset, offsetof(gentity_t, linked) == 0x16c);
GAME_STATIC_ASSERT(gentity_mover_state_offset, offsetof(gentity_t, moverState) == 0x180);
GAME_STATIC_ASSERT(gentity_model_index_offset, offsetof(gentity_t, modelIndex) == 0x181);
GAME_STATIC_ASSERT(gentity_script_classname_offset, offsetof(gentity_t, scriptClassname) == 0x184);
GAME_STATIC_ASSERT(gentity_spawnflags_offset, offsetof(gentity_t, spawnflags) == 0x188);
GAME_STATIC_ASSERT(gentity_flags_offset, offsetof(gentity_t, flags) == 0x18c);
GAME_STATIC_ASSERT(gentity_last_think_time_offset, offsetof(gentity_t, lastThinkTime) == 0x190);
GAME_STATIC_ASSERT(gentity_skip_type_dispatch_offset, offsetof(gentity_t, skipTypeDispatch) == 0x194);
GAME_STATIC_ASSERT(gentity_unlink_on_timeout_offset, offsetof(gentity_t, unlinkOnTimeout) == 0x198);
GAME_STATIC_ASSERT(gentity_bounce_factor_offset, offsetof(gentity_t, bounceFactor) == 0x19c);
GAME_STATIC_ASSERT(gentity_mover_pos1_offset, offsetof(gentity_t, moverPos1) == 0x1b4);
GAME_STATIC_ASSERT(gentity_mover_pos2_offset, offsetof(gentity_t, moverPos2) == 0x1c0);
GAME_STATIC_ASSERT(gentity_dyna_sink_end_time_offset, offsetof(gentity_t, dynaSinkEndTime) == 0x1dc);
GAME_STATIC_ASSERT(gentity_target_location_message_offset, offsetof(gentity_t, targetLocationMessage) == 0x1d8);
GAME_STATIC_ASSERT(gentity_door_yaw_offset, offsetof(gentity_t, doorYawOffset) == 0x1e0);
GAME_STATIC_ASSERT(gentity_target_offset, offsetof(gentity_t, target) == 0x1e4);
GAME_STATIC_ASSERT(gentity_targetname_offset, offsetof(gentity_t, targetname) == 0x1e6);
GAME_STATIC_ASSERT(gentity_team_name_offset, offsetof(gentity_t, teamName) == 0x1e8);
GAME_STATIC_ASSERT(gentity_max_speed_offset, offsetof(gentity_t, maxSpeed) == 0x1f0);
GAME_STATIC_ASSERT(gentity_door_alt_speed_offset, offsetof(gentity_t, doorAltSpeed) == 0x1f4);
GAME_STATIC_ASSERT(gentity_mover_dir_offset, offsetof(gentity_t, moverDir) == 0x1f8);
GAME_STATIC_ASSERT(gentity_mover_duration_offset, offsetof(gentity_t, moverDuration) == 0x204);
GAME_STATIC_ASSERT(gentity_mover_alt_duration_offset, offsetof(gentity_t, moverAltDuration) == 0x208);
GAME_STATIC_ASSERT(gentity_enemy_scan_radius_offset, offsetof(gentity_t, enemyScanRadius) == 0x280);
GAME_STATIC_ASSERT(gentity_team_chain_offset, offsetof(gentity_t, teamChain) == 0x270);
GAME_STATIC_ASSERT(gentity_team_master_offset, offsetof(gentity_t, teamMaster) == 0x274);
GAME_STATIC_ASSERT(gentity_trigger_activator_offset, offsetof(gentity_t, triggerActivator) == 0x26c);
GAME_STATIC_ASSERT(gentity_concussive_fx_end_time_offset, offsetof(gentity_t, concussiveFxEndTime) == 0x284);
GAME_STATIC_ASSERT(gentity_owner_icon_delay_offset, offsetof(gentity_t, ownerIconDelaySeconds) == 0x28c);
GAME_STATIC_ASSERT(gentity_script_mover_angle_target_offset, offsetof(gentity_t, scriptMoverAngleTarget) == 0x29c);
GAME_STATIC_ASSERT(gentity_item_info_offset, offsetof(gentity_t, itemInfo) == 0x2a8);
GAME_STATIC_ASSERT(gentity_vehicle_primary_yaw_clamp_offset, offsetof(gentity_t, vehiclePrimaryYawClamp) == 0x2bc);
GAME_STATIC_ASSERT(gentity_script_vehicle_primary_pitch_clamp_offset, offsetof(gentity_t, scriptVehiclePrimaryPitchClamp) == 0x2c0);
GAME_STATIC_ASSERT(gentity_mission_level_offset, offsetof(gentity_t, missionLevel) == 0x2c4);
GAME_STATIC_ASSERT(gentity_start_size_offset, offsetof(gentity_t, startSize) == 0x2cc);
GAME_STATIC_ASSERT(gentity_end_size_offset, offsetof(gentity_t, endSize) == 0x2d0);
GAME_STATIC_ASSERT(gentity_spawn_item_offset, offsetof(gentity_t, spawnItem) == 0x2d8);
GAME_STATIC_ASSERT(gentity_script_track_offset, offsetof(gentity_t, scriptTrack) == 0x2ec);
GAME_STATIC_ASSERT(gentity_link_info_offset, offsetof(gentity_t, linkInfo) == 0x2f4);
GAME_STATIC_ASSERT(gentity_vehicle_spawn_name_offset, offsetof(gentity_t, vehicleSpawnName) == 0x314);
GAME_STATIC_ASSERT(gentity_vehicle_primary_disabled_offset, offsetof(gentity_t, vehiclePrimaryDisabled) == 0x318);
GAME_STATIC_ASSERT(gentity_vehicle_owner_offset, offsetof(gentity_t, vehicleOwner) == 0x31c);
GAME_STATIC_ASSERT(gentity_vehicle_reenter_time_offset, offsetof(gentity_t, vehicleReenterTime) == 0x320);
GAME_STATIC_ASSERT(gentity_last_vehicle_entity_num_offset, offsetof(gentity_t, lastVehicleEntityNum) == 0x324);
GAME_STATIC_ASSERT(gentity_next_free_offset, offsetof(gentity_t, nextFree) == 0x328);
GAME_STATIC_ASSERT(gentity_missile_fuse_time_offset, offsetof(gentity_t, missileFuseTime) == 0x32c);
GAME_STATIC_ASSERT(gentity_parent_entity_num_offset, offsetof(gentity_t, parentEntityNum) == 0x330);
GAME_STATIC_ASSERT(gentity_nextthink_offset, offsetof(gentity_t, nextthink) == 0x20c);
GAME_STATIC_ASSERT(gentity_think_offset, offsetof(gentity_t, think) == 0x210);
GAME_STATIC_ASSERT(gentity_die_offset, offsetof(gentity_t, die) == 0x228);
GAME_STATIC_ASSERT(gentity_health_offset, offsetof(gentity_t, health) == 0x240);
GAME_STATIC_ASSERT(gentity_damage_offset, offsetof(gentity_t, damage) == 0x248);
GAME_STATIC_ASSERT(gentity_splash_damage_offset, offsetof(gentity_t, splashDamage) == 0x24c);
GAME_STATIC_ASSERT(gentity_splash_min_damage_offset, offsetof(gentity_t, splashMinDamage) == 0x250);
GAME_STATIC_ASSERT(gentity_splash_radius_offset, offsetof(gentity_t, splashRadius) == 0x254);
GAME_STATIC_ASSERT(gentity_method_of_death_offset, offsetof(gentity_t, methodOfDeath) == 0x258);
GAME_STATIC_ASSERT(gentity_splash_method_of_death_offset, offsetof(gentity_t, splashMethodOfDeath) == 0x25c);
GAME_STATIC_ASSERT(gentity_item_count_offset, offsetof(gentity_t, itemCount) == 0x260);
GAME_STATIC_ASSERT(gentity_dropped_clip_count_offset, offsetof(gentity_t, droppedClipCount) == 0x2dc);
GAME_STATIC_ASSERT(gentity_dobj_dirty_offset, offsetof(gentity_t, dobjDirty) == 0x348);
GAME_STATIC_ASSERT(gentity_size, sizeof(gentity_t) == 0x34cu);
GAME_I386_LAYOUT_ASSERT(gitem_classname_offset, offsetof(gitem_t, classname) == 0x000);
GAME_I386_LAYOUT_ASSERT(gitem_pickup_sound_offset, offsetof(gitem_t, pickupSound) == 0x004);
GAME_I386_LAYOUT_ASSERT(gitem_world_model_offset, offsetof(gitem_t, worldModel) == 0x008);
GAME_I386_LAYOUT_ASSERT(gitem_icon_model_offset, offsetof(gitem_t, iconModel) == 0x00c);
GAME_I386_LAYOUT_ASSERT(gitem_hud_icon_offset, offsetof(gitem_t, hudIcon) == 0x010);
GAME_I386_LAYOUT_ASSERT(gitem_ammo_icon_offset, offsetof(gitem_t, ammoIcon) == 0x014);
GAME_I386_LAYOUT_ASSERT(gitem_pickup_name_offset, offsetof(gitem_t, pickupName) == 0x018);
GAME_I386_LAYOUT_ASSERT(gitem_quantity_offset, offsetof(gitem_t, quantity) == 0x01c);
GAME_I386_LAYOUT_ASSERT(gitem_type_offset, offsetof(gitem_t, type) == 0x020);
GAME_I386_LAYOUT_ASSERT(gitem_weapon_offset, offsetof(gitem_t, weapon) == 0x024);
GAME_I386_LAYOUT_ASSERT(gitem_ammo_index_offset, offsetof(gitem_t, ammoIndex) == 0x028);
GAME_I386_LAYOUT_ASSERT(gitem_clip_index_offset, offsetof(gitem_t, clipIndex) == 0x02c);
GAME_I386_LAYOUT_ASSERT(gitem_size, sizeof(gitem_t) == 0x30u);
GAME_STATIC_ASSERT(pmove_command_offset, offsetof(pmove_t, command) == 0x004);
GAME_STATIC_ASSERT(pmove_command_time_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, commandTime) == 0x004);
GAME_STATIC_ASSERT(pmove_buttons_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, buttons) == 0x008);
GAME_STATIC_ASSERT(pmove_wbuttons_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, wbuttons) == 0x009);
GAME_STATIC_ASSERT(pmove_weapon_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, weapon) == 0x00a);
GAME_STATIC_ASSERT(pmove_usercmd_angles_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, angles) == 0x00c);
GAME_STATIC_ASSERT(pmove_usercmd_angle_yaw_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, angles[1]) == 0x010);
GAME_STATIC_ASSERT(pmove_usercmd_angle_roll_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, angles[2]) == 0x014);
GAME_STATIC_ASSERT(pmove_forwardmove_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, forwardmove) == 0x018);
GAME_STATIC_ASSERT(pmove_rightmove_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, rightmove) == 0x019);
GAME_STATIC_ASSERT(pmove_upmove_offset, offsetof(pmove_t, command) + offsetof(usercmd_t, upmove) == 0x01a);
GAME_STATIC_ASSERT(usercmd_size, sizeof(usercmd_t) == 0x018);
GAME_STATIC_ASSERT(pmove_old_command_offset, offsetof(pmove_t, oldCommand) == 0x01c);
GAME_STATIC_ASSERT(pmove_old_usercmd_angles_offset, offsetof(pmove_t, oldCommand.angles) == 0x024);
GAME_STATIC_ASSERT(pmove_old_forwardmove_offset, offsetof(pmove_t, oldCommand.forwardmove) == 0x030);
GAME_STATIC_ASSERT(pmove_trace_mask_offset, offsetof(pmove_t, traceMask) == 0x034);
GAME_STATIC_ASSERT(pmove_debug_move_offset, offsetof(pmove_t, debugMove) == 0x038);
GAME_STATIC_ASSERT(pmove_view_clamp_target_offset, offsetof(pmove_t, viewClampTargetAngles) == 0x03c);
GAME_STATIC_ASSERT(pmove_view_clamp_delta_offset, offsetof(pmove_t, viewClampMaxDeltas) == 0x048);
GAME_STATIC_ASSERT(pmove_numtouch_offset, offsetof(pmove_t, numtouch) == 0x054);
GAME_STATIC_ASSERT(pmove_impact_entity_nums_offset, offsetof(pmove_t, impactEntityNums) == 0x058);
GAME_STATIC_ASSERT(pmove_impact_entity_nums_size, sizeof(((pmove_t *)0)->impactEntityNums) == 0x080);
GAME_STATIC_ASSERT(pmove_trace_offset, offsetof(pmove_t, trace) == 0x104);
GAME_STATIC_ASSERT(pmove_mins_offset, offsetof(pmove_t, mins) == 0x0d8);
GAME_STATIC_ASSERT(pmove_maxs_offset, offsetof(pmove_t, maxs) == 0x0e4);
GAME_STATIC_ASSERT(pmove_watertype_offset, offsetof(pmove_t, watertype) == 0x0f0);
GAME_STATIC_ASSERT(pmove_waterlevel_offset, offsetof(pmove_t, waterlevel) == 0x0f1);
GAME_STATIC_ASSERT(pmove_horizontal_speed_offset, offsetof(pmove_t, horizontalSpeed) == 0x0f4);
GAME_STATIC_ASSERT(pmove_weapon_animscript_enabled_offset, offsetof(pmove_t, weaponAnimscriptEnabled) == 0x100);
GAME_STATIC_ASSERT(pmove_point_contents_offset, offsetof(pmove_t, pointContents) == 0x110);
GAME_STATIC_ASSERT(pmove_entity_type_offset, offsetof(pmove_t, entityType) == 0x114);
GAME_STATIC_ASSERT(pmove_ads_input_blocked_offset, offsetof(pmove_t, adsInputBlocked) == 0x118);
GAME_STATIC_ASSERT(pmove_size, sizeof(pmove_t) == 0x11c);

#endif

extern gentity_t g_entities[MAX_GENTITIES];
extern gclient_t g_clients[MAX_CLIENTS];

gentity_t *script_object_to_gentity(uint32_t scriptObject);
gentity_t *script_object_to_player(uint32_t scriptObject);

#endif
