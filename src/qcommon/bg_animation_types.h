#ifndef QCOMMON_BG_ANIMATION_TYPES_H
#define QCOMMON_BG_ANIMATION_TYPES_H

#include "client_info_types.h"
#include "xanim_types.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BG_ANIM_MAX_CONDITIONS = ANIM_COND_COUNT,
    BG_ANIM_CONDITION_VALUE_COUNT = 16,
    BG_ANIM_CONDITION_ALIAS_STRING_BUFFER_SIZE = 10000,
    BG_ANIM_SCRIPT_FILE_MAX_BYTES = 99998,
    BG_ANIM_SCRIPT_FILE_BUFFER_SIZE = 100000,
    BG_ANIM_MAX_SCRIPT_COMMANDS = 8,
    BG_ANIM_MAX_LIST_ITEMS = 128,
    BG_ANIM_MAX_GLOBAL_SCRIPTS = 2048,
    BG_ANIM_MAX_ANIMATIONS = 512,
    BGS_CLIENTINFO_COUNT = 64
};

/*
 * One entry in a NULL-name-terminated animation-script token table. The name
 * and lazy hash-cache fields at +0x00/+0x04 and the 8-byte i386 stride agree in
 * Windows cgame BG_IndexForString (0x30001420), Windows game (0x20001410), and
 * Linux game (0x00019d71).
 */
typedef struct bg_indexed_string_s {
    const char *name;                              /* +0x00 on i386 */
    int32_t hash;                                  /* +0x04 on i386 */
} bg_indexed_string_t;

enum {
    BG_INDEXED_STRING_HASH_UNSET = -1
};

/* The initialized condition tables and their matcher branches agree. */
typedef enum bg_anim_condition_mode_e {
    ANIM_CONDMODE_BITMASK = 0,
    ANIM_CONDMODE_EQUAL = 1
} bg_anim_condition_mode_t;

/*
 * Shared player-animation script domains. The initialized indexed-string
 * tables in Windows cgame (0x30082054..0x3008241f), Windows game
 * (0x20083054..0x2008341f), and Linux game (0x000a9040..0x000a9467) agree on
 * these values and their ordering.
 * ANIM_STATE_COUNT and ANIM_MT_COUNT also define the common script-table
 * geometry used by both modules.
 */
typedef enum bg_anim_state_e {
    ANIM_STATE_RELAXED = 0,
    ANIM_STATE_QUERY = 1,
    ANIM_STATE_ALERT = 2,
    ANIM_STATE_COMBAT = 3,
    ANIM_STATE_COUNT = 4
} bg_anim_state_t;

typedef enum bg_anim_move_type_e {
    ANIM_MT_UNUSED = 0,
    ANIM_MT_IDLE = 1,
    ANIM_MT_IDLECR = 2,
    ANIM_MT_IDLEPRONE = 3,
    ANIM_MT_WALK = 4,
    ANIM_MT_WALKBK = 5,
    ANIM_MT_WALKCR = 6,
    ANIM_MT_WALKCRBK = 7,
    ANIM_MT_WALKPRONE = 8,
    ANIM_MT_WALKPRONEBK = 9,
    ANIM_MT_RUN = 10,
    ANIM_MT_RUNBK = 11,
    ANIM_MT_RUNCR = 12,
    ANIM_MT_RUNCRBK = 13,
    ANIM_MT_TURNRIGHT = 14,
    ANIM_MT_TURNLEFT = 15,
    ANIM_MT_CLIMBUP = 16,
    ANIM_MT_CLIMBDOWN = 17,
    ANIM_MT_COUNT = 18
} bg_anim_move_type_t;

enum {
    BG_ANIM_IDLE_MOVE_TYPE_MASK = (1u << ANIM_MT_IDLE) | (1u << ANIM_MT_IDLECR),
    BG_ANIM_CLIMB_MOVE_TYPE_MASK = (1u << ANIM_MT_CLIMBUP) | (1u << ANIM_MT_CLIMBDOWN)
};

/*
 * BG_PlayAnim's Windows cgame dispatch ladder proves 1 => legs, 2 => torso,
 * and 3 => both; the client and game body-part string tables include the
 * unused zero entry and agree on the complete four-value domain.
 */
