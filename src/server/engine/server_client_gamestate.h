#ifndef SHARED_SERVER_CLIENT_GAMESTATE_H
#define SHARED_SERVER_CLIENT_GAMESTATE_H

#include "qcommon/server_runtime_types.h"

typedef enum serverClientPureState_e {
    SERVER_CLIENT_PURE_STATE_PENDING = 0,
    SERVER_CLIENT_PURE_STATE_VALID = 1,
    SERVER_CLIENT_PURE_STATE_INVALID = 2
} serverClientPureState_t;

#ifdef __cplusplus
extern "C" {
#endif

void SV_SendClientGameState(client_t *client);
void SV_ClientEnterWorld(client_t *client, const usercmd_t *command);

#ifdef __cplusplus
}
#endif

#endif
