#ifndef CODUO_COLLISION_LEAF_QUERIES_H
#define CODUO_COLLISION_LEAF_QUERIES_H

#include "qcommon/collision_map_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_StoreLeafs(cmLeafQueryWork_t *work, int32_t nodeNum);
void CM_StoreLeafBrushes(cmLeafQueryWork_t *work, int32_t nodeNum);
void CM_BoxLeafnums_r(cmLeafQueryWork_t *work, int32_t nodeNum);
int32_t CM_BoxLeafnums(const vec3_t mins, const vec3_t maxs,
                       int32_t *leafList, int32_t leafListSize,
                       int32_t *lastLeaf);
int32_t CM_BoxBrushes(const vec3_t mins, const vec3_t maxs,
                      collisionBrush_t **brushes, int32_t maxCount);

#ifdef __cplusplus
}
#endif

#endif
