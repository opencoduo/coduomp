#ifndef SHARED_SERVER_OPERATOR_MAPS_H
#define SHARED_SERVER_OPERATOR_MAPS_H

#include "qcommon/q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *SV_GetMapBaseName(const char *mapName);
qboolean SV_MapExists(const char *mapName);
void SV_Map_f(void);
void SV_MapRestart_f(void);
const char *SV_GetMapRotationToken(void);
void SV_MapRotate_f(void);

#ifdef __cplusplus
}
#endif

#endif
