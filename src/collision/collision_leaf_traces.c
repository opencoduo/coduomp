#include "collision_leaf_traces.h"

#include "collision_brush_traces.h"
#include "collision_patch_dispatch.h"
#include "collision_patch_trace.h"
#include "collision_terrain_dispatch.h"
#include "qcommon/qcommon_runtime_types.h"

#include <stdint.h>

extern int32_t *cm_leafbrushes;
extern int32_t *cm_leafsurfaces;
extern collisionBrush_t *cm_brushes;
extern collisionTerrainPatch_t *cm_terrainPatches;
extern int32_t cm_checkcount;
extern int32_t cm_numBrushes;
extern cvar_t *cm_noCurves;

/*
 * Complete collision-leaf traversal cluster:
 *
 *   CoDUOMP.exe  CM_TestInLeaf              0x004261c0..0x004262e6
 *                CM_TraceThroughLeaf        0x00427220..0x004272f9
 *                CM_SightTraceThroughLeaf   0x00429000..0x0042913a
 *   coduo_lnxded CM_TestInLeaf              0x080582c8..0x0805848e
 *                CM_TraceThroughLeaf        0x08059875..0x080599e5
 *                CM_SightTraceThroughLeaf   0x0805bdad..0x0805bf2f
 *
 * The original bodies use the same checkcount suppression, contents masks,
 * brush/patch order, and early-exit results.  Windows inlines the small
 * terrain position and sight dispatchers where Linux calls them; the shared
 * source keeps those already-shared boundaries explicit without changing the
 * data flow.
 */
void CM_TestInLeaf(traceWork_t *traceWork,
                   const collisionLeaf_t *leaf)
{
    for (int32_t leafBrushIndex = 0;
         leafBrushIndex < (int32_t)leaf->numLeafBrushes;
         ++leafBrushIndex) {
        const int32_t brushIndex =
            cm_leafbrushes[leaf->firstLeafBrush + leafBrushIndex];
        collisionBrush_t *const brush = &cm_brushes[brushIndex];

        if (brush->checkcount == cm_checkcount) {
            continue;
        }
        brush->checkcount = cm_checkcount;

        if ((traceWork->contents & brush->contents) == 0) {
            continue;
        }

        CM_TestBoxInBrush(traceWork, brush);
        if (traceWork->trace.allsolid != qfalse) {
            return;
        }
    }

    if (cm_noCurves->integer != 0) {
        return;
    }

    for (int32_t leafSurfaceIndex = 0;
         leafSurfaceIndex < (int32_t)leaf->numLeafTerrainPatches;
         ++leafSurfaceIndex) {
        const int32_t terrainPatchIndex =
            cm_leafsurfaces[leaf->firstLeafTerrainPatch + leafSurfaceIndex];
        collisionTerrainPatch_t *const terrainPatch =
            &cm_terrainPatches[terrainPatchIndex];

        if (terrainPatch->checkcount == cm_checkcount) {
            continue;
        }
        terrainPatch->checkcount = cm_checkcount;

        if ((traceWork->contents & terrainPatch->contents) == 0) {
            continue;
        }

        qboolean hit;
        if (terrainPatch->curveCollide != NULL) {
            hit = CM_PositionTestInPatchCollide(
                traceWork, terrainPatch->curveCollide);
        } else {
            hit = CM_PositionTestInTerrainCollide(
                traceWork, terrainPatch->terrainCollide);
        }

        if (hit != qfalse) {
            traceWork->trace.allsolid = qtrue;
            traceWork->trace.startsolid = qtrue;
            traceWork->trace.fraction = 0.0f;
            return;
        }
    }
}

void CM_TraceThroughLeaf(traceWork_t *traceWork,
                         const collisionLeaf_t *leaf)
{
    for (int32_t leafBrushIndex = 0;
         leafBrushIndex < (int32_t)leaf->numLeafBrushes;
         ++leafBrushIndex) {
        const int32_t brushIndex =
            cm_leafbrushes[leaf->firstLeafBrush + leafBrushIndex];
        collisionBrush_t *const brush = &cm_brushes[brushIndex];

        if (brush->checkcount == cm_checkcount) {
            continue;
        }
        brush->checkcount = cm_checkcount;

        if ((traceWork->contents & brush->contents) == 0) {
            continue;
        }

        CM_TraceThroughBrush(traceWork, brush);
        if (traceWork->trace.fraction == 0.0f) {
            return;
        }
    }

    if (cm_noCurves->integer != 0) {
        return;
    }

    for (int32_t leafSurfaceIndex = 0;
         leafSurfaceIndex < (int32_t)leaf->numLeafTerrainPatches;
         ++leafSurfaceIndex) {
        const int32_t terrainPatchIndex =
            cm_leafsurfaces[leaf->firstLeafTerrainPatch + leafSurfaceIndex];
        collisionTerrainPatch_t *const terrainPatch =
            &cm_terrainPatches[terrainPatchIndex];

        if (terrainPatch->checkcount == cm_checkcount) {
            continue;
        }
        terrainPatch->checkcount = cm_checkcount;

        if ((traceWork->contents & terrainPatch->contents) == 0) {
            continue;
        }

        CM_TraceThroughPatch(traceWork, terrainPatch);
        if (traceWork->trace.fraction == 0.0f) {
            return;
        }
    }
}

int32_t CM_SightTraceThroughLeaf(const traceWork_t *traceWork,
                                 const collisionLeaf_t *leaf)
{
    for (int32_t leafBrushIndex = 0;
         leafBrushIndex < (int32_t)leaf->numLeafBrushes;
         ++leafBrushIndex) {
        const int32_t brushIndex =
            cm_leafbrushes[leaf->firstLeafBrush + leafBrushIndex];
        collisionBrush_t *const brush = &cm_brushes[brushIndex];

        if (brush->checkcount == cm_checkcount) {
            continue;
        }
        brush->checkcount = cm_checkcount;

        if ((traceWork->contents & brush->contents) == 0) {
            continue;
        }

        const int32_t sightHit =
            CM_SightTraceThroughBrush(traceWork, brush);
        if (sightHit != 0) {
            return sightHit;
        }
    }

    if (cm_noCurves->integer != 0) {
        return 0;
    }

    for (int32_t leafSurfaceIndex = 0;
         leafSurfaceIndex < (int32_t)leaf->numLeafTerrainPatches;
         ++leafSurfaceIndex) {
        const int32_t terrainPatchIndex =
            cm_leafsurfaces[leaf->firstLeafTerrainPatch + leafSurfaceIndex];
        collisionTerrainPatch_t *const terrainPatch =
            &cm_terrainPatches[terrainPatchIndex];

        if (terrainPatch->checkcount == cm_checkcount) {
            continue;
        }
        terrainPatch->checkcount = cm_checkcount;

        if ((traceWork->contents & terrainPatch->contents) == 0) {
            continue;
        }

        if (CM_SightTraceThroughPatch(traceWork, terrainPatch) == qfalse) {
            return cm_numBrushes +
                   (int32_t)(terrainPatch - cm_terrainPatches) + 1;
        }
    }

    return 0;
}
