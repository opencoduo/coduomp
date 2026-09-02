#ifndef CODUOMP_CLIENT_SERVER_BROWSER_H
#define CODUOMP_CLIENT_SERVER_BROWSER_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"
#include "../networking/net_channel.h"
#include "qcommon/server_browser_types.h"

enum {
    LAN_LOCAL_SERVER_CAPACITY = 128,
    LAN_GLOBAL_SERVER_CAPACITY = 20000,
    LAN_FAVORITE_SERVER_CAPACITY = 128,
    CL_MAX_PING_REQUESTS = 16,
    CL_PING_INFO_SIZE = 1024,
    LAN_SERVER_HOSTNAME_SIZE = 32,
    LAN_SERVER_MAPNAME_SIZE = 32,
    LAN_SERVER_GAME_SIZE = 24,
    LAN_SERVER_GAMETYPE_SIZE = 18,
    CL_SERVER_STATUS_SLOT_COUNT = 16,
    CL_SERVER_STATUS_TEXT_SIZE = 8192,
    CL_SERVER_MESSAGE_SIZE = 256,
    CL_CDKEY_STRING_SIZE = 34,
    CL_CDKEY_PART_SIZE = 16,
    CL_CDKEY_CHECKSUM_SIZE = 4,
    CL_CDKEY_CHECKSUM_STORAGE_SIZE = 9,
    CL_PRIMARY_CDKEY_OFFSET = 0,
    CL_UNIQUE_MOD_CDKEY_OFFSET = CL_CDKEY_PART_SIZE,
    CL_PRIMARY_CDKEY_CHECKSUM_OFFSET = 0,
    CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET = 4
};

/* Original 152-byte server-browser record. LAN_GetServerInfo at
 * 0x0041ab80 proves every named field below by pairing its exact load width
 * with the info-string key written for that value. The four trailing strings
 * occupy the record without gaps. */
typedef struct lan_server_info_s {
    netadr_t address;              /* +0x00 */
    uint8_t netType;               /* +0x14 */
    uint8_t clients;               /* +0x15 */
    uint8_t maxClients;            /* +0x16 */
    uint8_t dirty;                 /* +0x17 */
    uint8_t allowAnonymous;        /* +0x18 */
    uint8_t passwordRequired;      /* +0x19 */
    uint8_t pure;                  /* +0x1a */
    int8_t friendlyFire;           /* +0x1b */
    int8_t killCam;                /* +0x1c */
    int8_t consoleDisabled;        /* +0x1d */
    uint8_t hardware;              /* +0x1e */
    uint8_t mod;                   /* +0x1f */
    uint8_t punkBuster;            /* +0x20 */
    uint8_t masterResponseAge;     /* +0x21 */
    int16_t minPing;               /* +0x22 */
    int16_t maxPing;               /* +0x24 */
    int16_t ping;                  /* +0x26 */
    int16_t timeoutsAllowed;       /* +0x28 */
    int16_t jeepsAllowed;          /* +0x2a */
    int16_t tanksAllowed;          /* +0x2c */
    char hostName[LAN_SERVER_HOSTNAME_SIZE]; /* +0x2e */
    char mapName[LAN_SERVER_MAPNAME_SIZE];   /* +0x4e */
    char game[LAN_SERVER_GAME_SIZE];         /* +0x6e */
    char gameType[LAN_SERVER_GAMETYPE_SIZE]; /* +0x86 */
} lan_server_info_t;

/* Original pending-ping record at 0x04dc4660, with a 0x41c-byte stride.
 * A zero address port marks a free slot. */
typedef struct cl_ping_s {
    netadr_t address;                    /* +0x000 */
    int32_t requestStartTime;            /* +0x014 */
    int32_t pingMsec;                    /* +0x018; zero while pending */
    char serverInfo[CL_PING_INFO_SIZE];   /* +0x01c */
} cl_ping_t;

/* Original server-status query record at 0x04df96c0. Status responses retain
 * their printable text until CL_ServerStatus copies it to a caller. */
typedef struct cl_server_status_s {
    char responseText[CL_SERVER_STATUS_TEXT_SIZE]; /* +0x0000 */
    netadr_t address;                              /* +0x2000 */
    int32_t responseReceivedTime;                  /* +0x2014; written but
                                                    * otherwise unused by
                                                    * CoDUOMP.exe */
    int32_t requestStartTime;                      /* +0x2018 */
    qboolean requestPending;                       /* +0x201c */
    qboolean printResponse;                        /* +0x2020 */
    qboolean responseRetrieved;                    /* +0x2024 */
} cl_server_status_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(cl_ping_t) == 4,
               "i386 pending-ping alignment changed");
_Static_assert(offsetof(cl_ping_t, address) == 0x000,
               "original pending-ping address offset");
_Static_assert(sizeof(((cl_ping_t *)0)->address) == 0x014,
               "original pending-ping address extent");
_Static_assert(offsetof(cl_ping_t, requestStartTime) == 0x014,
               "original pending-ping start-time offset");
_Static_assert(offsetof(cl_ping_t, pingMsec) == 0x018,
               "original pending-ping elapsed-time offset");
