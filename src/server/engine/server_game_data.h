#ifndef SHARED_SERVER_GAME_DATA_H
#define SHARED_SERVER_GAME_DATA_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The loaded game module owns the complete entity and client records.  These
 * globals retain the module-supplied bases, strides, and active entity count. */
extern sharedEntity_t *sv_gentities;
extern int32_t sv_gentitySize;
extern int32_t sv_numGentities;
extern playerState_t *sv_gameClients;
extern int32_t sv_gameClientSize;
extern svEntity_t sv_entities[MAX_GENTITIES];

void SV_LocateGameData(sharedEntity_t *gentities, int32_t numGentities,
                       int32_t sizeofGentity, playerState_t *clients,
                       int32_t sizeofGameClient);
int32_t SV_NumForGentity(const sharedEntity_t *gentity);
sharedEntity_t *SV_GentityNum(int32_t entityNum);
playerState_t *SV_GameClientNum(int32_t clientNum);
svEntity_t *SV_SvEntityForGentity(const sharedEntity_t *gentity);
sharedEntity_t *SV_GEntityForSvEntity(const svEntity_t *serverEntity);

#ifdef __cplusplus
}
#endif

#endif
