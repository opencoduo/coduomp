#ifndef SHARED_SERVER_CLIENT_RELEASE_H
#define SHARED_SERVER_CLIENT_RELEASE_H

#include "qcommon/server_runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void SV_FreeClientScriptId(client_t *client);
void SV_FreeClient(client_t *client);
void SV_FreeClients(void);
void SV_FreeClientScriptPers(void);

#ifdef __cplusplus
}
#endif

#endif
