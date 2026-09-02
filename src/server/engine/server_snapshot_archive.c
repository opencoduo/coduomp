#include "server_snapshot_archive.h"

#include "collision/collision_area.h"
#include "collision/collision_leaf_queries.h"
#include "collision/collision_queries.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/msg.h"
#include "qcommon/msg_delta.h"
#include "math/q_math.h"
#include "qcommon/q_memory.h"
#include "qcommon/qcommon_runtime_types.h"
#include "server_game_data.h"
#include "qcommon/vm_runtime.h"

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    SV_ENTITY_EFLAG_IN_VEHICLE = 0x100000,
    SV_VEHICLE_SEAT_MASK = 7,
    SV_HIDDEN_VEHICLE_TYPE = 2,
    SV_HIDDEN_VEHICLE_SEAT = 1,
    SV_ARCHIVED_VISIBILITY_MAX_LEAVES = 128,
    SV_PLAYERSTATE_FOLLOWING_FLAG = 0x40000,
    SV_ARCHIVE_MESSAGE_BUFFER_SIZE = 131072,
    SV_ARCHIVE_CLIENT_NUMBER_BITS = 6,
    SV_ARCHIVE_CLIENT_SORT_END = 9999,
    SV_ARCHIVE_NO_OLD_CLIENT = 99999,
    SV_SNAPSHOT_ENTITY_NUMBER_BITS = 10,
    SV_SNAPSHOT_ENTITY_END_NUMBER = MAX_GENTITIES - 1,
    SV_SNAPSHOT_COUNTER_WRAP_LIMIT = INT32_MAX - 1
};

extern serverHeader_t sv;
extern serverStatic_t svs;
extern cvar_t *sv_fps;
extern cvar_t *sv_maxclients;
extern vm_t *sv_gameVM;

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete archived-snapshot ownership and selection subsystem shared by the
 * Windows client engine and Linux dedicated engine. The supporting Mac client
 * supplies the canonical function names, including
 * SV_AddCachedEntitiesVisibleFromPoint.
 *
 * Function                                      Windows       Linux
 * SV_EnableArchivedSnapshot                     0x0045f7f0    0x08091999
 * SV_InitArchivedSnapshot                       0x0045f8f0    0x08091a14
 * SV_FreeArchivedSnapshot                       0x0045f920    0x08091a55
 * SV_AddEntToSnapshot                           0x00464750    0x08096b4b
 * SV_AddArchivedEntToSnapshot                   0x00464770    0x08096b71
 * SV_AddEntitiesVisibleFromPoint                0x00464790    0x08096b97
 * SV_GetClientArchiveTime                       0x00464a70    0x08096f2b
 * SV_AddCachedEntitiesVisibleFromPoint          0x00464a90    0x08096f4f
 * SV_GetClientState                             0x00464d10    0x08097250
 * SV_SetClientArchiveTime                       0x00464d30    0x08097274
 * SV_GetCachedSnapshotInternal                  0x00464d50    0x0809729f
 * SV_GetCachedSnapshot                          0x00465610    0x08097d7d
 * SV_GetFollowPlayerState                       0x004656c0    0x08097e9d
 * SV_GetCurrentClientInfo                       0x004656e0    0x08097ec8
 * SV_GetArchivedClientInfo                      0x00465740    0x08097f44
 * SV_BuildClientSnapshot                        0x00465930    0x080982cf
 * SV_ArchiveSnapshot                            0x00466160    0x08098e10
 *
 * The retained implementation expresses the common operation graph directly.
 * Compiler-only register allocation, inlining, CRT expansion, and signed
 * remainder lowering are not source-level platform variants.
 */

void SV_EnableArchivedSnapshot(qboolean enabled)
{
    svs.archiveEnabled = enabled;
    if (enabled == qfalse || svs.archivedSnapshotFrames != NULL) {
        return;
    }

    /*
     * Windows emits raw allocation followed by explicit zeroing. Linux calls
     * Z_MallocInternal, whose original body performs that same allocation,
     * out-of-memory check, and zeroing. The shared allocator owns that proven
     * contract, so each ring needs one call here.
     */
    svs.cachedSnapshotEntities = Z_MallocInternal(SERVER_CACHED_SNAPSHOT_ENTITY_COUNT * sizeof(*svs.cachedSnapshotEntities));
    svs.cachedSnapshotClients = Z_MallocInternal(SERVER_CACHED_SNAPSHOT_CLIENT_COUNT * sizeof(*svs.cachedSnapshotClients));
    svs.archivedSnapshotFrames = Z_MallocInternal(SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT * sizeof(*svs.archivedSnapshotFrames));
    svs.archivedSnapshotBuffer = Z_MallocInternal(SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE);
    svs.cachedSnapshotFrames = Z_MallocInternal(SERVER_CACHED_SNAPSHOT_FRAME_COUNT * sizeof(*svs.cachedSnapshotFrames));
}

void SV_InitArchivedSnapshot(void)
{
    svs.archiveEnabled = qfalse;
    svs.nextArchivedSnapshotFrames = 0;
    svs.nextArchivedSnapshotBuffer = 0;
    svs.nextCachedSnapshotEntities = 0;
    svs.nextCachedSnapshotClients = 0;
    svs.nextCachedSnapshotFrames = 0;
}

void SV_FreeArchivedSnapshot(void)
{
    if (svs.cachedSnapshotEntities != NULL) {
        Z_FreeInternal(svs.cachedSnapshotEntities);
        svs.cachedSnapshotEntities = NULL;
    }
    if (svs.cachedSnapshotClients != NULL) {
        Z_FreeInternal(svs.cachedSnapshotClients);
        svs.cachedSnapshotClients = NULL;
    }
    if (svs.archivedSnapshotFrames != NULL) {
        Z_FreeInternal(svs.archivedSnapshotFrames);
        svs.archivedSnapshotFrames = NULL;
    }
    if (svs.archivedSnapshotBuffer != NULL) {
        Z_FreeInternal(svs.archivedSnapshotBuffer);
        svs.archivedSnapshotBuffer = NULL;
    }
    if (svs.cachedSnapshotFrames != NULL) {
        Z_FreeInternal(svs.cachedSnapshotFrames);
        svs.cachedSnapshotFrames = NULL;
    }
}

