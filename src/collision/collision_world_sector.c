#include "collision_world_sector.h"

#include "collision_queries.h"
#include "server/engine/server_game_data.h"
#include "animation/xmodel.h"

#include <stddef.h>

enum {
    CM_WORLD_SECTOR_MIN_SPLIT_SIZE = 512,
    CM_BIG_STATIC_MODEL_MIN_HORIZONTAL_EXTENT = 32,
    CM_BIG_STATIC_MODEL_MIN_VERTICAL_EXTENT = 64
};

void Com_DPrintf(const char *format, ...);

/*
 * Complete world-sector allocation, static-model linking, and area-query
 * cluster shared by the Windows client/listen server and Linux dedicated
 * server.  The authoritative bodies are:
 *
 *   CoDUOMP.exe    0x0042a500..0x0042a748,
 *                  0x0042a750..0x0042a924,
 *                  0x0042a930..0x0042aa9a,
 *                  0x0042aaa0..0x0042ad2c
 *   coduo_lnxded   0x0805d808..0x0805db32,
 *                  0x0805db34..0x0805e020,
 *                  0x0805e02b..0x0805e386
 *
 * Pointer-bearing links have the same i386 layouts and use ordinary native
 * pointers in the maintained 64-bit source.  The three narrow behavior choices
 * below retain the only result-affecting platform differences in this cluster.
 */

void CM_InitWorldSector(void)
{
    CM_ModelBounds(CM_InlineModel(CM_WORLD_MODEL), cm_worldMins, cm_worldMaxs);

    cm_freeWorldSectors = &cm_worldSectorPool[0];
    for (int32_t sectorIndex = 0; sectorIndex < SERVER_WORLD_SECTOR_POOL_COUNT - 1; ++sectorIndex) {
        cm_worldSectorPool[sectorIndex].parent = &cm_worldSectorPool[sectorIndex + 1];
    }
    cm_worldSectorPool[SERVER_WORLD_SECTOR_POOL_COUNT - 1].parent = NULL;

#if defined(WINDOWS_BEHAVIOR)
    /* Windows compares both unspilled x87 subtractions. */
    const long double widthX = (long double)cm_worldMaxs[0] - (long double)cm_worldMins[0];
    const long double widthY = (long double)cm_worldMaxs[1] - (long double)cm_worldMins[1];
#else
    /* Linux stores both widths to binary32 before comparing them. */
    const float widthX = cm_worldMaxs[0] - cm_worldMins[0];
    const float widthY = cm_worldMaxs[1] - cm_worldMins[1];
#endif
    cm_worldSectorRoot.axis = widthX <= widthY ? 1 : 0;
    cm_worldSectorRoot.dist =
        (float)(((long double)cm_worldMins[cm_worldSectorRoot.axis] + (long double)cm_worldMaxs[cm_worldSectorRoot.axis]) * 0.5L);
    cm_worldSectorRoot.children[0] = &cm_nullWorldSector;
    cm_worldSectorRoot.children[1] = &cm_nullWorldSector;
}

worldSector_t *CM_AllocWorldSector(const vec2_t mins, const vec2_t maxs)
{
    worldSector_t *const sector = cm_freeWorldSectors;
    float sizes[2];
    int32_t axis;

    if (sector == NULL) {
        return NULL;
    }

#if defined(WINDOWS_BEHAVIOR)
    /* Windows keeps the X subtraction live while storing both array lanes. */
    const long double liveSizeX = (long double)maxs[0] - (long double)mins[0];
    sizes[0] = (float)liveSizeX;
    sizes[1] = (float)((long double)maxs[1] - (long double)mins[1]);
    axis = liveSizeX <= (long double)sizes[1] ? 1 : 0;
#else
    /* Linux compares the two stored binary32 lanes. */
    sizes[0] = maxs[0] - mins[0];
    sizes[1] = maxs[1] - mins[1];
    axis = sizes[0] <= sizes[1] ? 1 : 0;
#endif

    if (sizes[axis] <= (float)CM_WORLD_SECTOR_MIN_SPLIT_SIZE) {
        return NULL;
    }

    cm_freeWorldSectors = sector->parent;
    sector->axis = axis;
    sector->dist = (float)(((long double)mins[axis] + (long double)maxs[axis]) * 0.5L);
    sector->children[0] = &cm_nullWorldSector;
    sector->children[1] = &cm_nullWorldSector;
    return sector;
}

