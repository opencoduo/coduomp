#ifndef QCOMMON_PLAYER_STATE_TYPES_H
#define QCOMMON_PLAYER_STATE_TYPES_H

#include "cursor_hint_types.h"
#include "q_shared_types.h"
#include "vehicle_types.h"
#include "weapon_types.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_PS_EVENTS = 4,
    PLAYERSTATE_STAT_COUNT = 6,
    PLAYERSTATE_OBJECTIVE_COUNT = 16,
    PLAYERSTATE_HUD_ELEM_COUNT = 63
};

/* Retain the Quake-derived indexed stats representation used by the original
 * player-state codec while naming every CoD:UO slot used by game code. */
typedef enum playerStateStatIndex_e {
    STAT_HEALTH = 0,
    STAT_DEAD_YAW = 1,
    STAT_MAX_HEALTH = 2,
    STAT_IDENT_CLIENT_NUM = 3,
    STAT_IDENT_CLIENT_HEALTH = 4,
    STAT_SPAWN_COUNT = 5
} playerStateStatIndex_t;

typedef enum objectiveState_e {
    OBJECTIVE_STATE_EMPTY = 0,
    OBJECTIVE_STATE_ACTIVE = 1,
    OBJECTIVE_STATE_INVISIBLE = 2,
    OBJECTIVE_STATE_CURRENT = 4
} objectiveState_t;

/* The Windows executable and cgame/game modules, Linux engine/game module,
 * and their player-state delta tables all use this 0x1c-byte objective row. */
typedef struct objective_s {
    objectiveState_t state;
    vec3_t origin;
    int32_t entityNum;
    int32_t teamNum;
    int32_t icon;
} objective_t;

typedef enum hudElemType_e {
    HE_TYPE_NONE = 0,
    HE_TYPE_TEXT = 1,
    HE_TYPE_VALUE = 2,
    HE_TYPE_SHADER = 3,
    HE_TYPE_TIMER = 4,
    HE_TYPE_TIMER_UP = 5,
    HE_TYPE_TENTHS_TIMER = 6,
    HE_TYPE_TENTHS_TIMER_UP = 7,
    HE_TYPE_CLOCK = 8,
    HE_TYPE_CLOCK_UP = 9
} hudElemType_t;

/* The Windows game module writes the four color bytes in red, green, blue,
 * alpha order. The engine HUD delta table addresses the same storage through
 * the packed rgba word. */
typedef union hudelem_color_u {
    uint32_t rgba;
    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;
    } components;
} hudelem_color_t;

/* Shared axis-relative HUD alignment. START/CENTER/END mean left/center/right
 * for alignX and top/middle/bottom for alignY. */
typedef int32_t hudElemAlign_t;
enum hudElemAlign_e {
    HUDELEM_ALIGN_START = 0,
    HUDELEM_ALIGN_CENTER = 1,
    HUDELEM_ALIGN_END = 2
};

/* Client-visible HUD record embedded twice in playerState_t. The game module's
 * 0x88-byte server HUD object embeds this complete 0x7c-byte transmitted
 * prefix and then adds three server-only routing words. */