typedef enum bg_anim_body_part_e {
    ANIM_BP_UNUSED = 0,
    ANIM_BP_LEGS = 1,
    ANIM_BP_TORSO = 2,
    ANIM_BP_BOTH = 3,
    ANIM_BP_COUNT = 4
} bg_anim_body_part_t;

typedef enum bg_anim_mounted_e {
    ANIM_MOUNT_UNUSED = 0,
    ANIM_MOUNT_MG42 = 1,
    ANIM_MOUNT_VEHICLE_DRIVER = 2,
    ANIM_MOUNT_VEHICLE_GUNNER = 3,
    ANIM_MOUNT_VEHICLE_PASSENGER_1 = 4,
    ANIM_MOUNT_VEHICLE_PASSENGER_2 = 5,
    ANIM_MOUNT_VEHICLE_PASSENGER_3 = 6,
    ANIM_MOUNT_VEHICLE_PASSENGER_4 = 7,
    ANIM_MOUNT_COUNT = 8
} bg_anim_mounted_t;

typedef enum bg_anim_vehicle_motion_e {
    ANIM_VEHICLE_MOTION_UNUSED = 0,
    ANIM_VEHICLE_MOTION_IDLE = 1,
    ANIM_VEHICLE_MOTION_FORWARD = 2,
    ANIM_VEHICLE_MOTION_REVERSING = 3,
    ANIM_VEHICLE_MOTION_COUNT = 4
} bg_anim_vehicle_motion_t;

typedef enum bg_anim_vehicle_e {
    ANIM_VEHICLE_UNUSED = 0,
    ANIM_VEHICLE_TANK = 1,
    ANIM_VEHICLE_JEEP = 2,
    ANIM_VEHICLE_FLAK88 = 3,
    ANIM_VEHICLE_COUNT = 4
} bg_anim_vehicle_t;

typedef enum bg_anim_weapon_position_e {
    ANIM_WEAPON_POSITION_HIP = 0,
    ANIM_WEAPON_POSITION_ADS = 1,
    ANIM_WEAPON_POSITION_COUNT = 2
} bg_anim_weapon_position_t;

typedef enum bg_anim_strafe_e {
    ANIM_STRAFE_NOT = 0,
    ANIM_STRAFE_LEFT = 1,
    ANIM_STRAFE_RIGHT = 2,
    ANIM_STRAFE_COUNT = 3
} bg_anim_strafe_t;

/*
 * Animation-script event indices. The original bgAnimEventStrings data table
 * supplies this complete ordered domain. BG_AnimScriptEvent addresses all
 * sixteen list slots by this index even though the table's source-level layout
 * partitions them into eventLists[10] followed by scriptLists[6]. DEATH is the
 * exceptional event still permitted after pmType reaches PM_TYPE_DEAD.
 */
typedef enum bg_anim_event_e {
    ANIM_EVENT_NONE = -1,
    ANIM_EVENT_PAIN = 0,
    ANIM_EVENT_DEATH = 1,
    ANIM_EVENT_FIRE_WEAPON = 2,
    ANIM_EVENT_JUMP = 3,
    ANIM_EVENT_JUMP_BACK = 4,
    ANIM_EVENT_LAND = 5,
    ANIM_EVENT_DROP_WEAPON = 6,
    ANIM_EVENT_RAISE_WEAPON = 7,
    ANIM_EVENT_CLIMB_MOUNT = 8,
    ANIM_EVENT_CLIMB_DISMOUNT = 9,
    ANIM_EVENT_RELOAD = 10,
    ANIM_EVENT_CROUCH_TO_PRONE = 11,
    ANIM_EVENT_PRONE_TO_CROUCH = 12,
    ANIM_EVENT_MELEE_ATTACK = 13,
    ANIM_EVENT_LMG_DEPLOY = 14,
    ANIM_EVENT_LMG_BREAKDOWN = 15,
    ANIM_EVENT_COUNT = 16
} bg_anim_event_t;

/* The two parser jump tables use this same section order. */
typedef enum bg_anim_parse_section_e {
    ANIM_SECTION_DEFINES = 0,
    ANIM_SECTION_ANIMATIONS = 1,
    ANIM_SECTION_CANNED_ANIMATIONS = 2,
    ANIM_SECTION_STATECHANGES = 3,
    ANIM_SECTION_EVENTS = 4
} bg_anim_parse_section_t;

