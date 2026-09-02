#ifndef CODUO_COLLISION_TERRAIN_TRACE_H
#define CODUO_COLLISION_TERRAIN_TRACE_H

#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"
#include "qcommon/q_shared_types.h"

#define CM_TERRAIN_POINT_ENTER_EPSILON 0.125f
#define CM_TERRAIN_BARYCENTRIC_MIN -0.001f
#define CM_TERRAIN_BARYCENTRIC_MAX 1.001f

#ifdef __cplusplus
extern "C" {
#endif

void CM_TracePointThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);
void CM_TraceSphereThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);
qboolean CM_PositionTestSphereWithTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide);

#ifdef __cplusplus
}
#endif

#endif
