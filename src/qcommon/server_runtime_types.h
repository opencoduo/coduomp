#ifndef QCOMMON_SERVER_RUNTIME_TYPES_H
#define QCOMMON_SERVER_RUNTIME_TYPES_H

#include "asset_type_names.h"
#include "client_state_types.h"
#include "net_types.h"
#include "player_state_types.h"
#include "q_shared_types.h"
#include "qcommon_limits.h"
#include "server_types.h"
#include "snapshot_types.h"

#include <stddef.h>
#include <stdint.h>

/* Fixed domains shared by the original Windows and Linux server runtimes. */
enum {
    SERVER_CLIENT_SNAPSHOT_FRAME_COUNT = 32,
    SERVER_PUNKBUSTER_GUID_SIZE = 33,
    SERVER_MAP_NAME_BUFFER_SIZE = MAX_QPATH,
    SERVER_PLAYER_NAME_BUFFER_SIZE = 64,
    SERVER_CHALLENGE_AUTH_GUID_SIZE = 32,
    SERVER_AUTHORIZE_GUID_CACHE_ENTRY_COUNT = 16,
    SERVER_SCRIPT_CONFIGSTRING_BASE = 149,
    SERVER_SCRIPT_CONFIGSTRING_COUNT = 128,
    SERVER_CACHED_SNAPSHOT_ENTITY_COUNT = 16384,
    SERVER_CACHED_SNAPSHOT_CLIENT_COUNT = 4096,
    SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT = 1200,
    SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE = 33554432,
    SERVER_CACHED_SNAPSHOT_FRAME_COUNT = 512,
    SERVER_ENTITY_LINK_BOUNDS_COMPONENTS = 2,
    SERVER_WORLD_SECTOR_CHILD_COUNT = 2,
    SERVER_WORLD_SECTOR_POOL_COUNT = 1024,
    SERVER_PROTOCOL_VERSION = 22,
    SERVER_DELTA_MESSAGE_NONE = -1,
    SERVER_SNAPSHOT_RESTART_FLAG = 4,
    SERVER_ID_LOW_MASK = 15,
    SERVER_ID_HIGH_MASK = 240,
    SERVER_RESTART_WARMUP_FRAMES = 3,
    SERVER_RESTART_WARMUP_MSEC = 100
};

/* Complete host-visible prefix of the game module's shared entity. The module
 * supplies the complete record stride at runtime; the engine-visible members
 * through ownerNum are pointer-free and retain their original offsets on
 * native 64-bit targets. */
typedef struct sharedEntity_s {
    entityState_t entityState;
    qboolean linked;
    uint32_t svFlags;
    int32_t singleClient;
    int32_t soundTime;
    qboolean bmodel;
    vec3_t mins;
    vec3_t maxs;
    int32_t contents;
    vec3_t absMin;
    vec3_t absMax;
    vec3_t currentOrigin;
    vec3_t currentAngles;
    int32_t ownerNum;
} sharedEntity_t;

typedef struct worldSector_s worldSector_t;
typedef struct worldSectorAreaLink_s worldSectorAreaLink_t;
typedef struct svEntity_s svEntity_t;

struct worldSectorAreaLink_s {
    XModel *model;
    vec3_t origin;
    axis_t inverseAxis;
    worldSectorAreaLink_t *nextInWorldSector;
    vec3_t linkMins;
    vec3_t linkMaxs;
    qboolean sightTraceEligible;
};

struct svEntity_s {
    worldSector_t *worldSector;
    svEntity_t *nextInWorldSector;
    archivedEntity_t baseline;
    int32_t numClusters;
    int32_t clusterNums[MAX_ENT_CLUSTERS];
    int32_t lastCluster;
    int32_t areaNum;
    int32_t areaNum2;
    int32_t contentsMask;
    float linkMins[SERVER_ENTITY_LINK_BOUNDS_COMPONENTS];
    float linkMaxs[SERVER_ENTITY_LINK_BOUNDS_COMPONENTS];
};

struct worldSector_s {
    int32_t axis;
    int32_t staticModelContentsMask;
    int32_t sightTraceStaticModelContentsMask;
    int32_t entityContentsMask;
    float dist;
    svEntity_t *entityLinkHead;
    worldSectorAreaLink_t *staticModelLinkHead;
    worldSector_t *parent;
    worldSector_t *children[SERVER_WORLD_SECTOR_CHILD_COUNT];
};

typedef entityState_t serverSnapshotEntity_t;

typedef struct serverCachedSnapshotClient_s {
    qboolean playerStateValid;
    clientState_t snapshot;
    playerState_t playerState;
} serverCachedSnapshotClient_t;

