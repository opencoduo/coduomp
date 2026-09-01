#ifndef QCOMMON_CLIENT_STATE_TYPES_H
#define QCOMMON_CLIENT_STATE_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Shared multiplayer team domain. TEAM_COUNT is the exclusive array bound,
 * not a team value; TEAM_FREE is also the value used for no assigned team. */
typedef enum team_e {
    TEAM_FREE = 0,
    TEAM_AXIS = 1,
    TEAM_ALLIES = 2,
    TEAM_SPECTATOR = 3,
    TEAM_COUNT = 4
} team_t;

enum {
    CLIENT_STATE_ATTACHMENT_COUNT = 6,
    CLIENT_STATE_NAME_SIZE = 32
};

/* Client state transmitted beside entity snapshots and copied unchanged by
 * the live/archived-client-info game syscall. CoDUOMP's netfield table names
 * the complete record; the cgame transition path independently consumes the
 * same lanes at a 0x5c stride. The game client-session command exposes this
 * subobject directly, and the live/archive engine paths copy all 23 dwords.
 * The pointer-free layout is the same in the Windows client/server and Linux
 * server binaries. */
typedef struct clientState_s {
    int32_t clientNum; /* +0x00, visible client number */
    team_t team;       /* +0x04, current multiplayer team */
    int32_t modelindex; /* +0x08, exact original netfield spelling */
    int32_t attachModelIndex[CLIENT_STATE_ATTACHMENT_COUNT]; /* +0x0c */
    int32_t attachTagIndex[CLIENT_STATE_ATTACHMENT_COUNT];   /* +0x24 */
    char name[CLIENT_STATE_NAME_SIZE]; /* +0x3c, display name */
} clientState_t;

#define CLIENT_STATE_LAYOUT_ASSERT(name_, expression_) \
    typedef char name_[(expression_) ? 1 : -1]

#if defined(_MSC_VER)
#define CLIENT_STATE_ALIGNOF(type_) __alignof(type_)
#elif defined(__GNUC__) || defined(__clang__)
#define CLIENT_STATE_ALIGNOF(type_) __alignof__(type_)
#elif defined(__cplusplus)
#define CLIENT_STATE_ALIGNOF(type_) alignof(type_)
#else
#define CLIENT_STATE_ALIGNOF(type_) _Alignof(type_)
#endif

CLIENT_STATE_LAYOUT_ASSERT(q_team_type_size, sizeof(team_t) == 4);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_alignment,
                           CLIENT_STATE_ALIGNOF(clientState_t) == 4);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_client_num_offset,
                           offsetof(clientState_t, clientNum) == 0x00);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_team_offset,
                           offsetof(clientState_t, team) == 0x04);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_model_index_offset,
                           offsetof(clientState_t, modelindex) == 0x08);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_attach_model_index_offset,
                           offsetof(clientState_t, attachModelIndex) == 0x0c);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_attach_model_index_extent,
                           sizeof(((clientState_t *)0)->attachModelIndex) ==
                               0x18);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_attach_tag_index_offset,
                           offsetof(clientState_t, attachTagIndex) == 0x24);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_attach_tag_index_extent,
                           sizeof(((clientState_t *)0)->attachTagIndex) ==
                               0x18);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_name_offset,
                           offsetof(clientState_t, name) == 0x3c);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_name_extent,
                           sizeof(((clientState_t *)0)->name) == 0x20);
CLIENT_STATE_LAYOUT_ASSERT(q_client_state_size,
                           sizeof(clientState_t) == 0x5c);

#undef CLIENT_STATE_ALIGNOF
#undef CLIENT_STATE_LAYOUT_ASSERT

#endif
