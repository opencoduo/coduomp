#ifndef CODUO_COLLISION_TERRAIN_DISPATCH_H
#define CODUO_COLLISION_TERRAIN_DISPATCH_H

#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"
#include "qcommon/q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void CM_TraceSquareThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);
void CM_TraceThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);
qboolean CM_SightTraceThroughTerrainCollide(
    const traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);
qboolean CM_PositionTestInTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);

#ifdef __cplusplus
}
#endif

#endif