typedef struct hudElem_s {
    hudElemType_t type;                  /* +0x00 */
    int32_t x;                           /* +0x04 */
    int32_t y;                           /* +0x08 */
    float fontScale;                     /* +0x0c */
    int32_t font;                        /* +0x10 */
    hudElemAlign_t alignX;               /* +0x14 */
    hudElemAlign_t alignY;               /* +0x18 */
    hudelem_color_t color;               /* +0x1c */
    hudelem_color_t fromColor;           /* +0x20 */
    int32_t fadeStartTime;               /* +0x24 */
    int32_t fadeTime;                    /* +0x28 */
    int32_t label;                       /* +0x2c */
    int32_t width;                       /* +0x30 */
    int32_t height;                      /* +0x34 */
    int32_t materialIndex;               /* +0x38 */
    int32_t scaleFromWidth;              /* +0x3c */
    int32_t scaleFromHeight;             /* +0x40 */
    int32_t scaleStartTime;              /* +0x44 */
    int32_t scaleTime;                   /* +0x48 */
    int32_t moveFromX;                   /* +0x4c */
    int32_t moveFromY;                   /* +0x50 */
    int32_t moveStartTime;               /* +0x54 */
    int32_t moveTime;                    /* +0x58 */
    int32_t timerValue;                  /* +0x5c */
    int32_t rotationPeriodMs;            /* +0x60 */
    float value;                         /* +0x64 */
    int32_t text;                        /* +0x68 */
    float sortKey;                       /* +0x6c */
    float shaderRightTexcoord;           /* +0x70 */
    float shaderBottomTexcoord;          /* +0x74 */
    uint32_t unused78;                   /* +0x78 */
} hudElem_t;

typedef enum pmType_e {
    PM_TYPE_NORMAL = 0,
    PM_TYPE_LINKED = 1,
    PM_TYPE_NOCLIP = 2,
    PM_TYPE_UFO = 3,
    PM_TYPE_SPECTATOR = 4,
    PM_TYPE_INTERMISSION = 5,
    PM_TYPE_DEAD = 6,
    PM_TYPE_LINKED_DEAD = 7
} pmType_t;

/* Shared playerState_t::playerStateFlags domain.  The Windows cgame/game and
 * Linux game movement bodies test, set, and clear these same bits in the dword
 * at +0x00c.  The names retain the cgame PMF spellings already established by
 * the matching PM_* behavior; the server recovery's former PM_STANCE aliases
 * described the same physical flags. */
enum playerStateFlags_e {
    PMF_PRONE = 0x00000001u,
    PMF_DUCKED = 0x00000002u,
    PMF_JUMP_HELD = 0x00000008u,
    PMF_LADDER = 0x00000010u,
    PMF_ADS = 0x00000020u,
    PMF_BACKPEDAL = 0x00000040u,
    PMF_WALKING = 0x00000080u,
    PMF_LAND_STUN = 0x00000100u,
    PMF_NO_GROUNDFRICTION = 0x00000200u,
    PMF_PRONE_MOVEMENT_OVERRIDE = 0x00000400u,
    PMF_RESPAWNED = 0x00000800u,
    PMF_MELEE_HELD = 0x00001000u,
    PMF_WALLJUMP = 0x00002000u,
    PMF_FOLLOW = 0x00004000u,
    PMF_PRONE_BLOCKED = 0x00008000u,
    PMF_SPRINTING = 0x00010000u,
    PMF_FATIGUED = 0x00020000u,
    PSF_FOLLOWING = 0x00040000u,
    PSF_ACTIVE_PLAYER = 0x00080000u,
    /* Either state publishes a player entity and gives cgame a local/followed
     * first-person presentation. */
    PSF_PLAYER_ENTITY_MASK = PSF_FOLLOWING | PSF_ACTIVE_PLAYER,
    PMF_WEAPON_DISABLED = 0x00400000u,

    PMF_LADDER_TIMER_BLOCK_MASK =
        PMF_LAND_STUN | PMF_NO_GROUNDFRICTION,
    PMF_ALL_TIMES =
        PMF_LAND_STUN | PMF_NO_GROUNDFRICTION | PMF_WALLJUMP
};

/* Complete pointer-free player-state network/VM record. CoDUOMP.exe and
 * coduo_lnxded carry matching 114-entry delta tables, and the Windows cgame,
 * Windows game, and Linux game module consumers independently establish the
 * same field offsets and 0x4504-byte extent. */