void CM_RebucketWorldSectorLinks(worldSector_t *sector, const vec2_t sectorMins, const vec2_t sectorMaxs)
{
    const int32_t axis = sector->axis;
    const float distance = sector->dist;
    svEntity_t *previousEntity = NULL;
    svEntity_t *entity = sector->entityLinkHead;

    while (entity != NULL) {
        worldSector_t *child;

        if (entity->linkMins[axis] > distance) {
            child = sector->children[0];
            if (child == &cm_nullWorldSector) {
                child = CM_AllocWorldSector(sectorMins, sectorMaxs);
                if (child == NULL) {
                    return;
                }
                sector->children[0] = child;
                child->parent = sector;
            }
        } else if (entity->linkMaxs[axis] < distance) {
            child = sector->children[1];
            if (child == &cm_nullWorldSector) {
                child = CM_AllocWorldSector(sectorMins, sectorMaxs);
                if (child == NULL) {
                    return;
                }
                sector->children[1] = child;
                child->parent = sector;
            }
        } else {
            previousEntity = entity;
            entity = entity->nextInWorldSector;
            continue;
        }

        svEntity_t *const next = entity->nextInWorldSector;
        entity->worldSector = child;
        entity->nextInWorldSector = child->entityLinkHead;
        child->entityLinkHead = entity;
        child->entityContentsMask |= SV_GEntityForSvEntity(entity)->contents;

        if (previousEntity == NULL) {
            sector->entityLinkHead = next;
        } else {
            previousEntity->nextInWorldSector = next;
        }
        entity = next;
    }

    worldSectorAreaLink_t *previousArea = NULL;
    worldSectorAreaLink_t *area = sector->staticModelLinkHead;

    while (area != NULL) {
        worldSector_t *child;

        if (area->linkMins[axis] > distance) {
            child = sector->children[0];
            if (child == &cm_nullWorldSector) {
                child = CM_AllocWorldSector(sectorMins, sectorMaxs);
                if (child == NULL) {
                    return;
                }
                sector->children[0] = child;
                child->parent = sector;
            }
        } else if (area->linkMaxs[axis] < distance) {
            child = sector->children[1];
            if (child == &cm_nullWorldSector) {
                child = CM_AllocWorldSector(sectorMins, sectorMaxs);
                if (child == NULL) {
                    return;
                }
                sector->children[1] = child;
                child->parent = sector;
            }
        } else {
            previousArea = area;
            area = area->nextInWorldSector;
            continue;
        }

        worldSectorAreaLink_t *const next = area->nextInWorldSector;
        area->nextInWorldSector = child->staticModelLinkHead;
        child->staticModelLinkHead = area;

        const int32_t contents = XModelGetContents(area->model);
        child->staticModelContentsMask |= contents;
        if (area->sightTraceEligible != qfalse) {
            child->sightTraceStaticModelContentsMask |= contents;
        }

        if (previousArea == NULL) {
            sector->staticModelLinkHead = next;
        } else {
            previousArea->nextInWorldSector = next;
        }
        area = next;
    }
}

/* CoDUOMP.exe 0x0042a660; coduo_lnxded 0x0805d9df. */
void SV_UnlinkEntityFromWorldSector(svEntity_t *serverEntity)
{
    worldSector_t *sector = serverEntity->worldSector;

    if (sector == NULL) {
        return;
    }

    serverEntity->worldSector = NULL;
    if (sector->entityLinkHead == serverEntity) {
        sector->entityLinkHead = serverEntity->nextInWorldSector;
    } else {
        svEntity_t *previous = sector->entityLinkHead;
        while (previous->nextInWorldSector != serverEntity) {
            previous = previous->nextInWorldSector;
        }
        previous->nextInWorldSector = serverEntity->nextInWorldSector;
    }

    while (sector->entityLinkHead == NULL && sector->staticModelLinkHead == NULL && sector->children[0] == &cm_nullWorldSector &&
           sector->children[1] == &cm_nullWorldSector) {
        worldSector_t *const parent = sector->parent;

        sector->entityContentsMask = 0;
        if (parent == NULL) {
            break;
        }

        sector->parent = cm_freeWorldSectors;
        cm_freeWorldSectors = sector;

        if (parent->children[0] == sector) {
            parent->children[0] = &cm_nullWorldSector;
        } else {
            parent->children[1] = &cm_nullWorldSector;
        }
        sector = parent;
    }

    for (; sector != NULL; sector = sector->parent) {
        int32_t contentsMask = sector->children[0]->entityContentsMask | sector->children[1]->entityContentsMask;

        for (svEntity_t *scan = sector->entityLinkHead; scan != NULL; scan = scan->nextInWorldSector) {
            contentsMask |= SV_GEntityForSvEntity(scan)->contents;
        }

        sector->entityContentsMask = contentsMask;
    }
}

