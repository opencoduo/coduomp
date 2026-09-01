#ifndef QCOMMON_COM_STARTUP_COMMANDS_H
#define QCOMMON_COM_STARTUP_COMMANDS_H

#include "qcommon_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *com_journal;
extern int32_t com_journalFile;
extern int32_t com_journalDataFile;

void Com_ParseCommandLine(char *commandLine);
qboolean Com_SafeMode(void);
void Com_SetSafeMode(void);
void Com_StartupVariable(const char *name);
qboolean Com_AddStartupCommands(void);
void Com_InitJournaling(void);

#ifdef __cplusplus
}
#endif

#endif
