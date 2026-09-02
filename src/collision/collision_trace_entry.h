#ifndef CODUO_COLLISION_TRACE_ENTRY_H
#define CODUO_COLLISION_TRACE_ENTRY_H

#include "qcommon/collision_trace_work_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_Trace(trace_t *trace, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int32_t modelHandle,
              int32_t contentMask, qboolean capsule, const cmTraceSphereRecord_t *sphere);
int32_t CM_SightTrace(int32_t oldHitNum, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int32_t modelHandle,
                      const vec3_t origin, int32_t contentMask, qboolean capsule, const cmTraceSphereRecord_t *sphere);

#ifdef __cplusplus
}
#endif

#endif
