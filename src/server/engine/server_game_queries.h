#ifndef SHARED_SERVER_GAME_QUERIES_H
#define SHARED_SERVER_GAME_QUERIES_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

qboolean SV_inPVS(const vec3_t point1, const vec3_t point2);
qboolean SV_inSnapshot(const vec3_t origin, int32_t entityNum);
qboolean SV_inPVSIgnorePortals(const vec3_t point1, const vec3_t point2);
void SV_AdjustAreaPortalState(sharedEntity_t *gentity, qboolean open);

#ifdef __cplusplus
}
#endif

#endif
