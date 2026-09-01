#ifndef SHARED_SERVER_GAME_LIFECYCLE_H
#define SHARED_SERVER_GAME_LIFECYCLE_H

#include "qcommon/q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void SV_ResetEntityParsePoint(void);
void SV_ShutdownGameProgs(void);
void SV_InitGameVM(qboolean restart, qboolean cvarRestartGate);
void SV_RestartGameProgs(qboolean cvarRestartGate);
void SV_InitGameProgs(qboolean cvarRestartGate);
qboolean SV_GameCommand(void);

#ifdef __cplusplus
}
#endif

#endif
