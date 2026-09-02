#ifndef QCOMMON_ENTITY_STATE_TYPES_H
#define QCOMMON_ENTITY_STATE_TYPES_H

#include "cursor_hint_types.h"
#include "qcommon_limits.h"
#include "trajectory_types.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_ENTITY_EVENTS = 4,
    MAX_GENTITIES = 1024,
    GENTITYNUM_BITS = 10,
    ENTITYNUM_WORLD = MAX_GENTITIES - 2,
    ENTITYNUM_NONE = MAX_GENTITIES - 1
};

#define PLAYER_CLONE_LAYOUT_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]

PLAYER_CLONE_LAYOUT_ASSERT(player_clone_entitynum_base_nonnegative,
                           PLAYER_CLONE_ENTITYNUM_BASE >= 0);
PLAYER_CLONE_LAYOUT_ASSERT(player_clone_count_positive,
                           PLAYER_CLONE_COUNT > 0);
PLAYER_CLONE_LAYOUT_ASSERT(player_clone_range_after_clients,
                           PLAYER_CLONE_ENTITYNUM_BASE >= MAX_CLIENTS);
PLAYER_CLONE_LAYOUT_ASSERT(player_clone_entitynum_base_in_domain,
                           (int32_t)PLAYER_CLONE_ENTITYNUM_BASE <=
                               (int32_t)ENTITYNUM_WORLD);
PLAYER_CLONE_LAYOUT_ASSERT(player_clone_count_in_domain,
                           PLAYER_CLONE_COUNT <= ENTITYNUM_WORLD);
PLAYER_CLONE_LAYOUT_ASSERT(
    player_clone_range_fits_entity_domain,
    PLAYER_CLONE_ENTITYNUM_BASE <= ENTITYNUM_WORLD - PLAYER_CLONE_COUNT);

#undef PLAYER_CLONE_LAYOUT_ASSERT

/* The network entity number is a ten-bit domain in every engine/module
 * boundary.  Its final two values are reserved for the world and "no entity";
 * ordinary entity arrays retain all 1,024 addressable slots because both
 * sentinels also have server-side records. */

/* Shared entity-state discriminator.  Keep the storage domain explicitly
 * signed and 32-bit: MSVC's original enum ABI and the recovered cgame, game,
 * and engine fields all use a signed dword, while GCC/Clang otherwise choose
 * an unsigned C enum when every named value is nonnegative. */
typedef int32_t entityType_t;

/*
 * The Windows client and cgame dispatcher, Windows and Linux game modules,
 * and Linux engine use these same numeric values. The names of 13 and 14 are
 * corroborated by the original UO SP entity-name table, where
 * ET_VEHICLE_CORPSE and ET_VEHICLE_COLLMAP immediately follow ET_VEHICLE. The
 * MP-specific removal of the three actor slots shifts that vehicle suffix from
 * SP 16/17 to MP 13/14. MP behavior agrees: cgame renders 13 through the
 * vehicle handler and rejects the non-rendered collision-map type 14, while
 * both game modules retain DObj model state for both values.
 */
enum entityType_e {
    ET_GENERAL = 0,
    ET_PLAYER = 1,
    ET_PLAYER_CORPSE = 2,
    ET_ITEM = 3,
    ET_MISSILE = 4,
    ET_MOVER = 5,
    ET_PORTAL = 6,
    ET_INVISIBLE = 7,
    ET_SCRIPTMOVER = 8,
    ET_SOUND_BLEND = 9,
    ET_LOOPED_FX = 10,
    ET_TURRET = 11,
    ET_VEHICLE = 12,
    ET_VEHICLE_CORPSE = 13,
    ET_VEHICLE_COLLMAP = 14,
    ET_VEHICLE_OWNER_ICON = 15,
    ET_EVENTS = 16
};

