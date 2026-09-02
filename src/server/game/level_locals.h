#ifndef LEVEL_LOCALS_H
#define LEVEL_LOCALS_H

#include <stdint.h>
#include <stddef.h>
#include "recovered_game.h"

/*
 * Partial level_locals_t struct (ABI total size 0x2EC8 = 11976 bytes).
 *
 * Binary symbol: `level` at ELF RVA 0x23b520, size 0x2EC8.
 * Ghidra address range: 0x24b520 .. 0x24e3e7 (imagebase 0x10000).
 *
 * All DAT_0024xxxx addresses from the decompiler that fall within this
 * range are level fields, NOT separate globals or bgs fields.
 *
 * RECOVERED(UO-GAME-UNK-0141): struct layout is recovered. Named fields
 * are confirmed from decompiler output; field names/types are inferred.
 */
enum {
    LEVEL_TRIGGER_NOTIFY_WATCH_LIMIT = 256,
    LEVEL_SPAWN_VAR_MAX_PAIRS = 64,
    LEVEL_SPAWN_VAR_PAIR_SLOTS = LEVEL_SPAWN_VAR_MAX_PAIRS * 2,
    LEVEL_SPAWN_VAR_TEXT_SIZE = 2048
};

typedef struct level_trigger_notify_watch_entry_s {
    uint16_t entNum1;
    uint16_t entNum2;
    int32_t token1;
    int32_t token2;
} level_trigger_notify_watch_entry_t;

