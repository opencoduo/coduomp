#ifndef SHARED_SERVER_DOWNLOAD_H
#define SHARED_SERVER_DOWNLOAD_H

#include "qcommon/msg.h"
#include "qcommon/server_runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void SV_CloseDownload(client_t *client);
void SV_StopDownload_f(client_t *client);
void SV_DoneDownload_f(client_t *client);
void SV_RetransmitDownload_f(client_t *client);
void SV_NextDownload_f(client_t *client);
void SV_BeginDownload_f(client_t *client);
void SV_WWWDownload_f(client_t *client);
void SV_BadDownload(client_t *client, msg_t *message);
qboolean SV_CheckFallbackURL(void);
void SV_WriteDownloadToClient(client_t *client, msg_t *message);

#ifdef __cplusplus
}
#endif

#endif
