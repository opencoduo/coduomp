#ifndef CODUO_COLLISION_AREA_H
#define CODUO_COLLISION_AREA_H

#include "qcommon/q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CM_FloodArea_r(int32_t areaNum, int32_t floodNum);
void CM_FloodAreaConnections(void);
void CM_AdjustAreaPortalState(int32_t area1, int32_t area2, qboolean open);
qboolean CM_AreasConnected(int32_t area1, int32_t area2);

#ifdef __cplusplus
}
#endif

#endif
