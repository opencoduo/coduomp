#ifndef CODUOMP_NETWORKING_NET_CHANNEL_H
#define CODUOMP_NETWORKING_NET_CHANNEL_H

#include <stddef.h>
#include <stdint.h>

#include "net_address.h"
#include "qcommon/netchan.h"
#include "server/engine/server_netchan.h"

#ifdef __cplusplus
extern "C" {
#endif

void Net_DisplayProfile(void);
void CL_Netchan_PrintProfileStats(qboolean printHeader);

#ifdef __cplusplus
}
#endif

#endif
