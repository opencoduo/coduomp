#ifndef QCOMMON_SNAPSHOT_TYPES_H
#define QCOMMON_SNAPSHOT_TYPES_H

#include "client_state_types.h"
#include "entity_state_types.h"
#include "player_state_types.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_ENTITIES_IN_SNAPSHOT = 256,
    MAX_CLIENTS_IN_SNAPSHOT = 64
};

/* Public snapshot flags shared by the engine writer and cgame consumers.
 * SNAPFLAG_NOT_ACTIVE is tested by CoDUOMP.exe CL_FirstSnapshot (0x004056a0)
 * and uo_cgame_mp_x86.dll CG_ProcessSnapshots (0x3003d337) to skip a snapshot
 * that cannot activate the client.  CG_ProcessSnapshots tests a change in
 * SNAPFLAG_SERVERCOUNT at 0x3003d3e1 to detect a server restart. */
enum {
    SNAPFLAG_NOT_ACTIVE = 2,
    SNAPFLAG_SERVERCOUNT = 4
};

/* Archived server snapshot entity. The first member is the complete network
 * state; the four trailing fields are the extra rows present in the archived
 * entity netfield table in both original engines. */
typedef struct archivedEntity_s {
    entityState_t state;
    int32_t svFlags;
    int32_t singleClient;
    vec3_t absmin;
    vec3_t absmax;
} archivedEntity_t;

typedef char q_archived_entity_state_offset[
    offsetof(archivedEntity_t, state) == 0x000 ? 1 : -1];
typedef char q_archived_entity_svflags_offset[
    offsetof(archivedEntity_t, svFlags) == 0x0f4 ? 1 : -1];
typedef char q_archived_entity_single_client_offset[
    offsetof(archivedEntity_t, singleClient) == 0x0f8 ? 1 : -1];
typedef char q_archived_entity_absmin_offset[
    offsetof(archivedEntity_t, absmin) == 0x0fc ? 1 : -1];
typedef char q_archived_entity_absmax_offset[
    offsetof(archivedEntity_t, absmax) == 0x108 ? 1 : -1];
typedef char q_archived_entity_size[
    sizeof(archivedEntity_t) == 0x114 ? 1 : -1];

/*
 * Engine-to-cgame snapshot boundary.  CoDUOMP.exe CL_GetSnapshot
 * (0x00401260) writes the player state at +0x00c, caps and writes the entity
 * and client counts at +0x4510/+0x4514, copies 0xf4-byte entity rows to
 * +0x4518 and 0x5c-byte client rows to +0x13918, and writes the server-command
 * sequence at +0x1501c.  The original uo_cgame_mp_x86.dll transition path
 * (0x3003ca30) independently consumes those same offsets, extents, and strides.
 *
 * The word at +0x15018 is part of the public ABI footprint but has no recovered
 * value semantics: the original engine writer leaves it unchanged and no
 * original cgame instruction reads it.
 */
typedef struct snapshot_s {
    uint32_t snapFlags;                                  /* +0x00000 */
    int32_t ping;                                        /* +0x00004 */
    int32_t serverTime;                                  /* +0x00008 */
    playerState_t ps;                                    /* +0x0000c */
    int32_t numEntities;                                 /* +0x04510 */
    int32_t numClients;                                  /* +0x04514 */
    entityState_t entities[MAX_ENTITIES_IN_SNAPSHOT];    /* +0x04518 */
    clientState_t clients[MAX_CLIENTS_IN_SNAPSHOT];      /* +0x13918 */
    uint32_t unused15018;                                /* +0x15018 */
    int32_t serverCommandSequence;                       /* +0x1501c */
} snapshot_t;

#define SNAPSHOT_LAYOUT_ASSERT(name_, expression_) \
    typedef char name_[(expression_) ? 1 : -1]

SNAPSHOT_LAYOUT_ASSERT(q_snapshot_ps_offset,
                       offsetof(snapshot_t, ps) == 0x0000c);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_ps_client_num_offset,
                       offsetof(snapshot_t, ps) +
                               offsetof(playerState_t, psClientNum) ==
                           0x000e0);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_ps_current_weapon_offset,
                       offsetof(snapshot_t, ps) +
                               offsetof(playerState_t, currentWeapon) ==
                           0x000e4);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_ps_objectives_offset,
                       offsetof(snapshot_t, ps) +
                               offsetof(playerState_t, objectives) ==
                           0x00644);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_ps_hud_current_offset,
                       offsetof(snapshot_t, ps) +
                               offsetof(playerState_t, hudCurrent) ==
                           0x00804);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_ps_hud_archival_offset,
                       offsetof(snapshot_t, ps) +
                               offsetof(playerState_t, hudArchival) ==
                           0x02688);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_num_entities_offset,
                       offsetof(snapshot_t, numEntities) == 0x04510);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_num_clients_offset,
                       offsetof(snapshot_t, numClients) == 0x04514);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_entities_offset,
                       offsetof(snapshot_t, entities) == 0x04518);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_clients_offset,
                       offsetof(snapshot_t, clients) == 0x13918);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_unused_offset,
                       offsetof(snapshot_t, unused15018) == 0x15018);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_server_command_sequence_offset,
                       offsetof(snapshot_t, serverCommandSequence) == 0x1501c);
SNAPSHOT_LAYOUT_ASSERT(q_snapshot_size,
                       sizeof(snapshot_t) == 0x15020);

#undef SNAPSHOT_LAYOUT_ASSERT

#endif