typedef struct serverCachedSnapshotFrame_s {
    int32_t archivedFrameIndex;
    int32_t messageTime;
    int32_t numEntities;
    int32_t firstEntity;
    int32_t numClients;
    int32_t firstClient;
    qboolean decodedFromDelta;
} serverCachedSnapshotFrame_t;

typedef struct challenge_s {
    netadr_t address;
    int32_t challengeNumber;
    int32_t slotTimestamp;
    int32_t pingStartTime;
    int32_t authorizeStartTime;
    int32_t firstPingMsec;
    qboolean connected;
    int32_t numericGuid;
    char authGuidString[SERVER_CHALLENGE_AUTH_GUID_SIZE];
    uint8_t unusedTail[4];
} challenge_t;

typedef struct serverAuthorizeGuidCacheEntry_s {
    int32_t numericGuid;
    int32_t cacheTime;
} serverAuthorizeGuidCacheEntry_t;

typedef struct serverReliableCommand_s {
    char commandText[MAX_STRING_CHARS];
    int32_t enqueueTime;
    qboolean reliable;
} serverReliableCommand_t;

typedef struct serverClientDownload_s {
    char fileName[MAX_QPATH];
    int32_t fileHandle;
    int32_t fileSize;
    int32_t bytesRead;
    int32_t nextAcknowledgmentBlock;
    int32_t nextBufferedBlock;
    int32_t nextTransmitBlock;
    uint8_t *blockData[MAX_DOWNLOAD_WINDOW];
    int32_t blockByteCounts[MAX_DOWNLOAD_WINDOW];
    qboolean eofBlockQueued;
    int32_t lastBlockActivityTime;
    qboolean redirectAllowedByClient;
    char redirectUrl[256];
    qboolean redirectActive;
    qboolean redirectAcknowledged;
    qboolean redirectFailed;
} serverClientDownload_t;

typedef struct clientSnapshot_s {
    playerState_t playerState;
    int32_t numEntities;
    int32_t numClients;
    int32_t firstEntity;
    int32_t firstClient;
    int32_t messageSentTime;
    int32_t messageAcknowledgedTime;
    int32_t messageSize;
} clientSnapshot_t;

typedef struct snapshotEntityNumbers_s {
    int32_t count;
    int32_t entityRefs[MAX_GENTITIES];
} snapshotEntityNumbers_t;

/* Native server-side client slot. Pointer-bearing fields widen with the host;
 * the i386 assertions below preserve the original server ABI. */
typedef struct client_s {
    serverClientState_t state;
    qboolean sendAsActive;
    const char *deferredDropReason;
    char userinfo[MAX_STRING_CHARS];
    serverReliableCommand_t reliableCommands[MAX_RELIABLE_COMMANDS];
    int32_t reliableSequence;
    int32_t reliableAcknowledge;
    int32_t reliableSent;
    int32_t messageAcknowledge;
    int32_t gamestateMessageNum;
    int32_t challenge;
    usercmd_t lastUsercmd;
    int32_t lastClientCommand;
    char lastClientCommandString[MAX_STRING_CHARS];
    sharedEntity_t *gentity;
    char name[MAX_NAME_LENGTH];
    serverClientDownload_t download;
    int32_t deltaMessage;
    int32_t nextReliableTime;
    int32_t lastPacketTime;
    int32_t lastConnectTime;
    int32_t nextSnapshotTime;
    qboolean rateDelayed;
    int32_t timeoutCount;
    clientSnapshot_t snapshotFrames[SERVER_CLIENT_SNAPSHOT_FRAME_COUNT];
    int32_t ping;
    int32_t rate;
    int32_t snapshotMsec;
    int32_t pureAuthState;
    netchan_t netchan;
    int32_t guid;
    uint16_t scriptId;
    uint8_t paddingAfterScriptId[2];
    qboolean isTestClient;
    int32_t serverId;
    char punkbusterGuid[SERVER_PUNKBUSTER_GUID_SIZE];
    uint8_t paddingAfterPunkbusterGuid[3];
} client_t;

typedef void (*sv_client_command_handler_fn_t)(client_t *client);

/* Windows and Linux SV_ExecuteClientCommand walk the same null-terminated
 * command/handler pair table with an eight-byte i386 stride. */
typedef struct sv_client_command_handler_s {
    const char *commandName;
    sv_client_command_handler_fn_t handler;
} sv_client_command_handler_t;

typedef struct archivedSnapshotFrameIndex_s {
    int32_t firstByte;
    int32_t byteCount;
} archivedSnapshotFrameIndex_t;