typedef struct level_locals_s {
    gclient_t *clients;                  /* +0x000, set to g_clients by G_InitGame */
    gentity_t *gentities;                /* +0x004, set to g_entities */
    int32_t
        gentity_size; /* +0x008, size 4; checked metadata/decompiler for DAT_0024b528 access, none found; retained for num_entities +0x00c ABI. MANUALLY LABELLED */
    int32_t num_entities; /* +0x00c, DAT_0024b52c, set to 0x48 */
    gentity_t *freeListHead; /* +0x010, DAT_0024b530, free entity list head */
    gentity_t *freeListTail; /* +0x014, DAT_0024b534, free entity list tail */
    vehicle_state_t *vehicleStateBase; /* +0x018, DAT_0024b538; base of the 64 vehicle-state records */
    int32_t logFile; /* +0x01c, file handle */
    int32_t spawning; /* +0x020, DAT_0024b540, 1 during init */
    objective_t objectives[PLAYERSTATE_OBJECTIVE_COUNT]; /* +0x024, DAT_0024b544 */
    int32_t maxclients; /* +0x1e4, DAT_0024b704, from g_maxclients */
    int32_t framenum; /* +0x1e8, incremented/frame */
    int32_t time; /* +0x1ec, levelTime, server time ms */
    int32_t previousTime; /* +0x1f0, DAT_0024b710, previous frame time */
    int32_t frameTime; /* +0x1f4, frame delta */
    int32_t startTime; /* +0x1f8, DAT_0024b718, initial levelTime */
    char abiGap_1fc_1ff
        [0x004]; /* +0x1fc, size 4; checked metadata/decompiler for DAT_0024b71c access, none found; retained for teamScoreAxis +0x200 ABI */
    int32_t teamScoreAxis; /* +0x200, DAT_0024b720 */
    int32_t teamScoreAllies; /* +0x204, DAT_0024b724 */
    char abiGap_208_20b
        [0x004]; /* +0x208, size 4; checked metadata/decompiler for DAT_0024b728 access, none found; retained for teamStatusTime +0x20c ABI */
    int32_t teamStatusTime; /* +0x20c, CheckTeamStatus throttle */
    int32_t scoreboardDirty; /* +0x210, DAT_0024b730 */
    scriptClientNameMode_t clientNameMode; /* +0x214, DAT_0024b734 */
    int32_t sortedClientCount; /* +0x218, DAT_0024b738 */
    int32_t sortedClients[64]; /* +0x21c, DAT_0024b73c */
    char voteString[MAX_STRING_CHARS]; /* +0x31c, DAT_0024b83c */
    char voteDisplayString[MAX_STRING_CHARS]; /* +0x71c, DAT_0024bc3c */
    int32_t voteTime; /* +0xb1c, DAT_0024c03c */
    int32_t voteExecuteTime; /* +0xb20, DAT_0024c040 */
    int32_t voteYes; /* +0xb24, DAT_0024c044 */
    int32_t voteNo; /* +0xb28, DAT_0024c048 */
    int32_t numVotingClients; /* +0xb2c, DAT_0024c04c */
    char teamVoteString[2][0x400]; /* +0xb30 */
    int32_t teamVoteTime[2]; /* +0x1330 */
    int32_t teamVoteYes[2]; /* +0x1338 */
    int32_t teamVoteNo[2]; /* +0x1340 */
    char timeoutMessage[MAX_STRING_CHARS]; /* +0x1348, DAT_0024c868 */
    int32_t timeoutCache1; /* +0x1748, g_timeoutBank.integer */
    int32_t timeoutCache2; /* +0x174c, g_timeoutBank.integer */
    int32_t timeoutUsedAllies; /* +0x1750, DAT_0024cc70 */
    int32_t timeoutUsedAxis; /* +0x1754, DAT_0024cc74 */
    int32_t matchTimeoutDuration; /* +0x1758, DAT_0024cc78 */
    int32_t matchTimeoutStartTime; /* +0x175c, DAT_0024cc7c */
    int32_t matchTimeoutRecoveryEndTime; /* +0x1760, DAT_0024cc80 */
    int32_t matchTimeoutTeam; /* +0x1764, DAT_0024cc84 */
    char abiGap_1768_176b
        [0x004]; /* +0x1768, size 4; checked metadata/decompiler for DAT_0024cc88 access, none found; retained for spawningMapEntities +0x176c ABI */
    int32_t spawningMapEntities; /* +0x176c, DAT_0024cc8c */
    int32_t spawnVarCount; /* +0x1770, DAT_0024cc90 */
    char *spawnVarPairSlots[LEVEL_SPAWN_VAR_PAIR_SLOTS];
    /* +0x1774 on i386; native char* slots */
    int32_t spawnTextLength; /* +0x1974, DAT_0024ce94 */
    char spawnText[LEVEL_SPAWN_VAR_TEXT_SIZE]; /* +0x1978 */
    int32_t scriptExitParam; /* +0x2178, script exit delay */
    int32_t targetLocationsLinked; /* +0x217c, DAT_0024d69c */
    gentity_t *targetLocationHead; /* +0x2180, DAT_0024d6a0 */
    gentity_t *droppedWeaponSlots[32]; /* +0x2184, DAT_0024d6a4 */
    float fogOpaqueDist; /* +0x2204, DAT_0024d724, G_setfog cache */
    float fogOpaqueDistSq; /* +0x2208, DAT_0024d728, G_setfog squared cache */
    char abiGap_220c_220f
        [0x004]; /* +0x220c, size 4; checked metadata/decompiler for DAT_0024d72c access, none found; retained for playerCloneCursor +0x2210 ABI */
    int32_t playerCloneCursor; /* +0x2210, DAT_0024d730 */
    int32_t cachedTagMatrixTime; /* +0x2214, DAT_0024d734, ScriptSpatial_GetTagMatrix level-time cache */
    int32_t cachedTagMatrixObject; /* +0x2218, DAT_0024d738, set to 0x3ff; tag-matrix script-object cache key */
    uint16_t cachedTagName; /* +0x221c, DAT_0024d73c, tag-matrix const-string cache */
    char padding221e[2]; /* +0x221e..+0x221f, aligns cachedTagMatrix. */
    DObjSkelMat cachedTagMatrix; /* +0x2220, DAT_0024d740, G_DObjGetWorldTagMatrix output cache */
    level_trigger_notify_watch_entry_t triggerNotifyWatch[LEVEL_TRIGGER_NOTIFY_WATCH_LIMIT];
    /* +0x2260 */
    int32_t notifyWatchCount; /* +0x2e60, watch array count */
    int32_t exitState; /* +0x2e64, DAT_0024e384, 0=none 1=map_restart 2=exitlevel */
    int32_t radiusDamageIgnorePlayersActive; /* +0x2e68, DAT_0024e388, radiusdamage call-scoped copy */
    int32_t radiusDamageIgnorePlayersSetting; /* +0x2e6c, DAT_0024e38c, setplayerignoreradiusdamage setting */
    float
        cachedBoundsWidth; /* +0x2e70, AUDITED_DECOMPILER(0x69c21, 69c21_G_RunFrame.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): cvar.value cache for g_bounds_width. */
    float
        cachedBoundsHeightStanding; /* +0x2e74, AUDITED_DECOMPILER(0x69c21, 69c21_G_RunFrame.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): cvar.value cache for g_bounds_height_standing. */
    float
        cachedViewheightStanding; /* +0x2e78, AUDITED_DECOMPILER(0x69c21, 69c21_G_RunFrame.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): cvar.value cache for bg_viewheight_standing. */
    float
        cachedViewheightCrouched; /* +0x2e7c, AUDITED_DECOMPILER(0x69c21, 69c21_G_RunFrame.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): cvar.value cache for bg_viewheight_crouched. */
    float
        cachedViewheightProne; /* +0x2e80, AUDITED_DECOMPILER(0x69c21, 69c21_G_RunFrame.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): cvar.value cache for bg_viewheight_prone. */
    char abiGap_2e84_2ec3
        [0x040]; /* +0x2e84, size 0x40; the complete Linux and Windows game-module instruction streams contain no level-relative access in this range. Linux G_RunFrame stops at cachedViewheightProne +0x2e80 and later users resume at registeredItemsDirty +0x2ec4; retained unnamed because neither machine code nor Quake III supplies an exact inherited record. */
    int32_t registeredItemsDirty; /* +0x2ec4, DAT_0024e3e4 */
} level_locals_t;

