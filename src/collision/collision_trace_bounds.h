#ifndef CODUO_COLLISION_TRACE_BOUNDS_H
#define CODUO_COLLISION_TRACE_BOUNDS_H

#include "qcommon/collision_trace_work_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean CM_TraceWorkIntersectsBounds(const traceWork_t *traceWork, const vec3_t mins, const vec3_t maxs);
qboolean CM_TraceLineSkipsBox(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, float fraction);

#ifdef __cplusplus
}
#endif

#endif