/* CoDUOMP.exe 0x0042a930; coduo_lnxded 0x0805de1a. */
void SV_LinkEntityToWorldSector(svEntity_t *serverEntity, const vec2_t mins, const vec2_t maxs)
{
    const int32_t contentsMask = SV_GEntityForSvEntity(serverEntity)->contents;

    for (;;) {
        vec2_t sectorMins = {cm_worldMins[0], cm_worldMins[1]};
        vec2_t sectorMaxs = {cm_worldMaxs[0], cm_worldMaxs[1]};
        worldSector_t *sector = &cm_worldSectorRoot;
        qboolean stoppedAtNullChild = qfalse;

        for (;;) {
            const int32_t axis = sector->axis;
            const float distance = sector->dist;

            sector->entityContentsMask |= contentsMask;

            if (mins[axis] > distance) {
                sectorMins[axis] = distance;
                if (sector->children[0] == &cm_nullWorldSector) {
                    stoppedAtNullChild = qtrue;
                    break;
                }
                sector = sector->children[0];
                continue;
            }

            /* Both x87 originals send unordered comparisons to overlap. */
            if (!(distance > maxs[axis])) {
                break;
            }

            sectorMaxs[axis] = distance;
            if (sector->children[1] == &cm_nullWorldSector) {
                stoppedAtNullChild = qtrue;
                break;
            }
            sector = sector->children[1];
        }

        if (stoppedAtNullChild == qfalse && serverEntity->worldSector == sector && ((~contentsMask & serverEntity->contentsMask) == 0)) {
            serverEntity->contentsMask = contentsMask;
            serverEntity->linkMins[0] = mins[0];
            serverEntity->linkMins[1] = mins[1];
            serverEntity->linkMaxs[0] = maxs[0];
            serverEntity->linkMaxs[1] = maxs[1];
            return;
        }

        if (serverEntity->worldSector != NULL) {
            if (serverEntity->worldSector == sector && ((~contentsMask & serverEntity->contentsMask) == 0)) {
                serverEntity->contentsMask = contentsMask;
                serverEntity->linkMins[0] = mins[0];
                serverEntity->linkMins[1] = mins[1];
                serverEntity->linkMaxs[0] = maxs[0];
                serverEntity->linkMaxs[1] = maxs[1];
                CM_RebucketWorldSectorLinks(sector, sectorMins, sectorMaxs);
                return;
            }

            SV_UnlinkEntityFromWorldSector(serverEntity);
            continue;
        }

        serverEntity->worldSector = sector;
        serverEntity->nextInWorldSector = sector->entityLinkHead;
        sector->entityLinkHead = serverEntity;

        serverEntity->contentsMask = contentsMask;
        serverEntity->linkMins[0] = mins[0];
        serverEntity->linkMins[1] = mins[1];
        serverEntity->linkMaxs[0] = maxs[0];
        serverEntity->linkMaxs[1] = maxs[1];
        CM_RebucketWorldSectorLinks(sector, sectorMins, sectorMaxs);
        return;
    }
}

