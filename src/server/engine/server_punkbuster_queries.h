#ifndef SHARED_SERVER_PUNKBUSTER_QUERIES_H
#define SHARED_SERVER_PUNKBUSTER_QUERIES_H

#include "qcommon/q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t Pb_Q_maxclients(void);
qboolean Pb_Q_client(int32_t clientNum, char *info);
qboolean Pb_Q_stats(int32_t clientNum, char *info);
void SV_SendPbPacket(int32_t length, const void *data, int32_t clientNum);

#ifdef __cplusplus
}
#endif

#endif