/* Source: CoDUOMP.exe 0x00464750..0x00464760.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464750_00464761.mcode.
 * Name: exact same-module Mac symbol SV_AddEntToSnapshot. */
void SV_AddEntToSnapshot(int32_t entityNum, snapshotEntityNumbers_t *entityNumbers)
{
    if (entityNumbers->count != MAX_GENTITIES) {
        entityNumbers->entityRefs[entityNumbers->count] = entityNum;
        ++entityNumbers->count;
    }
}

/* Source: CoDUOMP.exe 0x00464770..0x00464780.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464770_00464781.mcode.
 * Name: exact same-module Mac symbol SV_AddArchivedEntToSnapshot. The stored
 * reference is relative to the archived frame's first cached entity. */
void SV_AddArchivedEntToSnapshot(int32_t entityNum, snapshotEntityNumbers_t *entityNumbers)
{
    if (entityNumbers->count != MAX_GENTITIES) {
        entityNumbers->entityRefs[entityNumbers->count] = entityNum;
        ++entityNumbers->count;
    }
}

/* Source: CoDUOMP.exe 0x00464790..0x00464a60.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464790_00464a61.mcode.
 * Name: exact same-module Mac symbol SV_AddEntitiesVisibleFromPoint. The
 * lastCluster equality below is unusual but literal: both this function and
 * SV_inSnapshot use that exact comparison in the Windows executable. */
