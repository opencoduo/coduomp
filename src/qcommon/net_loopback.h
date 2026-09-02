#ifndef QCOMMON_NET_LOOPBACK_H
#define QCOMMON_NET_LOOPBACK_H

#include "msg.h"
#include "net_types.h"

#include <stdint.h>

extern loopback_t net_loopbacks[NET_LOOPBACK_QUEUE_COUNT];

qboolean NET_GetLoopPacket(netsrc_t source, netadr_t *address, msg_t *message);
void NET_SendLoopPacket(netsrc_t source, int32_t length, const void *data);

#endif
