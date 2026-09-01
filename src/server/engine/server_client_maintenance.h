#ifndef SHARED_SERVER_CLIENT_MAINTENANCE_H
#define SHARED_SERVER_CLIENT_MAINTENANCE_H

#include "qcommon/server_runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void SV_CalcPings(void);
void SV_CheckTimeouts(void);
qboolean SV_CheckPaused(void);

#ifdef __cplusplus
}
#endif

#endif
