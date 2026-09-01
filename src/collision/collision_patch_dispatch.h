#ifndef CODUO_COLLISION_PATCH_DISPATCH_H
#define CODUO_COLLISION_PATCH_DISPATCH_H

#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"
#include "qcommon/q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void CM_TraceThroughPatch(
    traceWork_t *traceWork,
    const collisionTerrainPatch_t *terrainPatch);
qboolean CM_SightTraceThroughPatch(
    const traceWork_t *traceWork,
    const collisionTerrainPatch_t *terrainPatch);

#ifdef __cplusplus
}
#endif

#endif
