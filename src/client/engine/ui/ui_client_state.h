#ifndef CODUOMP_UI_CLIENT_STATE_H
#define CODUOMP_UI_CLIENT_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/q_renderer_types.h"
#include "qcommon/ui_module_abi_types.h"
#include "../networking/net_address.h"

void GetClipboardDataUI(char *buffer, int32_t bufferSize);
void CL_GetGlconfig(glconfig_t *config);
void CLUI_SetPbClStatus(qboolean enabled);
void CLUI_GetCDKey(char *key, int32_t keySize, char *checksum);
void CLUI_SetCDKey(const char *key, const char *checksum);
qboolean GetConfigString(int32_t index, char *buffer, int32_t bufferSize);
qboolean GetClientname(int32_t clientNum, char *buffer, int32_t bufferSize);
void GetClientState(uiClientState_t *state);

#endif
