#ifndef SHARED_SERVER_AUTHORIZE_H
#define SHARED_SERVER_AUTHORIZE_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_GetChallenge(netadr_t from);
void SV_AuthorizeRequest(netadr_t from, int32_t challenge);
void SV_AuthorizeIpPacket(netadr_t from);
qboolean SV_AuthorizeGuidRecentlySeen(int32_t guid);
qboolean SV_AuthorizeGuidOnBanList(int32_t guid);
int32_t SV_AuthorizeGuidCacheSelectSlot(void);
void SV_AuthorizeGuidCacheStore(int32_t guid);
void SV_BanClient(client_t *client);
void SV_UnbanClient(const char *name);

#ifdef __cplusplus
}
#endif

#endif