typedef struct playerState_s {
    int32_t commandTime;                 /* +0x000 */
    pmType_t pmType;                     /* +0x004 */
    int32_t bobCycle;                    /* +0x008 */
    uint32_t playerStateFlags;           /* +0x00c */
    int32_t pmTime;                      /* +0x010 */
    vec3_t psOrigin;                     /* +0x014 */
    vec3_t velocity;                     /* +0x020 */
    int32_t weaponTime;                  /* +0x02c */
    int32_t weaponDelay;                 /* +0x030 */
    int32_t grenadeTimeLeft;             /* +0x034 */
    int32_t foliageSoundTime;            /* +0x038 */
    int32_t fatigueSoundTime;            /* +0x03c */
    int32_t gravity;                     /* +0x040 */
    float leanFraction;                  /* +0x044 */
    int32_t speed;                       /* +0x048 */
    int32_t deltaAngles[3];              /* +0x04c */
    int32_t groundEntityNum;             /* +0x058 */
    vec3_t ladderNormal;                 /* +0x05c */
    int32_t lastJumpCommandTime;         /* +0x068 */
    float jumpOriginZ;                   /* +0x06c */
    int32_t legsTimer;                   /* +0x070 */
    int32_t legsAnim;                    /* +0x074 */
    int32_t torsoTimer;                  /* +0x078 */
    int32_t torsoAnim;                   /* +0x07c */
    int32_t movementDir;                 /* +0x080 */
    uint32_t entityStateFlags;           /* +0x084 */
    int32_t eventIndex;                  /* +0x088 */
    int32_t events[MAX_PS_EVENTS];       /* +0x08c */
    int32_t eventParms[MAX_PS_EVENTS];   /* +0x09c */
    int32_t lastEventIndex;              /* +0x0ac */
    uint8_t unused0B0[36];               /* +0x0b0 */
    int32_t psClientNum;                 /* +0x0d4 */
    int32_t currentWeapon;               /* +0x0d8 */
    weaponState_t weaponState;           /* +0x0dc */
    float adsFraction;                   /* +0x0e0 */
    int32_t viewModelIndex;              /* +0x0e4 */
    vec3_t viewAngles;                   /* +0x0e8 */
    int32_t viewHeightTarget;            /* +0x0f4 */
    float viewHeightCurrent;             /* +0x0f8 */
    int32_t viewHeightLerpTime;          /* +0x0fc */
    int32_t viewHeightLerpTarget;        /* +0x100 */
    qboolean viewHeightLerpDown;         /* +0x104 */
    float viewHeightLerpPosAdj;          /* +0x108 */
    int32_t damageEvent;                 /* +0x10c */
    int32_t damageYaw;                   /* +0x110 */
    int32_t damagePitch;                 /* +0x114 */
    int32_t damageCount;                 /* +0x118 */
    int32_t stats[PLAYERSTATE_STAT_COUNT]; /* +0x11c */
    int32_t ammo[MAX_AMMO_TYPES];        /* +0x134 */
    int32_t clips[MAX_AMMO_TYPES];       /* +0x334 */
    uint32_t weaponBits[WEAPON_BITSET_WORD_COUNT]; /* +0x534 */
    int8_t weaponSlots[WEAPSLOT_COUNT];  /* +0x544 */
    uint32_t weaponRechamberBits[WEAPON_BITSET_WORD_COUNT]; /* +0x54c */
    vec3_t playerMins;                   /* +0x55c */
    vec3_t playerMaxs;                   /* +0x568 */
    int32_t proneViewHeight;             /* +0x574 */
    int32_t crouchViewHeight;            /* +0x578 */
    int32_t standViewHeight;             /* +0x57c */
    int32_t deadViewHeight;              /* +0x580 */
    float walkSpeedScale;                /* +0x584 */
    float runSpeedScale;                 /* +0x588 */
    float sprintSpeedScale;              /* +0x58c */
    float proneSpeedScale;               /* +0x590 */
    float crouchSpeedScale;              /* +0x594 */
    float strafeSpeedScale;              /* +0x598 */
    float backSpeedScale;                /* +0x59c */
    float leanSpeedScale;                /* +0x5a0 */
    float proneDirection;                /* +0x5a4 */
    float proneDirectionPitch;           /* +0x5a8 */
    float proneTorsoPitch;               /* +0x5ac */
    float fatigueScale;                  /* +0x5b0 */
    int32_t lastSprintTime;              /* +0x5b4 */
    int32_t viewLocked;                  /* +0x5b8 */
    int32_t viewLockedEntityNum;         /* +0x5bc */
    float friction;                      /* +0x5c0 */
    cursorHint_t serverCursorHint;       /* +0x5c4 */
    int32_t serverCursorHintVal;         /* +0x5c8 */
    int32_t serverCursorHintString;      /* +0x5cc */
    uint8_t unused5D0[28];               /* +0x5d0 */
    uint32_t cursorHintFlags;            /* +0x5ec */
    uint8_t unused5F0[8];                /* +0x5f0 */
    uint16_t cursorHintEntNum;           /* +0x5f8 */
    uint8_t unused5FA[6];                /* +0x5fa */
    int32_t compassFriendInfo;           /* +0x600 */
    int32_t compassTankInfo;             /* +0x604 */
    float torsoHeight;                   /* +0x608 */
    float torsoPitch;                    /* +0x60c */
    float waistPitch;                    /* +0x610 */
    int32_t vehiclePosition;             /* +0x614 */
    vehicle_type_t vehicleType;          /* +0x618 */
    int32_t vehicleMotion;               /* +0x61c */
    int32_t oldEventIndex;               /* +0x620 */
    uint32_t weaponAnim;                 /* +0x624 */
    float aimSpreadScale;                /* +0x628 */
    union {
        vec3_t externalVelocity;
        struct {
            int32_t index;
            int32_t time;
            int32_t duration;
        } shellshock;
    } motionState;                       /* +0x62c */
    objective_t objectives[PLAYERSTATE_OBJECTIVE_COUNT]; /* +0x638 */
    hudElem_t hudCurrent[PLAYERSTATE_HUD_ELEM_COUNT]; /* +0x7f8 */
    hudElem_t hudArchival[PLAYERSTATE_HUD_ELEM_COUNT];/* +0x267c */
    int32_t deltaTime;                   /* +0x4500 */
} playerState_t;

