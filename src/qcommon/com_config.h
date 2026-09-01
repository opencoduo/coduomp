#ifndef QCOMMON_COM_CONFIG_H
#define QCOMMON_COM_CONFIG_H

#include "q_shared_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern qboolean com_configAutowriteEnabled;

void Com_WriteConfigToFile(const char *filename);
void Com_WriteDefaultsToFile(const char *filename);
void Com_WriteConfiguration(void);
void Com_WriteConfig_f(void);
void Com_WriteDefaults_f(void);

#ifdef __cplusplus
}
#endif

#endif
