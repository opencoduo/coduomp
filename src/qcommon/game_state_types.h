#ifndef QCOMMON_GAME_STATE_TYPES_H
#define QCOMMON_GAME_STATE_TYPES_H

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_CONFIGSTRINGS = 2048,
    MAX_GAMESTATE_CHARS_RETAIL = 20480,
    MAX_GAMESTATE_CHARS_EXTENDED = 32768
};

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): existing protocol-22
 * servers can serialize more configstring data than the retail client can
 * retain in its 20-KiB packed pool. The stock source keeps the original
 * engine/cgame ABI; the improved source expands both client-side copies to a
 * 32-KiB packed pool. */
enum {
    MAX_GAMESTATE_CHARS = MAX_GAMESTATE_CHARS_RETAIL
};

/*
 * Shared multiplayer config-string layout.  The Windows cgame asset loaders
 * and config-string dispatcher use the same bases and terminal indices as the
 * Windows/Linux game registration and script paths.  The consecutive ranges
 * also prove one another geometrically: each base is the preceding base plus
 * its count from CS_TAGS through CS_TIMEOUT_TIME.  The engine/cgame gameState_t
 * boundary independently fixes the enclosing 2,048-slot capacity.
 */
typedef enum configStringIndex_e {
    CS_SERVERINFO = 0,
    CS_SYSTEMINFO = 1,
    CS_AMBIENT = 3,
    CS_TEAM_SCORE_AXIS = 5,
    CS_TEAM_SCORE_ALLIES = 6,
    CS_WEAPONS = 7,
    CS_ITEMS = 8,
    CS_WIND = 12,
    CS_FOGVARS = 13,
    CS_MOTD = 15,
    CS_VOTE_TIME = 16,
    CS_VOTE_STRING = 17,
    CS_VOTE_YES = 18,
    CS_VOTE_NO = 19,
    CS_GAMESTATE = 21,
    CS_STATUS_ICONS = 22,
    CS_STATUS_ICONS_COUNT = 16,
    CS_HEAD_ICONS = 38,
    CS_HEAD_ICONS_COUNT = 15,
    CS_LOCATIONS = 53,
    CS_LOCATIONS_COUNT = 64,
    CS_TAGS = 117,
    CS_TAGS_COUNT = 32,
    CS_CONFIGVALUE_NAMES = 149,
    CS_CONFIGVALUE_VALUES = 277,
    CS_CONFIGVALUE_COUNT = 128,
    CS_MODELS = 405,
    CS_MODELS_COUNT = 256,
    CS_SOUNDS = 661,
    CS_SOUNDS_COUNT = 256,
    CS_EFFECTS = 917,
    CS_EFFECTS_COUNT = 80,
    CS_FX = 997,
    CS_FX_COUNT = 256,
    CS_SHELLSHOCKS = 1253,
    CS_SHELLSHOCKS_COUNT = 16,
    CS_SCRIPTMENUS = 1333,
    CS_SCRIPTMENUS_COUNT = 32,
    CS_HINTSTRINGS = 1365,
    CS_HINTSTRINGS_COUNT = 32,
    CS_LOCALIZED_STRINGS = 1397,
    CS_LOCALIZED_STRINGS_COUNT = 256,
    CS_SHADERS = 1653,
    CS_SHADERS_COUNT = 256,
    CS_TIMEOUT_TIME = 1909,
    CS_TIMEOUT_STRING = 1910
} configStringIndex_t;

typedef char q_config_string_index_abi_size[sizeof(configStringIndex_t) == 4 ? 1 : -1];

/* Packed config-string state copied across the client-engine/cgame boundary.
 * Retail CoDUOMP.exe CL_GetGameState (0x00401180) copies 0x1c01 dwords,
 * exactly 0x7004 bytes. The stock source preserves that layout. The improved
 * source expands the shared engine/cgame character pool while retaining the
 * offset-table geometry. */
typedef struct gameState_s {
    int32_t stringOffsets[MAX_CONFIGSTRINGS]; /* +0x0000 */
    char stringData[MAX_GAMESTATE_CHARS];     /* +0x2000 */
    int32_t dataCount; /* retail +0x7000; extended +0xa000 */
} gameState_t;

#define GAME_STATE_LAYOUT_ASSERT(name_, expression_) typedef char name_[(expression_) ? 1 : -1]

GAME_STATE_LAYOUT_ASSERT(q_game_state_offsets_extent, sizeof(((gameState_t *)0)->stringOffsets) == 0x2000);
GAME_STATE_LAYOUT_ASSERT(q_game_state_data_offset, offsetof(gameState_t, stringData) == 0x2000);
GAME_STATE_LAYOUT_ASSERT(q_game_state_data_extent, sizeof(((gameState_t *)0)->stringData) == MAX_GAMESTATE_CHARS);
GAME_STATE_LAYOUT_ASSERT(q_game_state_count_offset, offsetof(gameState_t, dataCount) == 0x2000 + MAX_GAMESTATE_CHARS);
GAME_STATE_LAYOUT_ASSERT(q_game_state_size, sizeof(gameState_t) == 0x2004 + MAX_GAMESTATE_CHARS);

GAME_STATE_LAYOUT_ASSERT(q_game_state_selected_retail_capacity, MAX_GAMESTATE_CHARS == 0x5000);

#undef GAME_STATE_LAYOUT_ASSERT

#endif
