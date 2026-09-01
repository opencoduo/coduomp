#ifndef SHARED_SERVER_COMMANDS_H
#define SHARED_SERVER_COMMANDS_H

#include "qcommon/msg.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char *SV_ExpandNewlines(const char *input);
qboolean SV_IsFirstTokenEqual(const char *left, const char *right);
int32_t SV_CanReplaceServerCommand(client_t *client,
                                   const char *command);
void SV_CullIgnorableServerCommands(client_t *client);
void SV_AddServerCommand(client_t *client, qboolean reliable,
                         const char *command);
void SV_SendServerCommand(client_t *client, qboolean reliable,
                          const char *format, ...);
void SV_UpdateServerCommandsToClient(client_t *client, msg_t *message);
void SV_UpdateServerCommandsToClient_PreventOverflow(
    client_t *client, msg_t *message, int32_t maxBytes);
void SV_PrintServerCommandsForClient(client_t *client);

#ifdef __cplusplus
}
#endif

#endif
