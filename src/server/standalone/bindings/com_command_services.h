#ifndef ENGINE_COM_COMMAND_SERVICES_H
#define ENGINE_COM_COMMAND_SERVICES_H

#include "qcommon/qcommon_runtime_types.h"

#include <stdint.h>

extern int32_t com_errorEntered;

int32_t Cmd_Argc(void);
const char *Cmd_Argv(int32_t argumentIndex);
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
int32_t Com_Milliseconds(void);
void Com_ClearTempMemory(void);
void Com_ClearServerFrameRunningFlag(void);
void SV_Shutdown(const char *finalMessage);
void Hunk_Clear(void);
void Com_Close(void);
void FS_Shutdown(qboolean clearLookupLists);
void FS_ShutdownServerPakNames(void);
void FS_ShutdownServerReferencedPaks(void);
_Noreturn void Sys_Quit(void);

#define COM_QUIT_TARGET_CLEANUP() Com_ClearServerFrameRunningFlag()

#endif
