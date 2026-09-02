#ifndef CODUO_CVAR_PRIVATE_H
#define CODUO_CVAR_PRIVATE_H

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"

extern cvar_t *sv_running;
extern cvar_t *com_logfile;

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
void Com_PrintMessage(int32_t channel, const char *message);
void FS_Printf(int32_t handle, const char *format, ...);
cvar_t *Cvar_Get(const char *name, const char *value, uint32_t flags);
cvar_t *Cvar_Set2(const char *name, const char *value, qboolean force);
void Cvar_Set(const char *name, const char *value);
void Cvar_SetExisting(const char *name, const char *value);
void Cvar_SetValue(const char *name, float value);
const char *Cvar_VariableString(const char *name);
float Cvar_VariableValue(const char *name);
const char *Cvar_InfoString(uint32_t flags);
const char *Cvar_InfoString_Big(uint32_t flags);
void Cvar_SetConfigstringValues(int32_t base, int32_t count, uint32_t flags);
void Cvar_Update(vmCvar_t *vmCvar);
void Cvar_ClearScriptSetServerinfoFlags(void);
void Cvar_AddCommands(void);
void Cvar_DumpToChannel(int32_t channel);
void Cvar_WriteVariables(int32_t handle);
void Cvar_WriteDefaults(int32_t handle);
void Cvar_CommandCompletion(void (*callback)(const char *name));
void Cvar_SetCheatState(void);

#endif