typedef struct serverStatic_s {
    qboolean initialized;
    int32_t time;
    int32_t realTime;
    int32_t snapFlagServerBit;
    client_t *clients;
    int32_t numEntityStateSnapshots;
    int32_t numClientSnapshots;
    int32_t nextEntityStateSnapshot;
    int32_t nextClientSnapshot;
    serverSnapshotEntity_t *entityStateSnapshots;
    clientState_t *clientSnapshots;
    qboolean archiveEnabled;
    int32_t nextArchivedSnapshotFrames;
    archivedSnapshotFrameIndex_t *archivedSnapshotFrames;
    uint8_t *archivedSnapshotBuffer;
    int32_t nextArchivedSnapshotBuffer;
    int32_t nextCachedSnapshotEntities;
    int32_t nextCachedSnapshotClients;
    int32_t nextCachedSnapshotFrames;
    archivedEntity_t *cachedSnapshotEntities;
    serverCachedSnapshotClient_t *cachedSnapshotClients;
    serverCachedSnapshotFrame_t *cachedSnapshotFrames;
    int32_t nextHeartbeatTime;
    int32_t nextStatusResponseTime;
    challenge_t challenges[MAX_CHALLENGES];
    netadr_t redirectAddress;
    netadr_t authorizeServerAddress;
    netProfileInfo_t *netProfile;
    serverAuthorizeGuidCacheEntry_t authorizeGuidCache[SERVER_AUTHORIZE_GUID_CACHE_ENTRY_COUNT];
} serverStatic_t;

typedef struct serverHeader_s {
    serverState_t state;
    qboolean restarting;
    int32_t serverId;
    int32_t gamestateChecksumFeed;
} serverHeader_t;

#define SERVER_RUNTIME_LAYOUT_ASSERT(name_, expression_) typedef char name_[(expression_) ? 1 : -1]

SERVER_RUNTIME_LAYOUT_ASSERT(q_shared_entity_state_offset, offsetof(sharedEntity_t, entityState) == 0x000);
SERVER_RUNTIME_LAYOUT_ASSERT(q_shared_entity_single_client_offset, offsetof(sharedEntity_t, singleClient) == 0x0fc);
SERVER_RUNTIME_LAYOUT_ASSERT(q_shared_entity_owner_offset, offsetof(sharedEntity_t, ownerNum) == 0x154);
SERVER_RUNTIME_LAYOUT_ASSERT(q_shared_entity_size, sizeof(sharedEntity_t) == 0x158);
SERVER_RUNTIME_LAYOUT_ASSERT(q_cached_snapshot_client_size, sizeof(serverCachedSnapshotClient_t) == 0x4564);
SERVER_RUNTIME_LAYOUT_ASSERT(q_cached_snapshot_frame_size, sizeof(serverCachedSnapshotFrame_t) == 0x1c);
SERVER_RUNTIME_LAYOUT_ASSERT(q_challenge_size, sizeof(challenge_t) == 0x54);
SERVER_RUNTIME_LAYOUT_ASSERT(q_snapshot_entity_numbers_size, sizeof(snapshotEntityNumbers_t) == 0x1004);
SERVER_RUNTIME_LAYOUT_ASSERT(q_server_header_size, sizeof(serverHeader_t) == 0x10);

#if UINTPTR_MAX == UINT32_MAX
SERVER_RUNTIME_LAYOUT_ASSERT(q_sv_client_command_handler_size, sizeof(sv_client_command_handler_t) == 0x08);
SERVER_RUNTIME_LAYOUT_ASSERT(q_world_sector_area_link_size, sizeof(worldSectorAreaLink_t) == 0x54);
SERVER_RUNTIME_LAYOUT_ASSERT(q_world_sector_size, sizeof(worldSector_t) == 0x28);
SERVER_RUNTIME_LAYOUT_ASSERT(q_sv_entity_size, sizeof(svEntity_t) == 0x180);
SERVER_RUNTIME_LAYOUT_ASSERT(q_server_reliable_command_size, sizeof(serverReliableCommand_t) == 0x408);
SERVER_RUNTIME_LAYOUT_ASSERT(q_server_download_size, sizeof(serverClientDownload_t) == 0x1b0);
SERVER_RUNTIME_LAYOUT_ASSERT(q_client_snapshot_size, sizeof(clientSnapshot_t) == 0x4520);
SERVER_RUNTIME_LAYOUT_ASSERT(q_server_client_size, sizeof(client_t) == 0xab0b4);
SERVER_RUNTIME_LAYOUT_ASSERT(q_server_static_size, sizeof(serverStatic_t) == 0x1510c);
#endif

#undef SERVER_RUNTIME_LAYOUT_ASSERT

#endif
