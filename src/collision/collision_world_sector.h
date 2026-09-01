#ifndef CODUO_COLLISION_WORLD_SECTOR_H
#define CODUO_COLLISION_WORLD_SECTOR_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: native recursive-query carrier.  The optimized
 * Windows caller uses these six consecutive fields.  The unoptimized Linux
 * caller leaves one dead stack dword immediately before them, but neither
 * original function initializes or reads that dword. */
typedef struct cmAreaEntitiesWork_s {
    const float *mins;
    const float *maxs;
    int32_t *entityList;
    int32_t count;
    int32_t maxCount;
    int32_t contentsMask;
} cmAreaEntitiesWork_t;

extern vec3_t cm_worldMins;
extern vec3_t cm_worldMaxs;
extern worldSector_t cm_worldSectorRoot;
extern worldSector_t *cm_freeWorldSectors;
extern worldSector_t cm_nullWorldSector;
extern worldSector_t cm_worldSectorPool[SERVER_WORLD_SECTOR_POOL_COUNT];

void CM_InitWorldSector(void);
worldSector_t *CM_AllocWorldSector(const vec2_t mins, const vec2_t maxs);
void CM_RebucketWorldSectorLinks(worldSector_t *sector,
                                 const vec2_t sectorMins,
                                 const vec2_t sectorMaxs);
void SV_UnlinkEntityFromWorldSector(svEntity_t *serverEntity);
void SV_LinkEntityToWorldSector(svEntity_t *serverEntity,
                                const vec2_t mins, const vec2_t maxs);
qboolean CM_IsBigStaticModel(const vec3_t mins, const vec3_t maxs);
void CM_LinkStaticModel(worldSectorAreaLink_t *areaLink);
void CM_AreaEntities_r(worldSector_t *sector, cmAreaEntitiesWork_t *work);
int32_t CM_AreaEntities(const vec3_t mins, const vec3_t maxs,
                        int32_t *entityList, int32_t maxEntityCount,
                        int32_t contentsMask);

#ifdef __cplusplus
}
#endif

#endif