/*
 * One two-body-part animation command. Windows cgame
 * BG_MarkScriptUsedPlayerAnims (0x300015f0), Windows game (0x200015e0), and
 * Linux game (0x00019ffa) agree on the signed-halfword fields and 0x10-byte
 * i386 stride.
 */
typedef struct bg_anim_script_command_s {
    int16_t bodyPart[2];                           /* +0x00 */
    int16_t animIndex[2];                          /* +0x04 */
    int16_t duration[2];                           /* +0x08 */
    const char *soundAliasName;                    /* +0x0c on i386 */
} bg_anim_script_command_t;

/*
 * One condition selector and its two comparison words. Windows cgame
 * BG_AnimConditionsMatch (0x30002ee0), Windows game (0x20002ec0), and Linux
 * game BG_EvaluateConditions (0x0001be6c) agree on the 0x0c-byte layout.
 */
typedef struct bg_anim_condition_s {
    int32_t type;                                  /* +0x00 */
    int32_t value[2];                              /* +0x04 */
} bg_anim_condition_t;

/*
 * One two-word value in the per-condition alias table. Windows cgame
 * BG_ParseConditionBits (0x30001920) and Linux game BG_ParseConditionBits
 * (RVA 0x0001a41e) both copy and combine the words independently. Each
 * condition owns BG_ANIM_CONDITION_VALUE_COUNT entries.
 */
typedef union bg_condition_bits_u {
    /* The cgame parser uses signed int words; the game parser's bit helpers
     * use the corresponding unsigned view. Both are the same stored bits. */
    int32_t bits[2];
    uint32_t unsignedBits[2];
} bg_condition_bits_t;

typedef char bg_condition_bits_size[sizeof(bg_condition_bits_t) == 8 ? 1 : -1];

/* Mode plus the optional indexed-string domain used to parse that condition. */
typedef struct bg_anim_condition_type_s {
    int32_t mode;                                  /* +0x00 */
    bg_indexed_string_t *values;                   /* +0x04 on i386 */
} bg_anim_condition_type_t;

/*
 * One parsed script block. The same condition and command walkers place
 * commandCount at +0x88 and the i386 command array at +0x8c.
 */
typedef struct bg_anim_script_s {
    int32_t conditionCount;                        /* +0x00 */
    bg_anim_condition_t conditions[ANIM_COND_COUNT]; /* +0x04 */
    int32_t commandCount;                          /* +0x88 */
    bg_anim_script_command_t commands[BG_ANIM_MAX_SCRIPT_COMMANDS];
                                                    /* +0x8c on i386 */
} bg_anim_script_t;

/*
 * One fixed-capacity candidate-script list. The authoritative Windows cgame,
 * Windows game, and Linux game bodies all load absolute bg_anim_script_t
 * pointers from the slots beginning at +0x04. The supporting Mac cgame and
 * game bodies store and reload the same absolute pointers. The original
 * 0x204-byte size is consequently an i386 pointer-width result, not a fixed
 * serialized or engine-facing ABI. Native 64-bit builds widen the pointers.
 */
typedef struct bg_anim_script_list_s {
    int32_t count;                                 /* +0x000 */
    bg_anim_script_t *scripts[BG_ANIM_MAX_LIST_ITEMS];
                                                    /* +0x004 on i386 */
} bg_anim_script_list_t;

/* bg_static_animation_t::flags is one shared client/server bit domain. The
 * Windows cgame and game parsers set the same values, and the Linux game
 * parser and consumers agree on the complete word. */
typedef enum bg_anim_entry_flag_e {
    BG_ANIM_ENTRY_NON_PRIMITIVE = 0x00000001,
    BG_ANIM_ENTRY_VERTICAL_MOTION = 0x00000002,
    BG_ANIM_ENTRY_TURRET = 0x00000004,
    BG_ANIM_ENTRY_FIRE_WEAPON = 0x00000008,
    BG_ANIM_ENTRY_STRAFE_LEFT = 0x00000010,
    BG_ANIM_ENTRY_STRAFE_RIGHT = 0x00000020,
    BG_ANIM_ENTRY_DEATH = 0x00000040,
    BG_ANIM_ENTRY_LOOPED = 0x00000080,
    BG_ANIM_ENTRY_UNUSED = 0x00000100
} bg_anim_entry_flag_t;

