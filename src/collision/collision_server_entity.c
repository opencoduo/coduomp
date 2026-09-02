#include "collision_server_entity.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87emu.h"
#include "collision_box_trace.h"
#include "collision_leaf_queries.h"
#include "collision_queries.h"
#include "collision_world_sector.h"
#include "animation/dobj.h"
#include "math/q_math.h"
#include "server/engine/server_game_data.h"

enum {
    SV_WORLD_LINK_LEAF_COUNT = 128,
    SV_WORLD_SOLID_COMPONENT_MIN = 1,
    SV_WORLD_SOLID_COMPONENT_MAX = 255,
    SV_WORLD_SOLID_ZDOWN_SHIFT = 8,
    SV_WORLD_SOLID_ZUP_SHIFT = 16
};

static const float sv_snapVectorEpsilon = 0.0000010000001f;

void Com_DPrintf(const char *format, ...);

/*
 * Complete server-entity world-registration cluster shared by the Windows
 * client/listen server and Linux dedicated server.  The authoritative bodies
 * are CoDUOMP.exe 0x00466f50..0x00467549 and coduo_lnxded
 * 0x08099ca0..0x0809a58d.  Their state changes, bounds, leaf/cluster walks,
 * DObj selection, and world-sector calls agree.  The snap spill and integer
 * conversion result-width differences are retained at their exact sites.
 */

int32_t SV_ClipHandleForEntity(const sharedEntity_t *entity)
{
    if (entity->bmodel != qfalse) {
        return CM_InlineModel(entity->entityState.index);
    }

    return CM_TempBoxModel(entity->mins, entity->maxs, entity->contents, (entity->svFlags & SVF_CAPSULE) != 0 ? qtrue : qfalse);
}

/*
 * CoDUOMP.exe 0x0045c860..0x0045c904 and coduo_lnxded
 * 0x0808e247..0x0808e2e8 perform the same model lookup, six bound stores,
 * bmodel/content assignments, and final relink.  Windows inlines the model
 * accessors; Linux calls them, which does not change their contract.
 */
void SV_SetBrushModel(sharedEntity_t *entity)
{
    const int32_t model = CM_InlineModel(entity->entityState.index);

    CM_ModelBounds(model, entity->mins, entity->maxs);
    entity->bmodel = qtrue;
    entity->contents = -1;
    SV_LinkEntity(entity);
}

/*
 * CoDUOMP.exe 0x0045cc30..0x0045cc82 and coduo_lnxded
 * 0x0808e6dc..0x0808e758 both trace the requested box from vec3_origin to
 * itself against the entity model with all contents enabled, then return the
 * transformed trace's byte-sized startsolid result.
 */
qboolean SV_EntityContact(const vec3_t mins, const vec3_t maxs, const sharedEntity_t *entity, qboolean capsule)
{
    trace_t trace;
    const int32_t clipHandle = SV_ClipHandleForEntity(entity);

    CM_TransformedBoxTraceExternal(&trace, vec3_origin, vec3_origin, mins, maxs, clipHandle, -1, entity->currentOrigin,
                                   entity->currentAngles, capsule);
    return trace.startsolid != 0 ? qtrue : qfalse;
}

void SV_UnlinkEntity(sharedEntity_t *entity)
{
    svEntity_t *const serverEntity = SV_SvEntityForGentity(entity);

    entity->linked = qfalse;
    SV_UnlinkEntityFromWorldSector(serverEntity);
}

#if defined(WINDOWS_BEHAVIOR)
void SV_SnapVector(vec3_t vector)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
        const int32_t rounded = FastRound(vector[axis]);

#if EMULATE_X87
        const x87f difference = x87f_sub(x87f_load_i32(rounded), x87f_load_f32(vector[axis]));
        const x87f squared = x87f_mul(difference, difference);
        if (x87f_lt(squared, x87f_load_f32(sv_snapVectorEpsilon))) {
            vector[axis] = (float)rounded;
        }
