#ifndef CODUO_COLLISION_BOX_TRACE_H
#define CODUO_COLLISION_BOX_TRACE_H

#include "qcommon/q_collision_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_vector_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_BoxTrace(trace_t *trace, const vec3_t start, const vec3_t end,
                 const vec3_t mins, const vec3_t maxs,
                 int32_t modelHandle, int32_t contentMask,
                 qboolean capsule);
void CM_TransformedBoxTrace(trace_t *trace,
                            const vec3_t start, const vec3_t end,
                            const vec3_t mins, const vec3_t maxs,
                            int32_t modelHandle, int32_t contentMask,
                            const vec3_t origin, const vec3_t angles,
                            qboolean capsule);
void CM_TransformedBoxTraceExternal(trace_t *trace,
                                    const vec3_t start, const vec3_t end,
                                    const vec3_t mins, const vec3_t maxs,
                                    int32_t modelHandle, int32_t contentMask,
                                    const vec3_t origin, const vec3_t angles,
                                    qboolean capsule);
int32_t CM_BoxSightTrace(int32_t previousHit,
                         const vec3_t start, const vec3_t end,
                         const vec3_t mins, const vec3_t maxs,
                         int32_t modelHandle, int32_t contentMask,
                         qboolean capsule);
int32_t CM_TransformedBoxSightTrace(int32_t previousHit,
                                    const vec3_t start, const vec3_t end,
                                    const vec3_t mins, const vec3_t maxs,
                                    int32_t modelHandle, int32_t contentMask,
                                    const vec3_t origin, const vec3_t angles,
                                    qboolean capsule);

#ifdef __cplusplus
}
#endif

#endif