enum {
    BG_ANIM_ENTRY_STRAFE_MASK = BG_ANIM_ENTRY_STRAFE_LEFT | BG_ANIM_ENTRY_STRAFE_RIGHT,
    BG_ANIM_CROUCH_STATE_MASK = 0x000000c4,
    BG_ANIM_PRONE_STATE_MASK = 0x00000308
};

/* These source-level views are anonymous in the original shape. Keep the game
 * module's strict C99 mode while accepting the compiler-supported aggregate
 * extension, as the shared entity-state record does. */
#if defined(__GNUC__) || defined(__clang__)
#define BG_ANIMATION_UNION __extension__ union
#define BG_ANIMATION_STRUCT __extension__ struct
#else
#define BG_ANIMATION_UNION union
#define BG_ANIMATION_STRUCT struct
#endif

/* One entry in the common static animation table. BG_MarkScriptUsedPlayerAnims
 * in Windows cgame (0x300015f0) and game (0x200015e0) are instruction-identical
 * and use the 0x5c stride, +0x50/+0x54 flag words, and +0x58 tail. Linux game
 * RVA 0x00019ffa uses the same accesses. The low-word entryBone view is required
 * by the Windows cgame DObj animation calls; it overlays the same tail dword. */
typedef struct bg_static_animation_s {
    char name[64];
    int32_t blendTime;
    int32_t moveSpeed;
    int32_t duration;
    int32_t hash;
    BG_ANIMATION_UNION
    {
        uint32_t flags;
        BG_ANIMATION_STRUCT
        {
            uint8_t flagsLowByte;
            uint8_t flagsUpperBytes[3];
        };
    };
    BG_ANIMATION_UNION
    {
        uint32_t stateFlags;
        BG_ANIMATION_STRUCT
        {
            uint8_t stateFlagsLowByte;
            uint8_t stateFlagsUpperBytes[3];
        };
    };
    BG_ANIMATION_UNION
    {
        int32_t usedByScript;
        uint16_t entryBone;
    };
} bg_static_animation_t;

typedef const char *(*bg_anim_sound_alias_fn)(const char *name);
typedef void (*bg_anim_sound_event_fn)(int clientNum, const char *soundAliasName);

/* Complete BG animation table. The original i386 modules agree on every
 * region base through +0xa7ac8. Pointer-bearing script records and the final
 * tree-name/tree-handle union widen naturally in native 64-bit builds. */
typedef struct bg_static_animation_table_s {
    bg_static_animation_t entries[BG_ANIM_MAX_ANIMATIONS];
    int32_t entryCount;
    bg_anim_script_list_t animations[ANIM_STATE_COUNT][ANIM_MT_COUNT];
    bg_anim_script_list_t canned[ANIM_STATE_COUNT][ANIM_MT_COUNT];
    bg_anim_script_list_t statechanges[ANIM_STATE_COUNT][ANIM_STATE_COUNT];
    bg_anim_script_list_t events[ANIM_EVENT_RELOAD];
    bg_anim_script_list_t scriptLists[ANIM_EVENT_COUNT - ANIM_EVENT_RELOAD];
    bg_anim_script_t globalItems[BG_ANIM_MAX_GLOBAL_SCRIPTS];
    int32_t globalItemCount;
    BG_ANIMATION_UNION
    {
        const char *animTreeName;
        XAnim *animTreeHandle;
    };
} bg_static_animation_table_t;

/* Complete shared BG state. The Linux game `bgs` symbol has size 0xbaef4,
 * matching the Windows cgame clear extent and all original i386 field offsets.
 * Script handles are the common four-byte scr_anim_t, while native pointers and
 * callbacks widen normally on 64-bit hosts. */
typedef struct bgs_s {
    bg_static_animation_table_t animationTable;
    scr_anim_t resolvedTorsoAnimHandle;
    scr_anim_t resolvedLegsAnimHandle;
    scr_anim_t resolvedTurningAnimHandle;
    bg_anim_sound_alias_fn soundAliasCallback;
    bg_anim_sound_event_fn soundEventCallback;
    XAnim *multiplayerAnimTree;
    scr_anim_t rootAnimHandle;
    scr_anim_t torsoAnimHandle;
    scr_anim_t legsAnimHandle;
    scr_anim_t turningAnimHandle;
    clientInfo_t clientinfo[BGS_CLIENTINFO_COUNT];
} bgs_t;