#else
        /* CoDUOMP.exe 0x00467021 keeps the difference and its square live
         * under the process PC=53 policy until the binary32 comparison. */
        const long double difference = (long double)rounded - (long double)vector[axis];
        if (difference * difference < (long double)sv_snapVectorEpsilon) {
            vector[axis] = (float)rounded;
        }
#endif
    }
}
#else
void SV_SnapVector(vec3_t vector)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
        const int32_t rounded = FastRound(vector[axis]);

#if EMULATE_X87
        const float difference = x87f_store_f32(x87f_sub(x87f_load_i32(rounded), x87f_load_f32(vector[axis])));
        const x87f squared = x87f_mul(x87f_load_f32(difference), x87f_load_f32(difference));
        if (x87f_lt(squared, x87f_load_f32(sv_snapVectorEpsilon))) {
            vector[axis] = (float)rounded;
        }
#else
        /* coduo_lnxded 0x08099dba stores the difference to binary32 before
         * reloading and squaring it under the Linux PC=64 policy. */
        const float difference = (float)((long double)rounded - (long double)vector[axis]);
        if ((long double)difference * (long double)difference < (long double)sv_snapVectorEpsilon) {
            vector[axis] = (float)rounded;
        }
#endif
    }
}
#endif

void SV_LinkEntity(sharedEntity_t *entity)
{
    int32_t leafs[SV_WORLD_LINK_LEAF_COUNT];
    int32_t lastLeaf;
    int32_t leafIndex;
    svEntity_t *const serverEntity = SV_SvEntityForGentity(entity);
    const int32_t entityNum = entity->entityState.number;

    if (entity->bmodel != qfalse) {
        entity->entityState.solid = SOLID_BMODEL;
    } else if ((entity->contents & (CONTENTS_SOLID | CONTENTS_BODY)) == 0) {
        entity->entityState.solid = 0;
    } else {
        /* Windows consumes the low dword of `_ftol2`'s signed-qword result;
         * Linux stores a signed dword under a temporary truncate control word.
         * The shared adapter preserves the platform-selected invalid result.
         * These three operation graphs contain only exact float/integer
         * widening and addition/subtraction before the conversion. */
        int32_t x = coduo_fp_to_i32_extended((long double)entity->maxs[0]);
        int32_t zDown = coduo_fp_to_i32_extended(1.0L - (long double)entity->mins[2]);
        int32_t zUp = coduo_fp_to_i32_extended((long double)entity->maxs[2] + 32.0L);

        if (x < SV_WORLD_SOLID_COMPONENT_MIN) {
            x = SV_WORLD_SOLID_COMPONENT_MIN;
        } else if (x > SV_WORLD_SOLID_COMPONENT_MAX) {
            x = SV_WORLD_SOLID_COMPONENT_MAX;
        }
        if (zDown < SV_WORLD_SOLID_COMPONENT_MIN) {
            zDown = SV_WORLD_SOLID_COMPONENT_MIN;
        } else if (zDown > SV_WORLD_SOLID_COMPONENT_MAX) {
            zDown = SV_WORLD_SOLID_COMPONENT_MAX;
        }
        if (zUp < SV_WORLD_SOLID_COMPONENT_MIN) {
            zUp = SV_WORLD_SOLID_COMPONENT_MIN;
        } else if (zUp > SV_WORLD_SOLID_COMPONENT_MAX) {
            zUp = SV_WORLD_SOLID_COMPONENT_MAX;
        }

        entity->entityState.solid = x | (zDown << SV_WORLD_SOLID_ZDOWN_SHIFT) | (zUp << SV_WORLD_SOLID_ZUP_SHIFT);
    }

    SV_SnapVector(entity->currentAngles);

    if (entity->bmodel == qfalse ||
        (entity->currentAngles[0] == 0.0f && entity->currentAngles[1] == 0.0f && entity->currentAngles[2] == 0.0f)) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            entity->absMin[axis] = entity->currentOrigin[axis] + entity->mins[axis];
            entity->absMax[axis] = entity->currentOrigin[axis] + entity->maxs[axis];
        }
    } else {
        const float radius = RadiusFromBounds(entity->mins, entity->maxs);
        for (int32_t axis = 0; axis < 3; ++axis) {
            entity->absMin[axis] = entity->currentOrigin[axis] - radius;
            entity->absMax[axis] = entity->currentOrigin[axis] + radius;
        }
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        entity->absMin[axis] -= 1.0f;
        entity->absMax[axis] += 1.0f;
    }

    serverEntity->numClusters = 0;
    serverEntity->lastCluster = 0;
    serverEntity->areaNum = -1;
    serverEntity->areaNum2 = -1;

    const int32_t leafCount = CM_BoxLeafnums(entity->absMin, entity->absMax, leafs, SV_WORLD_LINK_LEAF_COUNT, &lastLeaf);
    if (leafCount == 0) {
        SV_UnlinkEntityFromWorldSector(serverEntity);
        return;
    }

    for (leafIndex = 0; leafIndex < leafCount; ++leafIndex) {
        const int32_t area = CM_LeafArea(leafs[leafIndex]);

        if (area == -1) {
            continue;
        }
        if (serverEntity->areaNum == -1 || serverEntity->areaNum == area) {
            serverEntity->areaNum = area;
            continue;
        }
        if (serverEntity->areaNum2 != -1 && serverEntity->areaNum2 != area && sv.state == SS_LOADING) {
            Com_DPrintf("Object %i touching 3 areas at %f %f %f\n", entityNum, (double)entity->absMin[0], (double)entity->absMin[1],
                        (double)entity->absMin[2]);
        }
        serverEntity->areaNum2 = area;
    }

    serverEntity->numClusters = 0;
    for (leafIndex = 0; leafIndex < leafCount; ++leafIndex) {
        const int32_t cluster = CM_LeafCluster(leafs[leafIndex]);

        if (cluster == -1) {
            continue;
        }
        serverEntity->clusterNums[serverEntity->numClusters] = cluster;
        ++serverEntity->numClusters;
        if (serverEntity->numClusters == MAX_ENT_CLUSTERS) {
            break;
        }
    }

    if (leafIndex != leafCount) {
        serverEntity->lastCluster = CM_LeafCluster(lastLeaf);
    }

    entity->linked = qtrue;
    if (entity->contents == 0) {
        SV_UnlinkEntityFromWorldSector(serverEntity);
        return;
    }

    DObj *const dobj = Com_GetServerDObj(entityNum);
    if (dobj != NULL && (entity->svFlags & SVF_DOBJ_BOUNDS_MASK) != 0U) {
        vec2_t linkMins;
        vec2_t linkMaxs;

        if ((entity->svFlags & SVF_DOBJ_USE_DEFAULT_BOUNDS) != 0U) {
            for (int32_t axis = 0; axis < 2; ++axis) {
                linkMins[axis] = entity->currentOrigin[axis] + sv_defaultEntityClipMins[axis];
                linkMaxs[axis] = entity->currentOrigin[axis] + sv_defaultEntityClipMaxs[axis];
            }
        } else {
            vec3_t modelMins;
            vec3_t modelMaxs;

            DObjGetBounds(dobj, modelMins, modelMaxs);
            for (int32_t axis = 0; axis < 2; ++axis) {
                linkMins[axis] = entity->currentOrigin[axis] + modelMins[axis];
                linkMaxs[axis] = entity->currentOrigin[axis] + modelMaxs[axis];
            }
        }

        SV_LinkEntityToWorldSector(serverEntity, linkMins, linkMaxs);
        return;
    }

    SV_LinkEntityToWorldSector(serverEntity, entity->absMin, entity->absMax);
}
