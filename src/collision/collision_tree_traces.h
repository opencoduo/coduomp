#ifndef CODUO_COLLISION_TREE_TRACES_H
#define CODUO_COLLISION_TREE_TRACES_H

#include "qcommon/collision_trace_work_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_PositionTest(traceWork_t *traceWork);
void CM_TraceThroughTree(traceWork_t *traceWork, int32_t nodeNum, float startFraction, float endFraction, const vec3_t start,
                         const vec3_t end);
int32_t CM_SightTraceThroughTree(const traceWork_t *traceWork, int32_t nodeNum, float startFraction, float endFraction, const vec3_t start,
                                 const vec3_t end);

#ifdef __cplusplus
}
#endif

#endif