extern level_locals_t level;

GAME_STATIC_ASSERT(level_notify_watch_entry_size, sizeof(level_trigger_notify_watch_entry_t) == 0x0c);

#if UINTPTR_MAX == 0xffffffffu
GAME_STATIC_ASSERT(level_num_entities_offset, offsetof(level_locals_t, num_entities) == 0x00c);
GAME_STATIC_ASSERT(level_objectives_offset, offsetof(level_locals_t, objectives) == 0x024);
GAME_STATIC_ASSERT(level_time_offset, offsetof(level_locals_t, time) == 0x1ec);
GAME_STATIC_ASSERT(level_previous_time_offset, offsetof(level_locals_t, previousTime) == 0x1f0);
GAME_STATIC_ASSERT(level_team_status_time_offset, offsetof(level_locals_t, teamStatusTime) == 0x20c);
GAME_STATIC_ASSERT(level_sorted_clients_offset, offsetof(level_locals_t, sortedClients) == 0x21c);
GAME_STATIC_ASSERT(level_vote_display_string_offset, offsetof(level_locals_t, voteDisplayString) == 0x71c);
GAME_STATIC_ASSERT(level_vote_time_offset, offsetof(level_locals_t, voteTime) == 0xb1c);
GAME_STATIC_ASSERT(level_timeout_message_offset, offsetof(level_locals_t, timeoutMessage) == 0x1348);
GAME_STATIC_ASSERT(level_timeout_used_allies_offset, offsetof(level_locals_t, timeoutUsedAllies) == 0x1750);
GAME_STATIC_ASSERT(level_timeout_used_axis_offset, offsetof(level_locals_t, timeoutUsedAxis) == 0x1754);
GAME_STATIC_ASSERT(level_spawning_map_entities_offset, offsetof(level_locals_t, spawningMapEntities) == 0x176c);
GAME_STATIC_ASSERT(level_spawn_var_pairs_offset, offsetof(level_locals_t, spawnVarPairSlots) == 0x1774);
GAME_STATIC_ASSERT(level_spawn_var_pair_storage_size, sizeof(((level_locals_t *)0)->spawnVarPairSlots) == 0x200);
GAME_STATIC_ASSERT(level_spawn_var_text_offset, offsetof(level_locals_t, spawnText) == 0x1978);
GAME_STATIC_ASSERT(level_spawn_var_text_size, sizeof(((level_locals_t *)0)->spawnText) == 2048);
GAME_STATIC_ASSERT(level_target_locations_linked_offset, offsetof(level_locals_t, targetLocationsLinked) == 0x217c);
GAME_STATIC_ASSERT(level_target_location_head_offset, offsetof(level_locals_t, targetLocationHead) == 0x2180);
GAME_STATIC_ASSERT(level_dropped_weapon_slots_offset, offsetof(level_locals_t, droppedWeaponSlots) == 0x2184);
GAME_STATIC_ASSERT(level_fog_opaque_dist_offset, offsetof(level_locals_t, fogOpaqueDist) == 0x2204);
GAME_STATIC_ASSERT(level_fog_opaque_dist_sq_offset, offsetof(level_locals_t, fogOpaqueDistSq) == 0x2208);
GAME_STATIC_ASSERT(level_cached_tag_matrix_time_offset, offsetof(level_locals_t, cachedTagMatrixTime) == 0x2214);
GAME_STATIC_ASSERT(level_cached_tag_matrix_object_offset, offsetof(level_locals_t, cachedTagMatrixObject) == 0x2218);
GAME_STATIC_ASSERT(level_notify_watch_array_offset, offsetof(level_locals_t, triggerNotifyWatch) == 0x2260);
GAME_STATIC_ASSERT(level_cached_tag_name_offset, offsetof(level_locals_t, cachedTagName) == 0x221c);
GAME_STATIC_ASSERT(level_cached_tag_matrix_offset, offsetof(level_locals_t, cachedTagMatrix) == 0x2220);
GAME_STATIC_ASSERT(level_cached_tag_matrix_size, sizeof(((level_locals_t *)0)->cachedTagMatrix) == 0x40);
GAME_STATIC_ASSERT(level_notify_watch_count_offset, offsetof(level_locals_t, notifyWatchCount) == 0x2e60);
GAME_STATIC_ASSERT(level_radius_damage_ignore_players_active_offset, offsetof(level_locals_t, radiusDamageIgnorePlayersActive) == 0x2e68);
GAME_STATIC_ASSERT(level_radius_damage_ignore_players_setting_offset, offsetof(level_locals_t, radiusDamageIgnorePlayersSetting) == 0x2e6c);
GAME_STATIC_ASSERT(level_registered_items_dirty_offset, offsetof(level_locals_t, registeredItemsDirty) == 0x2ec4);
#endif

#endif /* LEVEL_LOCALS_H */
