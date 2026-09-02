#include "server_game_queries.h"

#include "collision/collision_area.h"
#include "collision/collision_queries.h"
#include "qcommon/game_module_abi_types.h"
#include "math/q_math.h"
#include "server_game_data.h"
#include "qcommon/vm_runtime.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

extern vm_t *sv_gameVM;

/*
 * Complete game-module spatial-query bridge shared by the Windows
 * client/listen server and Linux dedicated engine:
 *
 *                                  Windows client       Linux dedicated
 * SV_inPVS                         0x0045c910           0x0808e2e9
 * SV_inSnapshot                    0x0045c9c0           0x0808e3a9
 * SV_inPVSIgnorePortals            0x0045cb40           0x0808e60e
 * SV_AdjustAreaPortalState         0x0045cbd0           0x0808e693
 *
 * Direct machine-code comparison proves the same leaf, cluster, area,
 * visibility, fog-distance, and portal-state decisions. Windows inlines
 * CM_AreasConnected in SV_inPVS, exposing its negative-area rejection;
 * Linux calls the ordinary function at 0x08057abd, whose first two branches
 * perform the same rejection. That compiler decision is not a platform
 * behavior difference, so the common source retains the canonical call.
 */

qboolean SV_inPVS(const vec3_t point1, const vec3_t point2)
{
    int32_t leafNum = CM_PointLeafnum(point1);
    const int32_t cluster1 = CM_LeafCluster(leafNum);
    const int32_t area1 = CM_LeafArea(leafNum);
    const uint8_t *const pvs = CM_ClusterPVS(cluster1);

    leafNum = CM_PointLeafnum(point2);
    const int32_t cluster2 = CM_LeafCluster(leafNum);
    const int32_t area2 = CM_LeafArea(leafNum);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cluster2 < 0) {
        return qfalse;
    }
    if (pvs != NULL && (pvs[cluster2 >> 3] & (1u << (cluster2 & 7))) == 0) {
        return qfalse;
    }
    if (CM_AreasConnected(area1, area2) == qfalse) {
        return qfalse;
    }
    return qtrue;
}

qboolean SV_inSnapshot(const vec3_t origin, int32_t entityNum)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (entityNum < 0 || entityNum >= sv_numGentities || entityNum >= MAX_GENTITIES) {
        return qfalse;
    }

    sharedEntity_t *const gentity = SV_GentityNum(entityNum);

    if (gentity->linked == qfalse || (gentity->svFlags & SVF_NOCLIENT) != 0) {
        return qfalse;
    }
    if ((gentity->svFlags & SVF_VISIBILITY_BYPASS_MASK) != 0 || gentity->soundTime != 0) {
        return qtrue;
    }

    svEntity_t *const serverEntity = SV_SvEntityForGentity(gentity);
    const int32_t leafNum = CM_PointLeafnum(origin);
    const int32_t areaNum = CM_LeafArea(leafNum);

    if (CM_AreasConnected(areaNum, serverEntity->areaNum) == qfalse && CM_AreasConnected(areaNum, serverEntity->areaNum2) == qfalse) {
        return qfalse;
    }
    if (serverEntity->numClusters == 0) {
        return qfalse;
    }

    const uint8_t *const pvs = CM_ClusterPVS(CM_LeafCluster(leafNum));
    int32_t testCluster = 0;
    int32_t clusterIndex = 0;
    while (clusterIndex < serverEntity->numClusters) {
        testCluster = serverEntity->clusterNums[clusterIndex];
        if ((pvs[testCluster >> 3] & (1u << (testCluster & 7))) != 0) {
            break;
        }
        ++clusterIndex;
    }

    if (clusterIndex == serverEntity->numClusters) {
        if (serverEntity->lastCluster == 0) {
            return qfalse;
        }

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        while (testCluster <= serverEntity->lastCluster && (pvs[testCluster >> 3] & (1u << (testCluster & 7))) == 0) {
            ++testCluster;
        }
        if (testCluster == serverEntity->lastCluster) {
            return qfalse;
        }
    }

    const uint32_t fogDistanceBits = (uint32_t)VM_Call(sv_gameVM, GAME_GET_FOG_OPAQUE_DIST_SQ_BITS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    float fogDistanceSquared;
    memcpy(&fogDistanceSquared, &fogDistanceBits, sizeof(fogDistanceSquared));

    if (fogDistanceSquared == FLT_MAX) {
        return qtrue;
    }
    return BoxDistSqrdExceeds(gentity->absMin, gentity->absMax, origin, fogDistanceSquared) == qfalse ? qtrue : qfalse;
}

qboolean SV_inPVSIgnorePortals(const vec3_t point1, const vec3_t point2)
{
    const int32_t cluster1 = CM_LeafCluster(CM_PointLeafnum(point1));
    const uint8_t *const pvs = CM_ClusterPVS(cluster1);
    const int32_t cluster2 = CM_LeafCluster(CM_PointLeafnum(point2));

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (pvs != NULL && (pvs[cluster2 >> 3] & (1u << (cluster2 & 7))) == 0) {
        return qfalse;
    }
    return qtrue;
}

void SV_AdjustAreaPortalState(sharedEntity_t *gentity, qboolean open)
{
    svEntity_t *const serverEntity = SV_SvEntityForGentity(gentity);

    if (serverEntity->areaNum2 != -1) {
        CM_AdjustAreaPortalState(serverEntity->areaNum, serverEntity->areaNum2, open);
    }
}
