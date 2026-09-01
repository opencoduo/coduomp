#ifndef Q_CVAR_H
#define Q_CVAR_H

#include "qcommon_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *cvar_vars;
extern uint32_t cvar_modifiedFlags;
extern cvar_t *com_fixedtime; /* CoDUOMP 0x04927eb8; lnxded 0x084897d4 */
extern cvar_t *com_timescale; /* CoDUOMP 0x04927ecc; lnxded 0x08488714 */
extern cvar_t *sv_cheats;
extern cvar_t *sv_console_lockout;

void Cvar_Init(void);
cvar_t *Cvar_Get(const char *name, const char *defaultValue, uint32_t flags);
cvar_t *Cvar_FindVar(const char *name);
cvar_t *Cvar_Set2(const char *name, const char *value, qboolean force);
void Cvar_Set(const char *name, const char *value);
qboolean Cvar_ValidateString(const char *name);
void Cvar_Register(vmCvar_t *vmCvar, const char *name,
                   const char *defaultValue, uint32_t flags);
void Cvar_Update(vmCvar_t *vmCvar);
void Cvar_VMSet(vmCvar_t *vmCvar, const char *value);
float Cvar_VariableValue(const char *name);
int32_t Cvar_VariableIntegerValue(const char *name);
const char *Cvar_VariableString(const char *name);
void Cvar_VariableStringBuffer(const char *name, char *buffer,
                               int32_t bufferLength);
void Cvar_SetLatched(const char *name, const char *value);
void Cvar_Reset(const char *name);
void Cvar_SetExisting(const char *name, const char *value);
void Cvar_SetValue(const char *name, float value);
void Cvar_Set_f(void);
void Cvar_Toggle_f(void);
void Cvar_SetU_f(void);
void Cvar_SetS_f(void);
void Cvar_SetA_f(void);
void Cvar_SetFromCvar_f(void);
void Cvar_Reset_f(void);
void Cvar_List_f(void);
void Cvar_Dump_f(void);
void Cvar_Restart_f(void);
void Cvar_DumpToChannel(int32_t channel);
void Cvar_WriteVariables(int32_t handle);
void Cvar_WriteDefaults(int32_t handle);
void Cvar_ClearScriptSetServerinfoFlags(void);
void Cvar_SetCheatState(void);
qboolean Cvar_Command(void);
void Cvar_CommandCompletion(void (*callback)(const char *name));
void Cvar_AddCommands(void);
void Cvar_Shutdown(void);
void Cvar_InfoStringBuffer(uint32_t flags, char *buffer,
                           int32_t bufferLength);
const char *Cvar_InfoString(uint32_t flags);
const char *Cvar_InfoString_Big(uint32_t flags);
void Cvar_SetConfigstringValues(int32_t base, int32_t count,
                                uint32_t flags);
qboolean Cvar_NextExport(const char **name, const char **string,
                         uint32_t *flags, const char **resetString);
char *PbCvarValidate(char *buffer);
qboolean Com_SaveCvarsToBuffer(const char *const *cvarNames,
                               int32_t cvarCount, char *buffer,
                               size_t bufferSize);
qboolean Com_LoadCvarsFromBuffer(const char *const *cvarNames,
                                 int32_t cvarCount, char *buffer,
                                 const char *fileName);

#ifdef __cplusplus
}
#endif

#endif
