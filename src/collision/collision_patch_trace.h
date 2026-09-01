#ifndef CODUO_COLLISION_PATCH_TRACE_H
#define CODUO_COLLISION_PATCH_TRACE_H

#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean CM_PositionTestInPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide);
void CM_TracePointThroughPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide);
qboolean CM_SightTracePointThroughPatchCollide(
    const traceWork_t *traceWork,
    const patchCollide_t *patchCollide);
void CM_TraceThroughPatchCollide(
    traceWork_t *traceWork,
    const patchCollide_t *patchCollide);
qboolean CM_SightTraceThroughPatchCollide(
    const traceWork_t *traceWork,
    const patchCollide_t *patchCollide);
qboolean CM_CheckFacetPlane(
    const vec4_t plane,
    const vec3_t start, const vec3_t end,
    float *enterFraction, float *leaveFraction,
    qboolean *hit);

#ifdef __cplusplus
}
#endif

#endif
