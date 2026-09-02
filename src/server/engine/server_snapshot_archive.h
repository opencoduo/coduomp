#ifndef SHARED_SERVER_SNAPSHOT_ARCHIVE_H
#define SHARED_SERVER_SNAPSHOT_ARCHIVE_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_EnableArchivedSnapshot(qboolean enabled);
void SV_InitArchivedSnapshot(void);
void SV_FreeArchivedSnapshot(void);

void SV_AddEntToSnapshot(int32_t entityNum, snapshotEntityNumbers_t *entityNumbers);
void SV_AddArchivedEntToSnapshot(int32_t entityNum, snapshotEntityNumbers_t *entityNumbers);
void SV_AddEntitiesVisibleFromPoint(const vec3_t origin, int32_t clientNum, snapshotEntityNumbers_t *entityNumbers);
void SV_AddCachedEntitiesVisibleFromPoint(int32_t numEntities, int32_t firstEntity, const vec3_t origin, int32_t clientNum,
                                          snapshotEntityNumbers_t *entityNumbers, const clientSnapshot_t *frame);

int32_t SV_GetClientArchiveTime(int32_t clientNum);
const clientState_t *SV_GetClientState(int32_t clientNum);
void SV_SetClientArchiveTime(int32_t clientNum, int32_t archiveTime);
serverCachedSnapshotFrame_t *SV_GetCachedSnapshotInternal(int32_t archivedFrameIndex);
serverCachedSnapshotFrame_t *SV_GetCachedSnapshot(int32_t *archiveTime);
qboolean SV_GetFollowPlayerState(int32_t clientNum, playerState_t *playerStateOut);
qboolean SV_GetCurrentClientInfo(int32_t clientNum, playerState_t *playerStateOut, clientState_t *clientInfoOut);
qboolean SV_GetArchivedClientInfo(int32_t clientNum, int32_t *archiveTime, playerState_t *playerStateOut, clientState_t *clientInfoOut);

void SV_BuildClientSnapshot(client_t *client);
void SV_ArchiveSnapshot(void);

#ifdef __cplusplus
}
#endif

#endif
