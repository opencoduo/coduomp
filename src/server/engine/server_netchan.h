#ifndef SHARED_SERVER_NETCHAN_H
#define SHARED_SERVER_NETCHAN_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_Netchan_Encode(client_t *client, uint8_t *data, int32_t length);
void SV_Netchan_Decode(client_t *client, uint8_t *data, int32_t length);
void SV_Netchan_TransmitNextFragment(netchan_t *channel);
void SV_Netchan_Transmit(client_t *client, uint8_t *data, int32_t length);
void SV_Netchan_AddOOBProfilePacket(int32_t length);
void SV_Netchan_SendOOBPacket(int32_t length, const void *data, netadr_t address);
void SV_Netchan_UpdateProfileStats(void);
void SV_Netchan_PrintProfileStats(qboolean printHeader);

#ifdef __cplusplus
}
#endif

#endif
