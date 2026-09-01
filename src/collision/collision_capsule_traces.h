#ifndef CODUO_COLLISION_CAPSULE_TRACES_H
#define CODUO_COLLISION_CAPSULE_TRACES_H

#include "qcommon/collision_trace_work_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_TestCapsuleInCapsule(traceWork_t *traceWork);
void CM_TestBoundingBoxInCapsule(traceWork_t *traceWork);
qboolean CM_TraceSphereThroughSphere(
    traceWork_t *traceWork,
    const vec3_t start, const vec3_t end,
    const vec3_t sphereOrigin, float sphereRadius);
qboolean CM_TraceCylinderThroughCylinder(
    traceWork_t *traceWork,
    const vec3_t cylinderOrigin,
    float cylinderHalfHeight, float cylinderRadius);
void CM_TraceCapsuleThroughCapsule(traceWork_t *traceWork);
void CM_TraceBoundingBoxThroughCapsule(traceWork_t *traceWork);
qboolean CM_SightTraceSphereThroughSphere(
    const traceWork_t *traceWork,
    const vec3_t start, const vec3_t end,
    const vec3_t sphereOrigin, float sphereRadius);
qboolean CM_SightTraceCylinderThroughCylinder(
    const traceWork_t *traceWork,
    const vec3_t cylinderOrigin,
    float cylinderHalfHeight, float cylinderRadius);
int32_t CM_SightTraceCapsuleThroughCapsule(
    const traceWork_t *traceWork);
int32_t CM_SightTraceBoundingBoxThroughCapsule(
    traceWork_t *traceWork);

#ifdef __cplusplus
}
#endif

#endif