#define BG_ANIMATION_TYPES_ASSERT(name, expression) typedef char bg_animation_types_assert_##name[(expression) ? 1 : -1]

BG_ANIMATION_TYPES_ASSERT(condition_size, sizeof(bg_anim_condition_t) == 0x0c);
BG_ANIMATION_TYPES_ASSERT(static_animation_size, sizeof(bg_static_animation_t) == 0x5c);
BG_ANIMATION_TYPES_ASSERT(static_animation_flags_offset, offsetof(bg_static_animation_t, flags) == 0x50);
BG_ANIMATION_TYPES_ASSERT(static_animation_tail_offset, offsetof(bg_static_animation_t, usedByScript) == 0x58);
BG_ANIMATION_TYPES_ASSERT(script_command_count_offset, offsetof(bg_anim_script_t, commandCount) == 0x88);

#if UINTPTR_MAX == UINT32_MAX
BG_ANIMATION_TYPES_ASSERT(indexed_string_i386_layout, sizeof(bg_indexed_string_t) == 0x08);
BG_ANIMATION_TYPES_ASSERT(condition_type_i386_layout, sizeof(bg_anim_condition_type_t) == 0x08);
BG_ANIMATION_TYPES_ASSERT(script_command_i386_layout,
                          offsetof(bg_anim_script_command_t, soundAliasName) == 0x0c && sizeof(bg_anim_script_command_t) == 0x10);
BG_ANIMATION_TYPES_ASSERT(script_i386_layout, offsetof(bg_anim_script_t, commands) == 0x8c && sizeof(bg_anim_script_t) == 0x10c);
BG_ANIMATION_TYPES_ASSERT(script_list_i386_layout,
                          offsetof(bg_anim_script_list_t, scripts) == 0x04 && sizeof(bg_anim_script_list_t) == 0x204);
BG_ANIMATION_TYPES_ASSERT(static_table_i386_layout, offsetof(bg_static_animation_table_t, entryCount) == 0xb800 &&
                                                        offsetof(bg_static_animation_table_t, animations) == 0xb804 &&
                                                        offsetof(bg_static_animation_table_t, canned) == 0x14924 &&
                                                        offsetof(bg_static_animation_table_t, statechanges) == 0x1da44 &&
                                                        offsetof(bg_static_animation_table_t, events) == 0x1fa84 &&
                                                        offsetof(bg_static_animation_table_t, scriptLists) == 0x20eac &&
                                                        offsetof(bg_static_animation_table_t, globalItems) == 0x21ac4 &&
                                                        offsetof(bg_static_animation_table_t, globalItemCount) == 0xa7ac4 &&
                                                        offsetof(bg_static_animation_table_t, animTreeName) == 0xa7ac8 &&
                                                        sizeof(bg_static_animation_table_t) == 0xa7acc);
BG_ANIMATION_TYPES_ASSERT(bgs_i386_layout, offsetof(bgs_t, resolvedTorsoAnimHandle) == 0xa7acc &&
                                               offsetof(bgs_t, soundAliasCallback) == 0xa7ad8 &&
                                               offsetof(bgs_t, multiplayerAnimTree) == 0xa7ae0 && offsetof(bgs_t, clientinfo) == 0xa7af4 &&
                                               sizeof(bgs_t) == 0xbaef4);
#elif UINTPTR_MAX == UINT64_MAX
BG_ANIMATION_TYPES_ASSERT(script_command_64_layout,
                          offsetof(bg_anim_script_command_t, soundAliasName) == 0x10 && sizeof(bg_anim_script_command_t) == 0x18);
BG_ANIMATION_TYPES_ASSERT(script_64_layout, offsetof(bg_anim_script_t, commands) == 0x90 && sizeof(bg_anim_script_t) == 0x150);
BG_ANIMATION_TYPES_ASSERT(script_list_64_layout,
                          offsetof(bg_anim_script_list_t, scripts) == 0x08 && sizeof(bg_anim_script_list_t) == 0x408);
#endif

#undef BG_ANIMATION_TYPES_ASSERT
#undef BG_ANIMATION_STRUCT
#undef BG_ANIMATION_UNION

#endif
