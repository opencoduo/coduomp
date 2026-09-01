#ifndef QCOMMON_NETCHAN_H
#define QCOMMON_NETCHAN_H

#include "msg.h"
#include "net_types.h"
#include "qcommon_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *net_lanauthorize;
extern cvar_t *net_profile;
extern cvar_t *net_qport;
extern cvar_t *net_showprofile;
extern cvar_t *showdrop;
extern cvar_t *showpackets;
extern netProfileMode_t net_profileActiveMode;
extern const char *const
    net_profileSocketNames[NET_PROFILE_SOCKET_NAME_COUNT];

void NetProf_PrepProfiling(netProfileInfo_t **profile);
void NetProf_AddPacket(netProfileStream_t *stream, int32_t bytes,
                       qboolean fragmented);
void NetProf_NewSendPacket(netchan_t *channel, int32_t bytes,
                           qboolean fragmented);
void NetProf_NewRecievePacket(netchan_t *channel, int32_t bytes,
                              qboolean fragmented);
void NetProf_UpdateStatistics(netProfileStream_t *stream);
void Net_DumpProfile_f(void);

void Netchan_Init(int32_t qport);
void Netchan_Setup(netsrc_t source, netchan_t *channel,
                   netadr_t address, int32_t qport);
void Netchan_TransmitNextFragment(netchan_t *channel);
void Netchan_Transmit(netchan_t *channel, int32_t length,
                      const void *data);
qboolean Netchan_Process(netchan_t *channel, msg_t *message);

#ifdef __cplusplus
}
#endif

#endif
