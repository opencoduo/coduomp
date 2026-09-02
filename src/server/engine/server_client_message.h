#ifndef SHARED_SERVER_CLIENT_MESSAGE_H
#define SHARED_SERVER_CLIENT_MESSAGE_H

#include "qcommon/q_shared_types.h"
#include "qcommon/qcommon_runtime_types.h"
#include "qcommon/server_runtime_types.h"
#include "qcommon/msg.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_DropClient(client_t *client, const char *dropReason);
void PB_DropClient(int32_t clientNum, const char *dropReason);
void SV_Disconnect_f(client_t *client);
void SV_VerifyPaks_f(client_t *client);
void SV_ResetPureClient_f(client_t *client);
void SV_UserinfoChanged(client_t *client);
void SV_UpdateUserinfo_f(client_t *client);
void SV_ExecuteClientCommand(client_t *client, const char *text, qboolean clientOK);
qboolean SV_ClientCommand(client_t *client, msg_t *message);
void SV_ClientThink(client_t *client, const usercmd_t *command);
void SV_UserMove(client_t *client, msg_t *message, qboolean delta);
void SV_ExecuteClientMessage(client_t *client, msg_t *message);
int32_t SV_AddTestClient(void);

#ifdef __cplusplus
}
#endif

#endif
