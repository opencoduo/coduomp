#ifndef SHARED_SERVER_OPERATOR_CLIENTS_H
#define SHARED_SERVER_OPERATOR_CLIENTS_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

client_t *SV_GetPlayerByName(void);
client_t *SV_GetPlayerByNum(void);
int32_t SV_KickClient(client_t *client, char *nameOut, int32_t nameOutSize);
int32_t SV_KickUser_f(char *nameOut, int32_t nameOutSize);
int32_t SV_KickClient_f(char *nameOut, int32_t nameOutSize);
void SV_TempBan_f(void);
void SV_Ban_f(void);
void SV_BanNum_f(void);
void SV_Unban_f(void);
void SV_Drop_f(void);
void SV_DropNum_f(void);
void SV_TempBanNum_f(void);

#ifdef __cplusplus
}
#endif

#endif
