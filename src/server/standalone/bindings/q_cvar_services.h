#ifndef ENGINE_Q_CVAR_SERVICES_H
#define ENGINE_Q_CVAR_SERVICES_H

#include "coduo_engine_structs.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "server/engine/server_configstrings.h"

extern cvar_t *sv_running;
extern cvar_t *com_logfile;

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
void Com_PrintMessage(int32_t channel, const char *message);
void FS_Printf(int32_t handle, const char *format, ...);
#endif
