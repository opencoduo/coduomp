#ifndef CODUO_COLLISION_STATIC_MODEL_TRACE_H
#define CODUO_COLLISION_STATIC_MODEL_TRACE_H

#include "qcommon/collision_trace_work_types.h"
#include "collision_world_sector.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_TraceStaticModel(worldSectorAreaLink_t *areaLink,
                         trace_t *trace, const vec3_t start,
                         const vec3_t end, int32_t contentsMask);
void CM_PointTraceStaticModels_r(cmPointTraceStaticModelsWork_t *work,
                                 worldSector_t *sector,
                                 float startFraction, float endFraction,
                                 const vec3_t start, const vec3_t end);
void CM_PointTraceStaticModels(trace_t *trace, const vec3_t start,
                               const vec3_t end, int32_t contentsMask);
qboolean CM_SightTraceStaticModels_r(
    cmSightTraceStaticModelsWork_t *work, worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end);
qboolean CM_SightTraceStaticModels(const vec3_t start, const vec3_t end,
                                   int32_t contentsMask);

#ifdef __cplusplus
}
#endif

#endif