qboolean CM_IsBigStaticModel(const vec3_t mins, const vec3_t maxs)
{
#if defined(WINDOWS_BEHAVIOR)
    /* CoDUOMP.exe 0x004c57b0 and the inlined copy at 0x0042aaa0 test Z,
     * Y, then X.  Unordered extents follow the large-model path. */
    if ((long double)maxs[2] - (long double)mins[2] < (long double)CM_BIG_STATIC_MODEL_MIN_VERTICAL_EXTENT) {
        return qfalse;
    }
    if ((long double)maxs[1] - (long double)mins[1] < (long double)CM_BIG_STATIC_MODEL_MIN_HORIZONTAL_EXTENT) {
        return qfalse;
    }
    if ((long double)maxs[0] - (long double)mins[0] < (long double)CM_BIG_STATIC_MODEL_MIN_HORIZONTAL_EXTENT) {
        return qfalse;
    }
    return qtrue;
#else
    /* coduo_lnxded 0x0805e021 is an unconditional false return. */
    (void)mins;
    (void)maxs;
    return qfalse;
#endif
}

void CM_LinkStaticModel(worldSectorAreaLink_t *areaLink)
{
    areaLink->sightTraceEligible = CM_IsBigStaticModel(areaLink->linkMins, areaLink->linkMaxs);
    const int32_t contentsMask = XModelGetContents(areaLink->model);
    vec2_t sectorMins = {cm_worldMins[0], cm_worldMins[1]};
    vec2_t sectorMaxs = {cm_worldMaxs[0], cm_worldMaxs[1]};
    worldSector_t *sector = &cm_worldSectorRoot;

    for (;;) {
        const int32_t axis = sector->axis;
        const float distance = sector->dist;

        sector->staticModelContentsMask |= contentsMask;
        if (areaLink->sightTraceEligible != qfalse) {
            sector->sightTraceStaticModelContentsMask |= contentsMask;
        }

        if (areaLink->linkMins[axis] > distance) {
            sectorMins[axis] = distance;
            if (sector->children[0] == &cm_nullWorldSector) {
                break;
            }
            sector = sector->children[0];
            continue;
        }

        if (!(distance > areaLink->linkMaxs[axis])) {
            break;
        }

        sectorMaxs[axis] = distance;
        if (sector->children[1] == &cm_nullWorldSector) {
            break;
        }
        sector = sector->children[1];
    }

    areaLink->nextInWorldSector = sector->staticModelLinkHead;
    sector->staticModelLinkHead = areaLink;
    CM_RebucketWorldSectorLinks(sector, sectorMins, sectorMaxs);
}

void CM_AreaEntities_r(worldSector_t *sector, cmAreaEntitiesWork_t *work)
{
    if ((work->contentsMask & sector->entityContentsMask) == 0) {
        return;
    }

    for (svEntity_t *serverEntity = sector->entityLinkHead; serverEntity != NULL; serverEntity = serverEntity->nextInWorldSector) {
        const sharedEntity_t *const entity = SV_GEntityForSvEntity(serverEntity);

        if ((work->contentsMask & entity->contents) == 0) {
            continue;
        }

        /* Both originals reject only ordered separation.  NaN bounds remain
         * eligible, matching the x87 unordered branch paths. */
        if (entity->absMin[0] > work->maxs[0] || entity->absMax[0] < work->mins[0] || entity->absMin[1] > work->maxs[1] ||
            entity->absMax[1] < work->mins[1] || entity->absMin[2] > work->maxs[2] || entity->absMax[2] < work->mins[2]) {
            continue;
        }

        if (work->count == work->maxCount) {
            Com_DPrintf("CM_AreaEntities: MAXCOUNT\n");
            return;
        }

        work->entityList[work->count] = (int32_t)(serverEntity - sv_entities);
        ++work->count;
    }

    const int32_t axis = sector->axis;
    if (sector->dist < work->maxs[axis]) {
        CM_AreaEntities_r(sector->children[0], work);
    }
    if (work->mins[axis] < sector->dist) {
        CM_AreaEntities_r(sector->children[1], work);
    }
}

int32_t CM_AreaEntities(const vec3_t mins, const vec3_t maxs, int32_t *entityList, int32_t maxEntityCount, int32_t contentsMask)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (entityList == NULL || maxEntityCount <= 0) {
        return 0;
    }

    cmAreaEntitiesWork_t work = {
        .mins = mins, .maxs = maxs, .entityList = entityList, .count = 0, .maxCount = maxEntityCount, .contentsMask = contentsMask};

    CM_AreaEntities_r(&cm_worldSectorRoot, &work);
    return work.count;
}
