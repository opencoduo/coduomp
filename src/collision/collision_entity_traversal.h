#ifndef CODUO_COLLISION_ENTITY_TRAVERSAL_H
#define CODUO_COLLISION_ENTITY_TRAVERSAL_H

#include "qcommon/collision_trace_work_types.h"
#include "collision_world_sector.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_ClipMoveToEntities_r(cmClipMoveWork_t *work, worldSector_t *sector, float startFraction, float endFraction, const vec3_t start,
                             const vec3_t end);
void CM_ClipMoveToEntities(cmClipMoveWork_t *work);
int32_t CM_ClipSightTraceToEntities_r(cmClipSightTraceWork_t *work, worldSector_t *sector, float startFraction, float endFraction,
                                      const vec3_t start, const vec3_t end);
int32_t CM_ClipSightTraceToEntities(cmClipSightTraceWork_t *work);
void CM_PointTraceToEntities_r(cmPointTraceWork_t *work, worldSector_t *sector, float startFraction, float endFraction, const vec3_t start,
                               const vec3_t end);
void CM_PointTraceToEntities(cmPointTraceWork_t *work);
int32_t CM_PointSightTraceToEntities_r(cmPointSightTraceWork_t *work, worldSector_t *sector, float startFraction, float endFraction,
                                       const vec3_t start, const vec3_t end);
int32_t CM_PointSightTraceToEntities(cmPointSightTraceWork_t *work);

#ifdef __cplusplus
}
#endif

#endif