/*
 * Shared entity-state flag word. playerState_t carries the same physical word
 * as entityStateFlags before BG_PlayerStateToEntityState copies it into
 * entityState_t.eFlags. Several bits intentionally have type- or mode-specific
 * meanings; aliases below name those distinct proven roles without creating
 * module-local copies of the domain.
 */
enum entityFlags_e {
    EF_DEAD = 0x00000001u,
    EF_NONSOLID_BMODEL = 0x00000002u,
    EF_DOBJ_STATE_CHANGED = 0x00000008u,
    EF_CAPSULE = 0x00000010u,
    EF_STANCE_VALID = EF_CAPSULE,
    EF_CROUCHING = 0x00000020u,
    EF_PRONE = 0x00000040u,
    EF_NODRAW = 0x00000080u,
    EF_ADS = 0x00000100u,
    EF_FIRING = 0x00000200u,
    EF_ANGLE_FIXED_YAW = EF_DEAD,
    EF_ANGLE_TORSO_ONLY_YAW = EF_PRONE,
    EF_ANGLE_TURRET_YAW = EF_ADS,
    EF_ANGLE_TURRET_TIGHT = EF_FIRING,
    EF_TRANSITION_CLONE_DOBJ = 0x00000400u,
    EF_HEADICON_DISCONNECTED = 0x00000800u,
    EF_FORCE_PRONE = 0x00002000u,
    EF_FORCE_CROUCH = 0x00004000u,
    EF_VIEWMODEL_ANGLES_VALID = 0x00008000u,
    EF_HEADICON_TALKING = 0x00020000u,
    EF_ADS_HELD = EF_HEADICON_TALKING,
    EF_IN_VEHICLE = 0x00100000u,
    EF_VEHICLE_ACTIVE = 0x00200000u,
    EF_VEHICLE_ALLOW_WEAPON = 0x00400000u,
    EF_VEHICLE_POPOUT = EF_VEHICLE_ALLOW_WEAPON,
    EF_FORCED_STANCE_MASK = EF_FORCE_PRONE | EF_FORCE_CROUCH,
    EF_ZOOM_FOV_MASK = EF_FORCED_STANCE_MASK,
    EF_RESTRICTED_MASK = EF_FORCED_STANCE_MASK | EF_IN_VEHICLE,
    EF_VEHICLE_STATE_MASK =
        EF_IN_VEHICLE | EF_VEHICLE_ACTIVE | EF_VEHICLE_POPOUT
};

/* The original source shape uses anonymous aggregate views.  The game module
 * intentionally remains a strict C99 build, so mark those C11 aggregates as
 * compiler extensions without changing that target's language mode. */
#if defined(__GNUC__) || defined(__clang__)
#define ENTITY_STATE_UNION __extension__ union
#define ENTITY_STATE_STRUCT __extension__ struct
#else
#define ENTITY_STATE_UNION union
#define ENTITY_STATE_STRUCT struct
#endif

/*
 * Complete pointer-free entity snapshot record.
 *
 * CoDUOMP.exe's two 60-entry entity netfield tables establish the canonical
 * names and offsets.  Windows cgame copies the record as 61 dwords, the
 * Windows and Linux game modules expose it as the first 0xf4 bytes of
 * gentity_t, and the Linux engine copies that prefix into baselines and
 * archived snapshots.  The record is serialized field-by-field rather than
 * copied as raw host memory at the network boundary.
 *
 * Entity types deliberately reuse several transmitted lanes.  Canonical wire
 * names remain available directly; the additional union members name proven
 * type-specific interpretations without creating module-local record copies.
 */
