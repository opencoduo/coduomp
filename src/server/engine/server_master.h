#ifndef SHARED_SERVER_MASTER_H
#define SHARED_SERVER_MASTER_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

netadr_t *SV_MasterAddress(void);
void SV_MasterHeartbeat(const char *heartbeat);
void SV_MasterGameCompleteStatus(void);
void SV_MasterShutdown(void);
int32_t SV_GetClientScore(client_t *client);

#ifdef __cplusplus
}
#endif

#endif
