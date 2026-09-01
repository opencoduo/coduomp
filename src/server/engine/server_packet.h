#ifndef SHARED_SERVER_PACKET_H
#define SHARED_SERVER_PACKET_H

#include "qcommon/msg.h"
#include "qcommon/net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void SVC_Status(netadr_t from);
void SVC_GameCompleteStatus(netadr_t from);
void SVC_Info(netadr_t from);
void SV_FlushRedirect(char *text);
void SVC_RemoteCommand(netadr_t from);
void SV_ConnectionlessPacket(netadr_t from, msg_t *message);
void SV_PacketEvent(netadr_t from, msg_t *message);

#ifdef __cplusplus
}
#endif

#endif
