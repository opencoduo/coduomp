#ifndef SHARED_SERVER_FRAME_H
#define SHARED_SERVER_FRAME_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_RunFrame(void);
void SV_BotUserMove(client_t *client);
void SV_RunBotFrame(void);
void SV_Frame(int32_t msec);

#ifdef __cplusplus
}
#endif

#endif