_Static_assert(offsetof(cl_ping_t, serverInfo) == 0x01c,
               "original pending-ping info-string offset");
_Static_assert(sizeof(((cl_ping_t *)0)->serverInfo) == 0x400,
               "original pending-ping info-string extent");
_Static_assert(sizeof(cl_ping_t) == 0x41c,
               "original pending-ping record extent");

_Static_assert(_Alignof(cl_server_status_t) == 4,
               "i386 server-status alignment changed");
_Static_assert(offsetof(cl_server_status_t, responseText) == 0x0000,
               "original server-status text offset");
_Static_assert(sizeof(((cl_server_status_t *)0)->responseText) == 0x2000,
               "original server-status text extent");
_Static_assert(offsetof(cl_server_status_t, address) == 0x2000,
               "original server-status address offset");
_Static_assert(sizeof(((cl_server_status_t *)0)->address) == 0x014,
               "original server-status address extent");
_Static_assert(offsetof(cl_server_status_t, responseReceivedTime) == 0x2014,
               "original server-status response-time offset");
_Static_assert(offsetof(cl_server_status_t, requestStartTime) == 0x2018,
               "original server-status start-time offset");
_Static_assert(offsetof(cl_server_status_t, requestPending) == 0x201c,
               "original server-status pending offset");
_Static_assert(offsetof(cl_server_status_t, printResponse) == 0x2020,
               "original server-status print offset");
_Static_assert(offsetof(cl_server_status_t, responseRetrieved) == 0x2024,
               "original server-status retrieved offset");
_Static_assert(sizeof(cl_server_status_t) == 0x2028,
               "original server-status record extent");

_Static_assert(_Alignof(lan_server_info_t) == 4,
               "i386 LAN server-record alignment changed");
_Static_assert(offsetof(lan_server_info_t, address) == 0x00,
               "original LAN server address offset");
_Static_assert(sizeof(((lan_server_info_t *)0)->address) == 0x14,
               "original LAN server address extent");
_Static_assert(offsetof(lan_server_info_t, netType) == 0x14,
               "original LAN server network-type offset");
_Static_assert(offsetof(lan_server_info_t, clients) == 0x15,
               "original LAN server client-count offset");
_Static_assert(offsetof(lan_server_info_t, maxClients) == 0x16,
               "original LAN server client-limit offset");
_Static_assert(offsetof(lan_server_info_t, dirty) == 0x17,
               "original LAN server dirty offset");
_Static_assert(offsetof(lan_server_info_t, allowAnonymous) == 0x18,
               "original LAN server anonymous-access offset");
_Static_assert(offsetof(lan_server_info_t, passwordRequired) == 0x19,
               "original LAN server password-required offset");
_Static_assert(offsetof(lan_server_info_t, pure) == 0x1a,
               "original LAN server pure-server offset");
_Static_assert(offsetof(lan_server_info_t, friendlyFire) == 0x1b,
               "original LAN server friendly-fire offset");
_Static_assert(offsetof(lan_server_info_t, killCam) == 0x1c,
               "original LAN server kill-cam offset");
_Static_assert(offsetof(lan_server_info_t, consoleDisabled) == 0x1d,
               "original LAN server console-disabled offset");
_Static_assert(offsetof(lan_server_info_t, hardware) == 0x1e,
               "original LAN server hardware offset");
_Static_assert(offsetof(lan_server_info_t, mod) == 0x1f,
               "original LAN server mod offset");
_Static_assert(offsetof(lan_server_info_t, punkBuster) == 0x20,
               "original LAN server PunkBuster offset");
_Static_assert(offsetof(lan_server_info_t, masterResponseAge) == 0x21,
               "original LAN server master-response-age offset");
_Static_assert(offsetof(lan_server_info_t, minPing) == 0x22,
               "original LAN server minimum-ping offset");
_Static_assert(offsetof(lan_server_info_t, maxPing) == 0x24,
               "original LAN server maximum-ping offset");
_Static_assert(offsetof(lan_server_info_t, ping) == 0x26,
               "original LAN server ping offset");
_Static_assert(offsetof(lan_server_info_t, timeoutsAllowed) == 0x28,
               "original LAN server timeouts-allowed offset");
_Static_assert(offsetof(lan_server_info_t, jeepsAllowed) == 0x2a,
               "original LAN server jeeps-allowed offset");
_Static_assert(offsetof(lan_server_info_t, tanksAllowed) == 0x2c,
               "original LAN server tanks-allowed offset");
_Static_assert(offsetof(lan_server_info_t, hostName) == 0x2e,
               "original LAN server hostname offset");
_Static_assert(sizeof(((lan_server_info_t *)0)->hostName) == 0x20,
               "original LAN server hostname extent");
_Static_assert(offsetof(lan_server_info_t, mapName) == 0x4e,
               "original LAN server map-name offset");
_Static_assert(sizeof(((lan_server_info_t *)0)->mapName) == 0x20,
               "original LAN server map-name extent");
_Static_assert(offsetof(lan_server_info_t, game) == 0x6e,
               "original LAN server game offset");
_Static_assert(sizeof(((lan_server_info_t *)0)->game) == 0x18,
               "original LAN server game extent");
