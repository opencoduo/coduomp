#ifndef CODUO_COLLISION_LEAF_TRACES_H
#define CODUO_COLLISION_LEAF_TRACES_H

#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void CM_TestInLeaf(traceWork_t *traceWork,
                   const collisionLeaf_t *leaf);
void CM_TraceThroughLeaf(traceWork_t *traceWork,
                         const collisionLeaf_t *leaf);
int32_t CM_SightTraceThroughLeaf(const traceWork_t *traceWork,
                                 const collisionLeaf_t *leaf);

#ifdef __cplusplus
}
#endif

#endif
