#ifndef SHARED_SERVER_GAME_BRIDGE_H
#define SHARED_SERVER_GAME_BRIDGE_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_GameSendServerCommand(int32_t clientNum, qboolean reliable,
                              const char *command);
void SV_GameDropClient(int32_t clientNum, const char *reason);
void SV_GetServerinfo(char *buffer, int32_t bufferSize);
void SV_GetUsercmd(int32_t clientNum, usercmd_t *command);
void SV_SetUserinfo(int32_t clientNum, const char *userinfo);
void SV_GetUserinfo(int32_t clientNum, char *buffer, int32_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