_Static_assert(offsetof(lan_server_info_t, gameType) == 0x86,
               "original LAN server game-type offset");
_Static_assert(sizeof(((lan_server_info_t *)0)->gameType) == 0x12,
               "original LAN server game-type extent");
_Static_assert(sizeof(lan_server_info_t) == 0x98,
               "original LAN server record extent");
#endif

extern cl_ping_t cl_pingList[CL_MAX_PING_REQUESTS];
extern qboolean cls_autoupdateServerResolved;
extern qboolean cl_updateStarted;
extern cl_server_status_t cl_serverStatusList[CL_SERVER_STATUS_SLOT_COUNT];
extern int32_t cl_serverStatusNextSlot;
extern cvar_t *cl_serverStatusResendTime;
extern char cl_cdkey[CL_CDKEY_STRING_SIZE];
extern char cl_cdkeyChecksums[CL_CDKEY_CHECKSUM_STORAGE_SIZE];

int CL_CompareAdrSigned(const void *left, const void *right);
void CL_InitServerInfo(lan_server_info_t *server, netadr_t address);
qboolean CL_FindServerInfo(netadr_t address);
void CL_SortGlobalServers(void);
void CL_MotdPacket(netadr_t address);
const char *PB_Q_Serveraddr(void);
const char *PB_Q_Serverinfo(void);
void CL_SendPbPacket(int32_t length, const uint8_t *data);
void PBget_cl_cdkey(char *destination);
void CL_DisconnectPacket(netadr_t address);
void CL_ConnectionlessPacket(netadr_t address, msg_t *message,
                             int32_t packetTime);
void CL_ServersResponsePacket(msg_t *message);
void CL_ServerInfoPacket(netadr_t address, msg_t *message,
                         int32_t packetTime);
void CL_UpdateInfoPacket(netadr_t address);
void CL_MapLoading(const char *mapName, const char *gameType);
void CL_SetServerInfo(lan_server_info_t *server, const char *info,
                      int32_t pingTime);
qboolean LAN_LoadCachedServersInternal(int32_t fileHandle);
void LAN_LoadCachedServers(void);
void LAN_SaveServersToCache(void);
qboolean LAN_WaitServerResponse(lan_server_source_t source);
int32_t LAN_GetPingQueueCount(void);
void LAN_ClearPing(int32_t pingIndex);
void LAN_GetPing(int32_t pingIndex, char *address, int32_t addressSize,
                 int32_t *pingTime);
void LAN_GetPingInfo(int32_t pingIndex, char *buffer, int32_t bufferSize);
qboolean LAN_UpdateDirtyPings(lan_server_source_t source);
qboolean LAN_GetServerStatus(const char *address, char *status,
                             int32_t statusSize);
void LAN_MarkServerDirty(lan_server_source_t source, int32_t serverIndex,
                         qboolean dirty);
qboolean LAN_ServerIsDirty(lan_server_source_t source, int32_t serverIndex);
int32_t LAN_GetServerCount(lan_server_source_t source);
lan_server_info_t *LAN_GetServerPtr(lan_server_source_t source,
                                     int32_t serverIndex);
int32_t LAN_GetServerPing(lan_server_source_t source, int32_t serverIndex);
int32_t LAN_GetServerPunkBuster(lan_server_source_t source,
                                int32_t serverIndex);
void LAN_ResetPings(lan_server_source_t source);
void LAN_GetServerAddressString(lan_server_source_t source,
                                int32_t serverIndex, char *buffer,
                                int32_t bufferSize);
void LAN_GetServerInfo(lan_server_source_t source, int32_t serverIndex,
                       char *buffer, int32_t bufferSize);
int32_t LAN_AddServer(lan_server_source_t source, const char *name,
                      const char *address);
void LAN_RemoveServer(lan_server_source_t source, const char *address);
void LAN_CleanHostname(const char *source, char *destination);
int32_t LAN_CompareHostname(const char *left, const char *right);
int32_t LAN_CompareServers(lan_server_source_t source,
                           lan_server_sort_key_t sortKey,
                           qboolean sortDescending, int32_t firstServerIndex,
                           int32_t secondServerIndex);

int32_t CL_GetPingQueueCount(void);
cl_ping_t *CL_GetFreePing(void);
void CL_ClearPing(int32_t pingIndex);
void CL_GetPingInfo(int32_t pingIndex, char *buffer, int32_t bufferSize);
void CL_Ping_f(void);
void CL_LocalServers_f(void);
void CL_GlobalServers_f(void);
void CL_GetPing(int32_t pingIndex, char *address, int32_t addressSize,
                int32_t *pingTime);
qboolean CL_UpdateDirtyPings(lan_server_source_t source);
cl_server_status_t *CL_GetServerStatus(netadr_t address);
qboolean CL_ServerStatus(const char *address, char *status,
                         int32_t statusSize);
void CL_ServerStatusResponse(netadr_t address, msg_t *message);
void CL_ServerStatus_f(void);
void CL_SetServerInfoByAddress(netadr_t address, const char *info,
                               int32_t pingTime);
const char *CL_GetServerIPAddress(void);

#endif