#define PLAYER_STATE_LAYOUT_ASSERT(name_, expression_) \
    typedef char name_[(expression_) ? 1 : -1]

PLAYER_STATE_LAYOUT_ASSERT(q_player_state_objective_size,
                           sizeof(objective_t) == 0x1c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_color_size,
                           sizeof(hudelem_color_t) == 0x04);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_align_size,
                           sizeof(hudElemAlign_t) == 0x04);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_size,
                           sizeof(hudElem_t) == 0x7c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_align_x_offset,
                           offsetof(hudElem_t, alignX) == 0x14);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_align_y_offset,
                           offsetof(hudElem_t, alignY) == 0x18);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_color_offset,
                           offsetof(hudElem_t, color) == 0x1c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_timer_offset,
                           offsetof(hudElem_t, timerValue) == 0x5c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_sort_offset,
                           offsetof(hudElem_t, sortKey) == 0x6c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_event_parms_offset,
                           offsetof(playerState_t, eventParms) == 0x09c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_stats_offset,
                           offsetof(playerState_t, stats) == 0x11c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_weapon_slots_offset,
                           offsetof(playerState_t, weaponSlots) == 0x544);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_objectives_offset,
                           offsetof(playerState_t, objectives) == 0x638);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_current_offset,
                           offsetof(playerState_t, hudCurrent) == 0x7f8);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_hud_archival_offset,
                           offsetof(playerState_t, hudArchival) == 0x267c);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_delta_time_offset,
                           offsetof(playerState_t, deltaTime) == 0x4500);
PLAYER_STATE_LAYOUT_ASSERT(q_player_state_size,
                           sizeof(playerState_t) == 0x4504);

#undef PLAYER_STATE_LAYOUT_ASSERT

#endif