typedef struct entityState_s {
    ENTITY_STATE_UNION {
        int32_t number;                     /* +0x000 */
        uint32_t numberBits;
    };
    entityType_t eType;                    /* +0x004 */
    ENTITY_STATE_UNION {
        uint32_t eFlags;                   /* +0x008 */
        ENTITY_STATE_STRUCT {
            uint8_t eFlagsLowByte;
            uint8_t eFlagsUpperBytes[3];
        };
    };
    ENTITY_STATE_UNION {
        trajectory_t pos;                  /* +0x00c */
        ENTITY_STATE_STRUCT {
            trType_t posTrType;
            int32_t posTrTime;
            int32_t posTrDuration;
            vec3_t origin;
            ENTITY_STATE_UNION {
                vec3_t posTrDelta;
                vec3_t laserDir;
            };
        };
    };
    ENTITY_STATE_UNION {
        trajectory_t apos;                 /* +0x030 */
        ENTITY_STATE_STRUCT {
            trType_t aposTrType;
            int32_t aposTrTime;
            int32_t aposTrDuration;
            vec3_t angles;
            vec3_t aposTrDelta;
        };
    };
    ENTITY_STATE_UNION {
        int32_t time;                       /* +0x054 */
        int32_t iconFadeEndTime;
    };
    ENTITY_STATE_UNION {
        int32_t time2;                      /* +0x058 */
        int32_t vehicleWheelTracePacked;
    };
    ENTITY_STATE_UNION {
        vec3_t origin2;                     /* +0x05c */
        vec3_t loopedFxForward;
        vec3_t effectEndOrigin;
        ENTITY_STATE_STRUCT {
            int32_t vehicleBodyPitchPacked;
            float vehicleSecondaryBaseYaw;
            int32_t vehicleBodyRollPacked;
        };
    };
    ENTITY_STATE_UNION {
        vec3_t angles2;                     /* +0x068 */
        ENTITY_STATE_STRUCT {
            float angles2Pitch;
            ENTITY_STATE_UNION {
                float clientInfoLeanYawPayload;
                float leanAmount;
            };
            float angles2Roll;
        };
        ENTITY_STATE_STRUCT {
            float vehicleBarrelPitch;
            float vehicleTurretYaw;
            float vehicleSecondaryGunPitch;
        };
        ENTITY_STATE_STRUCT {
            float primaryPitch;
            float primaryYaw;
            float gunnerPitch;
        } vehicleTurret;
        ENTITY_STATE_STRUCT {
            float pitch;
            float yaw;
            float pitchCarry;
        } turret;
        ENTITY_STATE_STRUCT {
            float cullDistance;
            float repeatDelayMs;
            float unused70;
        } loopedFx;
        ENTITY_STATE_STRUCT {
            float unused68;
            float leanAmount;
            float unused70;
        } clientInfo;
        ENTITY_STATE_STRUCT {
            float scale;
            float radius;
            float durationMs;
        } earthquake;
        ENTITY_STATE_STRUCT {
            ENTITY_STATE_UNION {
                float loopedFxCullRadius;
                float portalShaderHandleBits;
            };
            ENTITY_STATE_UNION {
                float loopedFxInterval;
                float leanValue;
            };
            int32_t effectShaderHandle;
        };
    };
    ENTITY_STATE_UNION {
        int32_t otherEntityNum;             /* +0x074 */
        int32_t vehicleEntityNum;
        uint32_t vehicleEntityNumBits;
    };
    ENTITY_STATE_UNION {
        int32_t attackerEntityNum;          /* +0x078 */
        int32_t vehicleSlot;
        int32_t compassBlipIndex;
    };
    int32_t groundEntityNum;                /* +0x07c */
    uint32_t constantLight;                 /* +0x080 */
    ENTITY_STATE_UNION {
        int32_t loopSound;                  /* +0x084 */
        int32_t clientSound;
    };
    ENTITY_STATE_UNION {
        int32_t surfType;                   /* +0x088 */
        uint32_t surfTypeBits;
        int32_t vehicleAnimState;
        int32_t vehicleType;
        int32_t stateFilter;
        uint32_t poseType;
    };
    ENTITY_STATE_UNION {
        int32_t index;                      /* +0x08c */
        int32_t itemIndex;
    };
    ENTITY_STATE_UNION {
        int32_t xmodel;                     /* +0x090 */
        int32_t modelIndex;
        int32_t dobjModelIndex;
    };
    ENTITY_STATE_UNION {
        int32_t clientNum;                  /* +0x094 */
        uint32_t clientNumBits;
    };
    ENTITY_STATE_UNION {
        int32_t iHeadIcon;                  /* +0x098 */
        int32_t headIcon;
    };
    ENTITY_STATE_UNION {
        int32_t iHeadIconTeam;              /* +0x09c */
        int32_t headIconTeam;
    };
    int32_t solid;                          /* +0x0a0 */
    ENTITY_STATE_UNION {
        int32_t eventParm;                  /* +0x0a4 */
        uint32_t eventParmBits;
        int32_t tempEffectId;
    };
    ENTITY_STATE_UNION {
        int32_t eventSequence;              /* +0x0a8 */
        int32_t eventCount;
    };
    ENTITY_STATE_UNION {
        int32_t events[MAX_ENTITY_EVENTS];  /* +0x0ac */
        uint32_t eventBits[MAX_ENTITY_EVENTS];
    };
    ENTITY_STATE_UNION {
        int32_t eventParms[MAX_ENTITY_EVENTS]; /* +0x0bc */
        uint32_t eventParmBitsRing[MAX_ENTITY_EVENTS];
    };
    ENTITY_STATE_UNION {
        int32_t weapon;                     /* +0x0cc */
        uint32_t weaponIndex;
    };
    ENTITY_STATE_UNION {
        ENTITY_STATE_STRUCT {
            ENTITY_STATE_UNION {
                int32_t legsAnim;           /* +0x0d0 */
                uint32_t legsAnimWord;
            };
            ENTITY_STATE_UNION {
                int32_t torsoAnim;          /* +0x0d4 */
                uint32_t torsoAnimWord;
            };
        };
        ENTITY_STATE_STRUCT {
            int32_t weaponAnim;
            int32_t followClient;
        } surfaceImpact;
        int32_t anim[2];
    };
    ENTITY_STATE_UNION {
        float leanf;                        /* +0x0d8 */
        float clientInfoLeanFraction;
        float vehicleWheelAngle;
        float iconBaseYaw;
    };
    ENTITY_STATE_UNION {
        int32_t scale;                      /* +0x0dc */
        uint32_t scaleBits;
        int32_t hintStringIndex;
        int32_t loopedFxId;
    };
    ENTITY_STATE_UNION {
        uint32_t dmgFlags;                  /* +0x0e0 */
        cursorHint_t cursorHint;
    };
    ENTITY_STATE_UNION {
        int32_t animMovetype;               /* +0x0e4 */
        int32_t turretOverheatState;
        uint32_t hudTagMask;
    };
    ENTITY_STATE_UNION {
        ENTITY_STATE_STRUCT {
            float fTorsoHeight;             /* +0x0e8 */
            float fTorsoPitch;              /* +0x0ec */
            float fWaistPitch;              /* +0x0f0 */
        };
        vec3_t viewAngles;
        vec3_t entityAngles;
        ENTITY_STATE_STRUCT {
            float vehicleFollowPitchCenter;
            float vehicleFollowPitchDownLimit;
            float vehicleFollowPitchUpLimit;
        };
    };
} entityState_t;

