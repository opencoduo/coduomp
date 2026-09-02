#ifndef CODUOMP_NETWORKING_NET_ADDRESS_H
#define CODUOMP_NETWORKING_NET_ADDRESS_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"
#include "qcommon/net_compare.h"
#include "qcommon/net_loopback.h"
#include "qcommon/net_text.h"
#include "qcommon/net_types.h"
#include "qcommon/netchan.h"

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *showpackets;
extern cvar_t *showdrop;
extern cvar_t *net_profile;
extern cvar_t *net_showprofile;
extern cvar_t *net_qport;
extern cvar_t *net_lanauthorize;
extern netProfileMode_t net_profileActiveMode;
qboolean Sys_GetPacket(netadr_t *address, msg_t *message);
const char *NET_ErrorString(void);
void NET_Sleep(int32_t milliseconds);
void CL_Netchan_SendOOBPacket(netadr_t address, const void *data, int32_t length);

void Sys_SendPacket(int32_t length, const void *data, netadr_t address);
void Sys_SendPacketByName(const char *name, uint16_t port, const void *data, int32_t length);
qboolean Sys_IsLANAddress(netadr_t address);
void Sys_ShowIP(void);
void NET_Config(qboolean enableNetworking);
void Sys_InitNetworking(void);
void Sys_ShutdownNetworking(void);
void NET_Restart_f(void);
#ifdef __cplusplus
}
#endif

#endif
