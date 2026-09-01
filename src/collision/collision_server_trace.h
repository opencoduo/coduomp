#ifndef CODUO_COLLISION_SERVER_TRACE_H
#define CODUO_COLLISION_SERVER_TRACE_H

#include "qcommon/collision_trace_work_types.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_ClipMoveToEntity(cmClipMoveWork_t *work,
                         svEntity_t *serverEntity);
void SV_PointTraceToEntity(cmPointTraceWork_t *work,
                           svEntity_t *serverEntity);
int32_t SV_ClipSightTraceToEntity(cmClipSightTraceWork_t *work,
                                  svEntity_t *serverEntity);
int32_t SV_PointSightTraceToEntity(cmPointSightTraceWork_t *work,
                                   svEntity_t *serverEntity);

void SV_Trace(trace_t *trace, const vec3_t start,
              const vec3_t mins, const vec3_t maxs, const vec3_t end,
              int32_t passEntityNum, int32_t contentMask,
              qboolean capsule, qboolean useDObj,
              const uint8_t *dobjTracePartState, qboolean locational);
void SV_SightTrace(int32_t *traceResult, const vec3_t start,
                   const vec3_t mins, const vec3_t maxs, const vec3_t end,
                   int32_t passEntityNum, int32_t passOwnerNum,
                   int32_t contentMask, qboolean capsule);
int32_t SV_SightTraceToEntity(const vec3_t start, const vec3_t mins,
                              const vec3_t maxs, const vec3_t end,
                              int32_t entityNum, int32_t contentMask,
                              qboolean capsule);
int32_t SV_PointContents(const vec3_t point, int32_t passEntityNum,
                         int32_t contentMask);

#ifdef __cplusplus
}
#endif

#endif
