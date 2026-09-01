#ifndef SHARED_SERVER_SNAPSHOT_SEND_H
#define SHARED_SERVER_SNAPSHOT_SEND_H

#include "qcommon/msg.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>

enum {
    SERVER_AVERAGE_BPS_WINDOW_COUNT = 20
};

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t
    sv_compressedBpsWindow[SERVER_AVERAGE_BPS_WINDOW_COUNT];
extern int32_t sv_averageBpsFrameCount;
extern int32_t sv_totalBytesSentThisFrame;
extern int32_t sv_compressedBpsMax;
extern int32_t
    sv_uncompressedBpsWindow[SERVER_AVERAGE_BPS_WINDOW_COUNT];
extern int32_t sv_totalUncompressedBytesThisFrame;
extern int32_t sv_uncompressedBpsMax;
extern float sv_averageCompressionRatioSum;
extern int32_t sv_averageCompressionRatioCount;

void SV_EmitPacketEntities(int32_t oldNumEntities,
                           int32_t oldFirstEntity,
                           int32_t newNumEntities,
                           int32_t newFirstEntity,
                           msg_t *message);
void SV_EmitPacketClients(int32_t oldNumClients,
                          int32_t oldFirstClient,
                          int32_t newNumClients,
                          int32_t newFirstClient,
                          msg_t *message);
void SV_WriteSnapshotToClient(client_t *client, msg_t *message);
int32_t SV_RateMsec(client_t *client, int32_t messageSize);
void SV_SendMessageToClient(msg_t *message, client_t *client);
void SV_SendClientSnapshot(client_t *client);
void SV_SendClientMessages(void);

#ifdef __cplusplus
}
#endif

#endif