void SV_AddEntitiesVisibleFromPoint(const vec3_t origin, int32_t clientNum, snapshotEntityNumbers_t *entityNumbers)
{
    const int32_t viewLeaf = CM_PointLeafnum(origin);
    const int32_t viewArea = CM_LeafArea(viewLeaf);
    if (viewArea < 0)
        return;

    const uint8_t *const pvs = CM_ClusterPVS(CM_LeafCluster(viewLeaf));
    const uint32_t fogDistanceBits = (uint32_t)VM_Call(sv_gameVM, GAME_GET_FOG_OPAQUE_DIST_SQ_BITS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    float fogDistanceSquared;
    memcpy(&fogDistanceSquared, &fogDistanceBits, sizeof(fogDistanceSquared));
    if (fogDistanceSquared == FLT_MAX)
        fogDistanceSquared = 0.0f;

    for (int32_t entityNum = 0; entityNum < sv_numGentities; ++entityNum) {
        sharedEntity_t *gentity = SV_GentityNum(entityNum);

        if (gentity->linked == qfalse)
            continue;

        if (gentity->entityState.eType == ET_SOUND_BLEND && gentity->entityState.otherEntityNum != ENTITYNUM_NONE) {
            gentity = SV_GentityNum(gentity->entityState.otherEntityNum);
        }

        if ((gentity->svFlags & SVF_NOCLIENT) != 0)
            continue;
        if ((gentity->svFlags & SVF_SINGLECLIENT) != 0 && gentity->singleClient != clientNum) {
            continue;
        }
        if ((gentity->svFlags & SVF_NOTSINGLECLIENT) != 0 && gentity->singleClient == clientNum) {
            continue;
        }
        if (entityNum == clientNum)
            continue;

        if ((gentity->svFlags & SVF_VISIBILITY_BYPASS_MASK) != 0) {
            SV_AddEntToSnapshot(entityNum, entityNumbers);
            continue;
        }

        if (gentity->soundTime != 0) {
            /* RECONSTRUCTION_FIX: both originals subtract svs.time and test
             * the sign of the wrapped dword (Windows 0x004648df, Linux
             * 0x08096d25). The former Windows recovery used a relational
             * comparison, which differs when the signed clock wraps. */
            if (gentity->soundTime < 0 || gentity->soundTime - svs.time >= 0) {
                SV_AddEntToSnapshot(entityNum, entityNumbers);
                continue;
            }
            gentity->soundTime = 0;
        }

        svEntity_t *const serverEntity = SV_SvEntityForGentity(gentity);

        if (gentity->entityState.eType == ET_PLAYER && (gentity->entityState.eFlags & SV_ENTITY_EFLAG_IN_VEHICLE) != 0 &&
            SV_GentityNum(gentity->ownerNum)->entityState.eventParm == SV_HIDDEN_VEHICLE_TYPE &&
            (gentity->entityState.eventParm & SV_VEHICLE_SEAT_MASK) == SV_HIDDEN_VEHICLE_SEAT) {
            continue;
        }

        if (CM_AreasConnected(viewArea, serverEntity->areaNum) == qfalse && CM_AreasConnected(viewArea, serverEntity->areaNum2) == qfalse) {
            continue;
        }
        if (serverEntity->numClusters == 0)
            continue;

        int32_t clusterIndex = 0;
        int32_t testCluster = 0;
        while (clusterIndex < serverEntity->numClusters) {
            testCluster = serverEntity->clusterNums[clusterIndex];
            if ((pvs[testCluster >> 3] & (1u << (testCluster & 7))) != 0) {
                break;
            }
            ++clusterIndex;
        }

        if (clusterIndex == serverEntity->numClusters) {
            if (serverEntity->lastCluster == 0)
                continue;

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            while (testCluster <= serverEntity->lastCluster && (pvs[testCluster >> 3] & (1u << (testCluster & 7))) == 0) {
                ++testCluster;
            }
            if (testCluster == serverEntity->lastCluster)
                continue;
        }

        if (fogDistanceSquared != 0.0f && BoxDistSqrdExceeds(gentity->absMin, gentity->absMax, origin, fogDistanceSquared) != qfalse) {
            continue;
        }

        SV_AddEntToSnapshot(entityNum, entityNumbers);
    }
}

/* Source: CoDUOMP.exe 0x00464a70..0x00464a80.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464a70_00464a81.mcode.
 * Name: exact same-module Mac symbol SV_GetClientArchiveTime. */
int32_t SV_GetClientArchiveTime(int32_t clientNum)
{
    return (int32_t)VM_Call(sv_gameVM, GAME_GET_CLIENT_ARCHIVE_TIME, clientNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00464a90..0x00464d00.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464a90_00464d01.mcode.
 * Name: exact same-module Mac symbol
 * SV_AddCachedEntitiesVisibleFromPoint. Entity numbers appended here are
 * indices relative to firstEntity, as consumed by the caller's archived ring
 * copy loop. */
void SV_AddCachedEntitiesVisibleFromPoint(int32_t numEntities, int32_t firstEntity, const vec3_t origin, int32_t clientNum,
                                          snapshotEntityNumbers_t *entityNumbers, const clientSnapshot_t *frame)
{
    const int32_t viewLeaf = CM_PointLeafnum(origin);
    const int32_t viewArea = CM_LeafArea(viewLeaf);
    if (viewArea < 0)
        return;

    const uint8_t *const pvs = CM_ClusterPVS(CM_LeafCluster(viewLeaf));
    const uint32_t fogDistanceBits = (uint32_t)VM_Call(sv_gameVM, GAME_GET_FOG_OPAQUE_DIST_SQ_BITS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    float fogDistanceSquared;
    memcpy(&fogDistanceSquared, &fogDistanceBits, sizeof(fogDistanceSquared));
    if (fogDistanceSquared == FLT_MAX)
        fogDistanceSquared = 0.0f;

    for (int32_t entityIndex = 0; entityIndex < numEntities; ++entityIndex) {
        archivedEntity_t *const entity = &svs.cachedSnapshotEntities[(firstEntity + entityIndex) % SERVER_CACHED_SNAPSHOT_ENTITY_COUNT];

        if (((uint32_t)entity->svFlags & SVF_SINGLECLIENT) != 0 && entity->singleClient != clientNum) {
            continue;
        }
        if (((uint32_t)entity->svFlags & SVF_NOTSINGLECLIENT) != 0 && entity->singleClient == clientNum) {
            continue;
        }
        if (entity->state.number == clientNum && (frame->playerState.playerStateFlags & SV_PLAYERSTATE_FOLLOWING_FLAG) != 0) {
            continue;
        }

        if (((uint32_t)entity->svFlags & SVF_VISIBILITY_BYPASS_MASK) != 0) {
            SV_AddArchivedEntToSnapshot(entityIndex, entityNumbers);
            continue;
        }

        int32_t leaves[SV_ARCHIVED_VISIBILITY_MAX_LEAVES];
        int32_t lastLeaf;
        const int32_t leafCount = CM_BoxLeafnums(entity->absmin, entity->absmax, leaves, SV_ARCHIVED_VISIBILITY_MAX_LEAVES, &lastLeaf);
        if (leafCount == 0)
            continue;

        int32_t leafIndex = 0;
        while (leafIndex < leafCount) {
            const int32_t leafArea = CM_LeafArea(leaves[leafIndex]);
            if (leafArea >= 0 && CM_AreasConnected(viewArea, leafArea) != qfalse) {
                break;
            }
            ++leafIndex;
        }
        if (leafIndex == leafCount)
            continue;

        leafIndex = 0;
        while (leafIndex < leafCount) {
            const int32_t cluster = CM_LeafCluster(leaves[leafIndex]);
            if (cluster != -1 && (pvs[cluster >> 3] & (1u << (cluster & 7))) != 0) {
                break;
            }
            ++leafIndex;
        }
        if (leafIndex == leafCount)
            continue;

        if (fogDistanceSquared != 0.0f && BoxDistSqrdExceeds(entity->absmin, entity->absmax, origin, fogDistanceSquared) != qfalse) {
            continue;
        }

        SV_AddArchivedEntToSnapshot(entityIndex, entityNumbers);
    }
}

/* Source: CoDUOMP.exe 0x00464d10..0x00464d20.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464d10_00464d21.mcode.
 * Name: exact same-module Mac symbol SV_GetClientState. */
const clientState_t *SV_GetClientState(int32_t clientNum)
{
    return (const clientState_t *)VM_Call(sv_gameVM, GAME_GET_CLIENT_STATE, clientNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00464d30..0x00464d41.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464d30_00464d42.mcode.
 * Name: exact same-module Mac symbol SV_SetClientArchiveTime. */
void SV_SetClientArchiveTime(int32_t clientNum, int32_t archiveTime)
{
    (void)VM_Call(sv_gameVM, GAME_SET_CLIENT_ARCHIVE_TIME, clientNum, archiveTime, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00464d50..0x00465609.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464d50_00465609.mcode.
 * Name: exact same-module Mac symbol SV_GetCachedSnapshotInternal. The
 * archived-frame index is the cache key at record +0x00; the actual
 * millisecond server time follows at +0x04. Delta archives recursively recover
 * their base frame before merging
 * the sorted client stream; entity records are always decoded relative to the
 * live server baseline. */
serverCachedSnapshotFrame_t *SV_GetCachedSnapshotInternal(int32_t archivedFrameIndex)
{
    archivedSnapshotFrameIndex_t *archivedFrame = &svs.archivedSnapshotFrames[archivedFrameIndex % SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT];

    if (archivedFrame->firstByte < svs.nextArchivedSnapshotBuffer - SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE) {
        return NULL;
    }

    int32_t oldestCachedFrame = svs.nextCachedSnapshotFrames - SERVER_CACHED_SNAPSHOT_FRAME_COUNT;
    if (oldestCachedFrame < 0)
        oldestCachedFrame = 0;

    for (int32_t frameIndex = svs.nextCachedSnapshotFrames - 1; frameIndex >= oldestCachedFrame; --frameIndex) {
        serverCachedSnapshotFrame_t *const cachedFrame = &svs.cachedSnapshotFrames[frameIndex % SERVER_CACHED_SNAPSHOT_FRAME_COUNT];
        if (cachedFrame->archivedFrameIndex != archivedFrameIndex)
            continue;

        if (cachedFrame->firstEntity >= svs.nextCachedSnapshotEntities - SERVER_CACHED_SNAPSHOT_ENTITY_COUNT &&
            cachedFrame->firstClient >= svs.nextCachedSnapshotClients - SERVER_CACHED_SNAPSHOT_CLIENT_COUNT) {
            return cachedFrame;
        }
        break;
    }

    uint8_t archivePayload[SV_ARCHIVE_MESSAGE_BUFFER_SIZE];
    msg_t message;
    MSG_Init(&message, archivePayload, sizeof(archivePayload));

    const int32_t byteCount = archivedFrame->byteCount;
    message.cursize = byteCount;

    const int32_t wrappedFirstByte = archivedFrame->firstByte % SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE;
    const int32_t firstCopyBytes = SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE - wrappedFirstByte;
    if (byteCount > firstCopyBytes) {
        memcpy(archivePayload, &svs.archivedSnapshotBuffer[wrappedFirstByte], (size_t)firstCopyBytes);
        memcpy(&archivePayload[firstCopyBytes], svs.archivedSnapshotBuffer, (size_t)(byteCount - firstCopyBytes));
    } else {
        memcpy(archivePayload, &svs.archivedSnapshotBuffer[wrappedFirstByte], (size_t)byteCount);
    }

    serverCachedSnapshotFrame_t *cachedFrame;
    if (MSG_ReadBit(&message) == 0) {
        const int32_t deltaFrameIndex = MSG_ReadLong(&message);
        if (deltaFrameIndex < svs.nextArchivedSnapshotFrames - SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT) {
            return NULL;
        }

        archivedFrame = &svs.archivedSnapshotFrames[deltaFrameIndex % SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT];
        if (archivedFrame->firstByte < svs.nextArchivedSnapshotBuffer - SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE) {
            return NULL;
        }

        serverCachedSnapshotFrame_t *const oldFrame = SV_GetCachedSnapshotInternal(deltaFrameIndex);
        if (oldFrame == NULL)
            return NULL;

        cachedFrame = &svs.cachedSnapshotFrames[svs.nextCachedSnapshotFrames % SERVER_CACHED_SNAPSHOT_FRAME_COUNT];
        cachedFrame->archivedFrameIndex = archivedFrameIndex;
        cachedFrame->numEntities = 0;
        cachedFrame->firstEntity = svs.nextCachedSnapshotEntities;
        cachedFrame->numClients = 0;
        cachedFrame->firstClient = svs.nextCachedSnapshotClients;
        cachedFrame->decodedFromDelta = qtrue;
        cachedFrame->messageTime = MSG_ReadLong(&message);

        int32_t oldClientIndex = 0;
        int32_t oldClientNum;
        serverCachedSnapshotClient_t *oldClient;
        if (oldFrame->numClients < 1) {
            oldClient = NULL;
            oldClientNum = SV_ARCHIVE_NO_OLD_CLIENT;
        } else {
            oldClient = &svs.cachedSnapshotClients[oldFrame->firstClient % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
            oldClientNum = oldClient->snapshot.clientNum;
        }

        while (MSG_ReadBit(&message) != 0) {
            const int32_t clientNum = MSG_ReadBits(&message, SV_ARCHIVE_CLIENT_NUMBER_BITS);
            if (message.readcount > byteCount) {
                Com_Error(ERR_DROP, "\x15"
                                    "SV_GetCachedSnapshot: end of message");
            }

            while (oldClientNum < clientNum) {
                ++oldClientIndex;
                if (oldClientIndex < oldFrame->numClients) {
                    oldClient = &svs.cachedSnapshotClients[(oldFrame->firstClient + oldClientIndex) % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
                    oldClientNum = oldClient->snapshot.clientNum;
                } else {
                    oldClient = NULL;
                    oldClientNum = SV_ARCHIVE_NO_OLD_CLIENT;
                }
            }

            serverCachedSnapshotClient_t *const newClient =
                &svs.cachedSnapshotClients[svs.nextCachedSnapshotClients % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
            if (oldClientNum == clientNum) {
                MSG_ReadDeltaClient(&message, &oldClient->snapshot, &newClient->snapshot, clientNum);
                newClient->playerStateValid = MSG_ReadBit(&message);
                if (newClient->playerStateValid != qfalse) {
                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    MSG_ReadDeltaPlayerstate(&message, &oldClient->playerState, &newClient->playerState);
                }

                ++oldClientIndex;
                if (oldClientIndex < oldFrame->numClients) {
                    oldClient = &svs.cachedSnapshotClients[(oldFrame->firstClient + oldClientIndex) % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
                    oldClientNum = oldClient->snapshot.clientNum;
                } else {
                    oldClient = NULL;
                    oldClientNum = SV_ARCHIVE_NO_OLD_CLIENT;
                }
            } else {
                MSG_ReadDeltaClient(&message, NULL, &newClient->snapshot, clientNum);
                newClient->playerStateValid = MSG_ReadBit(&message);
                if (newClient->playerStateValid != qfalse) {
                    MSG_ReadDeltaPlayerstate(&message, NULL, &newClient->playerState);
                }
            }

            ++svs.nextCachedSnapshotClients;
            if (svs.nextCachedSnapshotClients >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
                Com_Error(ERR_FATAL, "\x15"
                                     "svs.nextCachedSnapshotClients wrapped");
            }
            ++cachedFrame->numClients;
        }
    } else {
        cachedFrame = &svs.cachedSnapshotFrames[svs.nextCachedSnapshotFrames % SERVER_CACHED_SNAPSHOT_FRAME_COUNT];
        cachedFrame->archivedFrameIndex = archivedFrameIndex;
        cachedFrame->numEntities = 0;
        cachedFrame->firstEntity = svs.nextCachedSnapshotEntities;
        cachedFrame->numClients = 0;
        cachedFrame->firstClient = svs.nextCachedSnapshotClients;
        cachedFrame->decodedFromDelta = qfalse;
        cachedFrame->messageTime = MSG_ReadLong(&message);

        while (MSG_ReadBit(&message) != 0) {
            const int32_t clientNum = MSG_ReadBits(&message, SV_ARCHIVE_CLIENT_NUMBER_BITS);
            if (message.readcount > byteCount) {
                Com_Error(ERR_DROP, "\x15"
                                    "SV_GetCachedSnapshot: end of message");
            }

            serverCachedSnapshotClient_t *const newClient =
                &svs.cachedSnapshotClients[svs.nextCachedSnapshotClients % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
            MSG_ReadDeltaClient(&message, NULL, &newClient->snapshot, clientNum);
            newClient->playerStateValid = MSG_ReadBit(&message);
            if (newClient->playerStateValid != qfalse) {
                MSG_ReadDeltaPlayerstate(&message, NULL, &newClient->playerState);
            }

            ++svs.nextCachedSnapshotClients;
            if (svs.nextCachedSnapshotClients >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
                Com_Error(ERR_FATAL, "\x15"
                                     "svs.nextCachedSnapshotClients wrapped");
            }
            ++cachedFrame->numClients;
        }
    }

    for (;;) {
        const int32_t entityNum = MSG_ReadBits(&message, SV_SNAPSHOT_ENTITY_NUMBER_BITS);
        if (entityNum == SV_SNAPSHOT_ENTITY_END_NUMBER)
            break;

        if (message.readcount > byteCount) {
            Com_Error(ERR_DROP, "\x15"
                                "SV_GetCachedSnapshot: end of message");
        }

        archivedEntity_t *const entity = &svs.cachedSnapshotEntities[svs.nextCachedSnapshotEntities % SERVER_CACHED_SNAPSHOT_ENTITY_COUNT];
        (void)MSG_ReadDeltaArchivedEntity(&message, &sv_entities[entityNum].baseline, entity, entityNum);

        ++svs.nextCachedSnapshotEntities;
        if (svs.nextCachedSnapshotEntities >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
            Com_Error(ERR_FATAL, "\x15"
                                 "svs.nextCachedSnapshotEntities wrapped");
        }
        ++cachedFrame->numEntities;
    }

    ++svs.nextCachedSnapshotFrames;
    if (svs.nextCachedSnapshotFrames >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
        Com_Error(ERR_FATAL, "\x15"
                             "svs.nextCachedSnapshotFrames wrapped");
    }

    return cachedFrame;
}

/* Source: CoDUOMP.exe 0x00465610..0x004656b6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00465610_004656b6.mcode.
 * Name: exact same-module Mac symbol SV_GetCachedSnapshot. archiveTime is an
 * in/out millisecond offset: clamping an unavailable request rewrites it to
 * the oldest representable archive time, while complete lookup failure clears
 * it to zero. */
serverCachedSnapshotFrame_t *SV_GetCachedSnapshot(int32_t *archiveTime)
{
    if (svs.archiveEnabled == qfalse || *archiveTime <= 0)
        return NULL;

    const int32_t snapshotFramesPerSecond = sv_fps->integer;
    int32_t archivedFrameIndex = svs.nextArchivedSnapshotFrames - (*archiveTime * snapshotFramesPerSecond) / 1000;

    const int32_t oldestArchivedFrame = svs.nextArchivedSnapshotFrames - SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT;
    if (archivedFrameIndex < oldestArchivedFrame) {
        archivedFrameIndex = oldestArchivedFrame;
        *archiveTime = ((svs.nextArchivedSnapshotFrames - archivedFrameIndex) * 1000) / snapshotFramesPerSecond;
    }

    if (archivedFrameIndex < 0) {
        archivedFrameIndex = 0;
        *archiveTime = (svs.nextArchivedSnapshotFrames * 1000) / snapshotFramesPerSecond;
    }

    while (archivedFrameIndex < svs.nextArchivedSnapshotFrames) {
        serverCachedSnapshotFrame_t *const cachedFrame = SV_GetCachedSnapshotInternal(archivedFrameIndex);
        if (cachedFrame != NULL)
            return cachedFrame;
        ++archivedFrameIndex;
    }

    *archiveTime = 0;
    return NULL;
}

/* Source: CoDUOMP.exe 0x004656c0..0x004656d1, recovered from an exporter
 * function-boundary gap.
 * Name and signature: exact same-module Mac symbol
 * SV_GetFollowPlayerState. The Windows internal register convention supplies
 * clientNum in ECX and playerStateOut in EAX before forwarding VM command 10. */
qboolean SV_GetFollowPlayerState(int32_t clientNum, playerState_t *playerStateOut)
{
    return (qboolean)VM_Call(sv_gameVM, GAME_GET_FOLLOW_PLAYER_STATE, clientNum, (intptr_t)playerStateOut, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x004656e0..0x00465738.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004656e0_00465738.mcode.
 * Name: exact same-module Mac symbol SV_GetCurrentClientInfo. The active-state
 * gate is read from the engine client slot; player state and client state are
 * then obtained through game-VM commands 10 and 19 respectively. */
qboolean SV_GetCurrentClientInfo(int32_t clientNum, playerState_t *playerStateOut, clientState_t *clientInfoOut)
{
    if (svs.clients[clientNum].state != CS_ACTIVE)
        return qfalse;

    if (SV_GetFollowPlayerState(clientNum, playerStateOut) == qfalse) {
        return qfalse;
    }

    const clientState_t *const currentClientState = SV_GetClientState(clientNum);
    memcpy(clientInfoOut, currentClientState, sizeof(*clientInfoOut));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00465740..0x0046592c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00465740_0046592c.mcode.
 * Name: exact same-module Mac symbol SV_GetArchivedClientInfo. Cached player
 * states retain their original snapshot clock, so every nonzero absolute
 * timestamp proven by the machine code is rebased to current server time
 * after the record copy. deltaTime itself is adjusted unconditionally. */
qboolean SV_GetArchivedClientInfo(int32_t clientNum, int32_t *archiveTime, playerState_t *playerStateOut, clientState_t *clientInfoOut)
{
    serverCachedSnapshotFrame_t *const cachedFrame = SV_GetCachedSnapshot(archiveTime);
    if (cachedFrame == NULL) {
        if (*archiveTime <= 0) {
            return SV_GetCurrentClientInfo(clientNum, playerStateOut, clientInfoOut);
        }
        return qfalse;
    }

    const int32_t timeDelta = svs.time - cachedFrame->messageTime;
    for (int32_t clientIndex = 0; clientIndex < cachedFrame->numClients; ++clientIndex) {
        const serverCachedSnapshotClient_t *const cachedClient =
            &svs.cachedSnapshotClients[(cachedFrame->firstClient + clientIndex) % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
        if (cachedClient->snapshot.clientNum != clientNum)
            continue;
        if (cachedClient->playerStateValid == qfalse)
            return qfalse;

        *playerStateOut = cachedClient->playerState;
        memcpy(clientInfoOut, &cachedClient->snapshot, sizeof(*clientInfoOut));

        if (playerStateOut->commandTime != 0)
            playerStateOut->commandTime += timeDelta;
        if (playerStateOut->pmTime != 0)
            playerStateOut->pmTime += timeDelta;
        if (playerStateOut->foliageSoundTime != 0)
            playerStateOut->foliageSoundTime += timeDelta;
        if (playerStateOut->fatigueSoundTime != 0)
            playerStateOut->fatigueSoundTime += timeDelta;
        if (playerStateOut->lastJumpCommandTime != 0)
            playerStateOut->lastJumpCommandTime += timeDelta;
        if (playerStateOut->viewHeightLerpTime != 0)
            playerStateOut->viewHeightLerpTime += timeDelta;
        if (playerStateOut->motionState.shellshock.time != 0) {
            playerStateOut->motionState.shellshock.time += timeDelta;
        }

        for (int32_t hudIndex = 0; hudIndex < PLAYERSTATE_HUD_ELEM_COUNT; ++hudIndex) {
            hudElem_t *const hud = &playerStateOut->hudArchival[hudIndex];
            if (hud->timerValue != 0)
                hud->timerValue += timeDelta;
            if (hud->fadeStartTime != 0)
                hud->fadeStartTime += timeDelta;
            if (hud->scaleStartTime != 0)
                hud->scaleStartTime += timeDelta;
            if (hud->moveStartTime != 0)
                hud->moveStartTime += timeDelta;
        }

        playerStateOut->deltaTime += timeDelta;
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00465930..0x00465d31.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00465930_00465d31.mcode.
 * Name: exact same-module Mac symbol SV_BuildClientSnapshot. The current
 * player state always supplies the view origin and followed-player state;
 * archive mode changes only the entity/client records selected for this
 * outgoing snapshot and rebases the four proven entity timestamps. */
void SV_BuildClientSnapshot(client_t *client)
{
    clientSnapshot_t *const snapshotFrame =
        &client->snapshotFrames[client->netchan.outgoingSequence & (SERVER_CLIENT_SNAPSHOT_FRAME_COUNT - 1)];
    snapshotFrame->numEntities = 0;
    snapshotFrame->numClients = 0;

    if (client->gentity == NULL || client->state == CS_ZOMBIE)
        return;

    snapshotFrame->firstEntity = svs.nextEntityStateSnapshot;
    snapshotFrame->firstClient = svs.nextClientSnapshot;
    if (sv.state != SS_GAME)
        return;

    snapshotEntityNumbers_t visibleEntities;
    visibleEntities.count = 0;

    const int32_t clientNum = (int32_t)(client - svs.clients);
    int32_t archiveTime = SV_GetClientArchiveTime(clientNum);
    serverCachedSnapshotFrame_t *const archiveFrame = SV_GetCachedSnapshot(&archiveTime);
    SV_SetClientArchiveTime(clientNum, archiveTime);

    const int32_t archiveTimeDelta = archiveFrame != NULL ? svs.time - archiveFrame->messageTime : 0;

    const playerState_t *const gamePlayerState = SV_GameClientNum(clientNum);
    snapshotFrame->playerState = *gamePlayerState;

    const int32_t clientEntityNum = snapshotFrame->playerState.psClientNum;
    if (clientEntityNum < 0 || clientEntityNum >= MAX_GENTITIES) {
        Com_Error(ERR_DROP, "\x15"
                            "SV_BuildClientSnapshot: bad gEnt");
    }

    vec3_t viewOrigin = {snapshotFrame->playerState.psOrigin[0], snapshotFrame->playerState.psOrigin[1],
                         snapshotFrame->playerState.psOrigin[2] + snapshotFrame->playerState.viewHeightCurrent};
    AddLeanToPosition(viewOrigin, snapshotFrame->playerState.viewAngles[1], snapshotFrame->playerState.leanFraction, 16.0f, 20.0f);

    if (archiveFrame == NULL) {
        SV_AddEntitiesVisibleFromPoint(viewOrigin, clientEntityNum, &visibleEntities);

        for (int32_t visibleIndex = 0; visibleIndex < visibleEntities.count; ++visibleIndex) {
            const sharedEntity_t *const gentity = SV_GentityNum(visibleEntities.entityRefs[visibleIndex]);
            serverSnapshotEntity_t *const destination =
                &svs.entityStateSnapshots[svs.nextEntityStateSnapshot % svs.numEntityStateSnapshots];
            *destination = gentity->entityState;

            ++svs.nextEntityStateSnapshot;
            if (svs.nextEntityStateSnapshot >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
                Com_Error(ERR_FATAL, "\x15"
                                     "svs.nextSnapshotEntities wrapped");
            }
            ++snapshotFrame->numEntities;
        }

        for (int32_t snapshotClientNum = 0; snapshotClientNum < sv_maxclients->integer; ++snapshotClientNum) {
            if (svs.clients[snapshotClientNum].state < CS_CONNECTED) {
                continue;
            }

            clientState_t *const destination = &svs.clientSnapshots[svs.nextClientSnapshot % svs.numClientSnapshots];
            *destination = *SV_GetClientState(snapshotClientNum);

            ++svs.nextClientSnapshot;
            if (svs.nextClientSnapshot >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
                Com_Error(ERR_FATAL, "\x15"
                                     "svs.nextSnapshotClients wrapped");
            }
            ++snapshotFrame->numClients;
        }
        return;
    }

    SV_AddCachedEntitiesVisibleFromPoint(archiveFrame->numEntities, archiveFrame->firstEntity, viewOrigin, clientEntityNum,
                                         &visibleEntities, snapshotFrame);

    for (int32_t visibleIndex = 0; visibleIndex < visibleEntities.count; ++visibleIndex) {
        const archivedEntity_t *const archivedEntity =
            &svs.cachedSnapshotEntities[(archiveFrame->firstEntity + visibleEntities.entityRefs[visibleIndex]) %
                                        SERVER_CACHED_SNAPSHOT_ENTITY_COUNT];
        serverSnapshotEntity_t *const destination = &svs.entityStateSnapshots[svs.nextEntityStateSnapshot % svs.numEntityStateSnapshots];
        *destination = archivedEntity->state;

        if (destination->pos.trTime != 0)
            destination->pos.trTime += archiveTimeDelta;
        if (destination->apos.trTime != 0)
            destination->apos.trTime += archiveTimeDelta;
        if (destination->time != 0)
            destination->time += archiveTimeDelta;
        if (destination->time2 != 0)
            destination->time2 += archiveTimeDelta;

        ++svs.nextEntityStateSnapshot;
        if (svs.nextEntityStateSnapshot >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
            Com_Error(ERR_FATAL, "\x15"
                                 "svs.nextSnapshotEntities wrapped");
        }
        ++snapshotFrame->numEntities;
    }

    for (int32_t archivedClientIndex = 0; archivedClientIndex < archiveFrame->numClients; ++archivedClientIndex) {
        const serverCachedSnapshotClient_t *const archivedClient =
            &svs.cachedSnapshotClients[(archiveFrame->firstClient + archivedClientIndex) % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
        clientState_t *const destination = &svs.clientSnapshots[svs.nextClientSnapshot % svs.numClientSnapshots];
        *destination = archivedClient->snapshot;

        ++svs.nextClientSnapshot;
        if (svs.nextClientSnapshot >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
            Com_Error(ERR_FATAL, "\x15"
                                 "svs.nextSnapshotClients wrapped");
        }
        ++snapshotFrame->numClients;
    }
}
/* Source: CoDUOMP.exe 0x00466160..0x00466bc7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00466160_00466bc7.mcode.
 * Name: exact same-module Mac symbol SV_ArchiveSnapshot. A live current-server
 * cache frame supplies the delta baseline; otherwise this frame is serialized
 * in full and retained in the entity/client/frame cache rings. Both forms are
 * then copied into the long-lived circular archive byte buffer. */
void SV_ArchiveSnapshot(void)
{
    if (sv.state != SS_GAME || svs.archiveEnabled == qfalse)
        return;

    uint8_t archivePayload[SV_ARCHIVE_MESSAGE_BUFFER_SIZE];
    msg_t message;
    MSG_Init(&message, archivePayload, sizeof(archivePayload));

    int32_t oldestCachedFrame = svs.nextCachedSnapshotFrames - SERVER_CACHED_SNAPSHOT_FRAME_COUNT;
    if (oldestCachedFrame < 0)
        oldestCachedFrame = 0;

    const int32_t minimumArchivedFrame = svs.nextArchivedSnapshotFrames - sv_fps->integer;
    serverCachedSnapshotFrame_t *deltaFrame = NULL;
    qboolean useDelta = qfalse;
    for (int32_t frameIndex = svs.nextCachedSnapshotFrames - 1; frameIndex >= oldestCachedFrame; --frameIndex) {
        deltaFrame = &svs.cachedSnapshotFrames[frameIndex % SERVER_CACHED_SNAPSHOT_FRAME_COUNT];
        if (deltaFrame->archivedFrameIndex < minimumArchivedFrame || deltaFrame->decodedFromDelta != qfalse) {
            continue;
        }

        if (deltaFrame->firstEntity >= svs.nextCachedSnapshotEntities - SERVER_CACHED_SNAPSHOT_ENTITY_COUNT &&
            deltaFrame->firstClient >= svs.nextCachedSnapshotClients - SERVER_CACHED_SNAPSHOT_CLIENT_COUNT) {
            useDelta = qtrue;
        }
        break;
    }

    if (useDelta != qfalse) {
        MSG_WriteBit0(&message);
        MSG_WriteLong(&message, deltaFrame->archivedFrameIndex);
        MSG_WriteLong(&message, svs.time);

        int32_t clientNum = 0;
        int32_t oldClientIndex = 0;
        while (clientNum < sv_maxclients->integer || oldClientIndex < deltaFrame->numClients) {
            if (clientNum < sv_maxclients->integer && svs.clients[clientNum].state < CS_CONNECTED) {
                ++clientNum;
                continue;
            }

            const serverCachedSnapshotClient_t *oldClient = NULL;
            int32_t oldClientNum = SV_ARCHIVE_CLIENT_SORT_END;
            if (oldClientIndex < deltaFrame->numClients) {
                oldClient = &svs.cachedSnapshotClients[(deltaFrame->firstClient + oldClientIndex) % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
                oldClientNum = oldClient->snapshot.clientNum;
            }

            if (clientNum == oldClientNum) {
                MSG_WriteDeltaClient(&message, &oldClient->snapshot, SV_GetClientState(clientNum), qtrue);

                playerState_t playerState;
                const qboolean playerStateValid = (qboolean)VM_Call(sv_gameVM, GAME_GET_FOLLOW_PLAYER_STATE, clientNum,
                                                                    (intptr_t)&playerState, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                if (playerStateValid == qfalse) {
                    MSG_WriteBit0(&message);
                } else {
                    MSG_WriteBit1(&message);
                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    MSG_WriteDeltaPlayerstate(&message, &oldClient->playerState, &playerState);
                }

                ++oldClientIndex;
                ++clientNum;
            } else if (clientNum < oldClientNum) {
                MSG_WriteDeltaClient(&message, NULL, SV_GetClientState(clientNum), qtrue);

                playerState_t playerState;
                const qboolean playerStateValid = (qboolean)VM_Call(sv_gameVM, GAME_GET_FOLLOW_PLAYER_STATE, clientNum,
                                                                    (intptr_t)&playerState, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                if (playerStateValid == qfalse) {
                    MSG_WriteBit0(&message);
                } else {
                    MSG_WriteBit1(&message);
                    MSG_WriteDeltaPlayerstate(&message, NULL, &playerState);
                }

                ++clientNum;
            } else {
                ++oldClientIndex;
            }
        }
        MSG_WriteBit0(&message);

        for (int32_t entityNum = 0; entityNum < sv_numGentities; ++entityNum) {
            sharedEntity_t *const gentity = SV_GentityNum(entityNum);
            if (gentity->linked == qfalse || (gentity->svFlags & SVF_NOCLIENT) != 0) {
                continue;
            }

            svEntity_t *const serverEntity = SV_SvEntityForGentity(gentity);
            if ((gentity->svFlags & SVF_VISIBILITY_BYPASS_MASK) == 0 && serverEntity->numClusters == 0 && gentity->soundTime == 0) {
                continue;
            }

            archivedEntity_t archivedEntity;
            archivedEntity.state = gentity->entityState;
            archivedEntity.svFlags = (int32_t)gentity->svFlags;
            if (gentity->soundTime != 0) {
                archivedEntity.svFlags |= SVF_LOOPED_FX;
            }
            archivedEntity.singleClient = gentity->singleClient;
            memcpy(archivedEntity.absmin, gentity->absMin, sizeof(archivedEntity.absmin));
            memcpy(archivedEntity.absmax, gentity->absMax, sizeof(archivedEntity.absmax));

            MSG_WriteDeltaArchivedEntity(&message, &sv_entities[gentity->entityState.number].baseline, &archivedEntity, qtrue);
        }
    } else {
        MSG_WriteBit1(&message);
        MSG_WriteLong(&message, svs.time);

        serverCachedSnapshotFrame_t *const cachedFrame =
            &svs.cachedSnapshotFrames[svs.nextCachedSnapshotFrames % SERVER_CACHED_SNAPSHOT_FRAME_COUNT];
        cachedFrame->archivedFrameIndex = svs.nextArchivedSnapshotFrames;
        cachedFrame->messageTime = svs.time;
        cachedFrame->numEntities = 0;
        cachedFrame->firstEntity = svs.nextCachedSnapshotEntities;
        cachedFrame->numClients = 0;
        cachedFrame->firstClient = svs.nextCachedSnapshotClients;
        cachedFrame->decodedFromDelta = qfalse;

        for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
            if (svs.clients[clientNum].state < CS_CONNECTED)
                continue;

            serverCachedSnapshotClient_t *const newClient =
                &svs.cachedSnapshotClients[svs.nextCachedSnapshotClients % SERVER_CACHED_SNAPSHOT_CLIENT_COUNT];
            newClient->snapshot = *SV_GetClientState(clientNum);
            MSG_WriteDeltaClient(&message, NULL, &newClient->snapshot, qtrue);

            newClient->playerStateValid = (qboolean)VM_Call(sv_gameVM, GAME_GET_FOLLOW_PLAYER_STATE, clientNum,
                                                            (intptr_t)&newClient->playerState, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            if (newClient->playerStateValid == qfalse) {
                MSG_WriteBit0(&message);
            } else {
                MSG_WriteBit1(&message);
                MSG_WriteDeltaPlayerstate(&message, NULL, &newClient->playerState);
            }

            ++svs.nextCachedSnapshotClients;
            if (svs.nextCachedSnapshotClients >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
                Com_Error(ERR_FATAL, "\x15"
                                     "svs.nextCachedSnapshotClients wrapped");
            }
            ++cachedFrame->numClients;
        }
        MSG_WriteBit0(&message);

        for (int32_t entityNum = 0; entityNum < sv_numGentities; ++entityNum) {
            sharedEntity_t *const gentity = SV_GentityNum(entityNum);
            if (gentity->linked == qfalse || (gentity->svFlags & SVF_NOCLIENT) != 0) {
                continue;
            }

            const int32_t stateNumber = gentity->entityState.number;
            if (stateNumber < 0 || stateNumber >= MAX_GENTITIES) {
                Com_Error(ERR_DROP, "\x15"
                                    "SV_SvEntityForGentity: bad gEnt");
            }

            svEntity_t *const serverEntity = &sv_entities[stateNumber];
            if ((gentity->svFlags & SVF_VISIBILITY_BYPASS_MASK) == 0 && serverEntity->numClusters == 0 && gentity->soundTime == 0) {
                continue;
            }

            archivedEntity_t *const archivedEntity =
                &svs.cachedSnapshotEntities[svs.nextCachedSnapshotEntities % SERVER_CACHED_SNAPSHOT_ENTITY_COUNT];
            archivedEntity->state = gentity->entityState;
            archivedEntity->svFlags = (int32_t)gentity->svFlags;
            if (gentity->soundTime != 0) {
                archivedEntity->svFlags |= SVF_LOOPED_FX;
            }
            archivedEntity->singleClient = gentity->singleClient;
            memcpy(archivedEntity->absmin, gentity->absMin, sizeof(archivedEntity->absmin));
            memcpy(archivedEntity->absmax, gentity->absMax, sizeof(archivedEntity->absmax));

            MSG_WriteDeltaArchivedEntity(&message, &serverEntity->baseline, archivedEntity, qtrue);

            ++svs.nextCachedSnapshotEntities;
            if (svs.nextCachedSnapshotEntities >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
                Com_Error(ERR_FATAL, "\x15"
                                     "svs.nextCachedSnapshotEntities wrapped");
            }
            ++cachedFrame->numEntities;
        }

        ++svs.nextCachedSnapshotFrames;
        if (svs.nextCachedSnapshotFrames >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
            Com_Error(ERR_FATAL, "\x15"
                                 "svs.nextCachedSnapshotFrames wrapped");
        }
    }

    MSG_WriteBits(&message, SV_SNAPSHOT_ENTITY_END_NUMBER, SV_SNAPSHOT_ENTITY_NUMBER_BITS);

    archivedSnapshotFrameIndex_t *const archivedFrame =
        &svs.archivedSnapshotFrames[svs.nextArchivedSnapshotFrames % SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT];
    archivedFrame->firstByte = svs.nextArchivedSnapshotBuffer;
    archivedFrame->byteCount = message.cursize;

    const int32_t wrappedFirstByte = svs.nextArchivedSnapshotBuffer % SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE;
    svs.nextArchivedSnapshotBuffer += message.cursize;
    if (svs.nextArchivedSnapshotBuffer >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
        Com_Error(ERR_FATAL, "\x15"
                             "svs.nextArchivedSnapshotBuffer wrapped");
    }

    const int32_t firstCopyBytes = SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE - wrappedFirstByte;
    if (message.cursize > firstCopyBytes) {
        memcpy(&svs.archivedSnapshotBuffer[wrappedFirstByte], archivePayload, (size_t)firstCopyBytes);
        memcpy(svs.archivedSnapshotBuffer, &archivePayload[firstCopyBytes], (size_t)(message.cursize - firstCopyBytes));
    } else {
        memcpy(&svs.archivedSnapshotBuffer[wrappedFirstByte], archivePayload, (size_t)message.cursize);
    }

    ++svs.nextArchivedSnapshotFrames;
    if (svs.nextArchivedSnapshotFrames >= SV_SNAPSHOT_COUNTER_WRAP_LIMIT) {
        Com_Error(ERR_FATAL, "\x15"
                             "svs.nextArchivedSnapshotFrames wrapped");
    }
}