#undef ENTITY_STATE_STRUCT
#undef ENTITY_STATE_UNION

#define ENTITY_STATE_LAYOUT_ASSERT(name_, expression_) \
    typedef char name_[(expression_) ? 1 : -1]

#if defined(_MSC_VER)
#define ENTITY_STATE_ALIGNOF(type_) __alignof(type_)
#elif defined(__GNUC__) || defined(__clang__)
#define ENTITY_STATE_ALIGNOF(type_) __alignof__(type_)
#elif defined(__cplusplus)
#define ENTITY_STATE_ALIGNOF(type_) alignof(type_)
#else
#define ENTITY_STATE_ALIGNOF(type_) _Alignof(type_)
#endif

ENTITY_STATE_LAYOUT_ASSERT(entity_type_size, sizeof(entityType_t) == 4);
ENTITY_STATE_LAYOUT_ASSERT(entity_type_is_signed, (entityType_t)-1 < 0);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_alignment,
                           ENTITY_STATE_ALIGNOF(entityState_t) == 4);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_number_offset,
                           offsetof(entityState_t, number) == 0x000);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_type_offset,
                           offsetof(entityState_t, eType) == 0x004);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_flags_offset,
                           offsetof(entityState_t, eFlags) == 0x008);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_pos_offset,
                           offsetof(entityState_t, pos) == 0x00c);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_apos_offset,
                           offsetof(entityState_t, apos) == 0x030);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_time_offset,
                           offsetof(entityState_t, time) == 0x054);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_time2_offset,
                           offsetof(entityState_t, time2) == 0x058);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_origin2_offset,
                           offsetof(entityState_t, origin2) == 0x05c);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_angles2_offset,
                           offsetof(entityState_t, angles2) == 0x068);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_other_entity_offset,
                           offsetof(entityState_t, otherEntityNum) == 0x074);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_attacker_offset,
                           offsetof(entityState_t, attackerEntityNum) == 0x078);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_ground_entity_offset,
                           offsetof(entityState_t, groundEntityNum) == 0x07c);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_constant_light_offset,
                           offsetof(entityState_t, constantLight) == 0x080);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_loop_sound_offset,
                           offsetof(entityState_t, loopSound) == 0x084);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_surface_type_offset,
                           offsetof(entityState_t, surfType) == 0x088);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_index_offset,
                           offsetof(entityState_t, index) == 0x08c);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_xmodel_offset,
                           offsetof(entityState_t, xmodel) == 0x090);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_client_num_offset,
                           offsetof(entityState_t, clientNum) == 0x094);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_head_icon_offset,
                           offsetof(entityState_t, iHeadIcon) == 0x098);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_head_icon_team_offset,
                           offsetof(entityState_t, iHeadIconTeam) == 0x09c);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_solid_offset,
                           offsetof(entityState_t, solid) == 0x0a0);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_event_parm_offset,
                           offsetof(entityState_t, eventParm) == 0x0a4);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_event_sequence_offset,
                           offsetof(entityState_t, eventSequence) == 0x0a8);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_events_offset,
                           offsetof(entityState_t, events) == 0x0ac);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_event_parms_offset,
                           offsetof(entityState_t, eventParms) == 0x0bc);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_weapon_offset,
                           offsetof(entityState_t, weapon) == 0x0cc);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_legs_anim_offset,
                           offsetof(entityState_t, legsAnim) == 0x0d0);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_torso_anim_offset,
                           offsetof(entityState_t, torsoAnim) == 0x0d4);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_lean_offset,
                           offsetof(entityState_t, leanf) == 0x0d8);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_scale_offset,
                           offsetof(entityState_t, scale) == 0x0dc);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_damage_flags_offset,
                           offsetof(entityState_t, dmgFlags) == 0x0e0);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_anim_move_type_offset,
                           offsetof(entityState_t, animMovetype) == 0x0e4);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_torso_height_offset,
                           offsetof(entityState_t, fTorsoHeight) == 0x0e8);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_torso_pitch_offset,
                           offsetof(entityState_t, fTorsoPitch) == 0x0ec);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_waist_pitch_offset,
                           offsetof(entityState_t, fWaistPitch) == 0x0f0);
ENTITY_STATE_LAYOUT_ASSERT(entity_state_size, sizeof(entityState_t) == 0x0f4);

#undef ENTITY_STATE_ALIGNOF
#undef ENTITY_STATE_LAYOUT_ASSERT

#endif
