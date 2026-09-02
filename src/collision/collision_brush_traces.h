#ifndef CODUO_COLLISION_BRUSH_TRACES_H
#define CODUO_COLLISION_BRUSH_TRACES_H

#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_TestBoxInBrush(traceWork_t *traceWork,
                       const collisionBrush_t *brush);
void CM_TraceThroughBrush(traceWork_t *traceWork,
                          const collisionBrush_t *brush);
int32_t CM_SightTraceThroughBrush(
    const traceWork_t *traceWork,
    const collisionBrush_t *brush);

#ifdef __cplusplus
}
#endif

#endif
