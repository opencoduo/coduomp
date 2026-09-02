#include "server_browser.h"

#include "cgame.h"
#include "../networking/net_address.h"
#include "../ui/ui_client_state.h"
#include "client/common/client_legacy_crt.h"
#include "compat/crt/qsort_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Original Win32 16-entry pending-ping list at 0x04dc4660. */
cl_ping_t cl_pingList[CL_MAX_PING_REQUESTS];
/* Original Win32 16-entry server-status response cache at 0x04df96c0 and its
 * fallback round-robin selector at 0x04ad3d40. */
cl_server_status_t cl_serverStatusList[CL_SERVER_STATUS_SLOT_COUNT];
int32_t cl_serverStatusNextSlot;
/* Original server-status resend interval cvar pointer at 0x04e19968. */
cvar_t *cl_serverStatusResendTime;
static char cl_serverIPAddress[128];

typedef enum coduomp_server_player_query_state_e {
    CODUOMP_SERVER_PLAYER_QUERY_QUEUED = 0,
    CODUOMP_SERVER_PLAYER_QUERY_PENDING = 1,
    CODUOMP_SERVER_PLAYER_QUERY_COMPLETE = 2
} coduomp_server_player_query_state_t;

typedef struct coduomp_server_player_query_s {
    netadr_t address;
    int32_t requestStartTime;
    int32_t timeoutMsec;
    coduomp_server_player_query_state_t state;
} coduomp_server_player_query_t;

static coduomp_server_player_query_t coduomp_serverPlayerQueries[LAN_GLOBAL_SERVER_CAPACITY];
static int32_t coduomp_serverPlayerQueryCount;

typedef struct coduomp_server_bot_cache_entry_s {
    netadr_t address;
    uint8_t cachedBotCount;
    uint8_t displayedBotCount;
    uint8_t occupied;
} coduomp_server_bot_cache_entry_t;

enum {
    CODUOMP_SERVER_BOT_CACHE_CAPACITY = 65536
};

#define CODUOMP_SERVER_ADDRESS_HASH_OFFSET UINT32_C(2166136261)
#define CODUOMP_SERVER_ADDRESS_HASH_PRIME UINT32_C(16777619)

static coduomp_server_bot_cache_entry_t coduomp_serverBotCache[CODUOMP_SERVER_BOT_CACHE_CAPACITY];

/* Mac symbol and the call at 0x0041b2f8 identify the message-pump boundary.
 * Its recovery is owned by the common event-loop subsystem. */

enum {
    LAN_SERVER_INFO_STRING_SIZE = MAX_STRING_CHARS,
    LAN_SERVER_PRINT_MESSAGE_CAPACITY = LAN_SERVER_INFO_STRING_SIZE + 1,
    LAN_SERVER_CACHE_VERSION = 1,
    LAN_ADD_SERVER_DUPLICATE = 0,
    LAN_ADD_SERVER_ADDED = 1,
    LAN_ADD_SERVER_LIST_FULL = -1,
    LAN_ADD_SERVER_BAD_ADDRESS = -2,
    LAN_SERVER_RECENT_MASTER_RESPONSE_AGE_LIMIT = 3,
    CL_SERVERINFO_CONFIGSTRING = 0,
    CL_CDKEY_PB_COPY_LIMIT = 32,
    CODUOMP_SERVER_BOT_PING = 999,
    CODUOMP_SERVER_STATUS_MIN_TIMEOUT_MSEC = 1000,
    CODUOMP_SERVER_STATUS_MAX_TIMEOUT_MSEC = 3000,
    CODUOMP_SERVER_STATUS_TIMEOUT_BIAS_MSEC = 250
};

/* Original Win32 CD-key storage at 0x005c51cc. The primary CoD:UO key and an
 * optional fs_game-specific unique key share cl_cdkey. The checksum byte region
 * begins at 0x005c51f0: the primary checksum occupies offsets 0..3 and the
 * unique-mod checksum occupies offsets 4..7. Offset 4 is also where primary-key
 * paths write their terminating NUL, so this is one overlapping nine-byte
 * region rather than two independent C arrays. */
char cl_cdkey[CL_CDKEY_STRING_SIZE] = "                                "; /* original 0x005c51cc */
char cl_cdkeyChecksums[CL_CDKEY_CHECKSUM_STORAGE_SIZE] = {' ', ' ', ' ', ' '}; /* original 0x005c51f0 */

static const char lan_serverCacheFileName[] = "uoservercache.dat";

/* The cache compatibility marker is the combined byte count of the two
 * serialized arrays. It is 0x2eaf00 in the original build: 0x2e6300 bytes of
 * global-server records followed by 0x4c00 bytes of favorite records. */
static const int32_t lan_serverCacheArrayBytes = (int32_t)(sizeof(cls.globalServers) + sizeof(cls.favoriteServers));

/* Source: CoDUOMP.exe 0x00412500..0x0041250c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00412500_0041250d.mcode.
 * Name and qsort-compatible argument roles: exact same-module Mac symbol
 * CL_CompareAdrSigned. */
int CL_CompareAdrSigned(const void *left, const void *right)
{
    const lan_server_info_t *leftServer = left;
    const lan_server_info_t *rightServer = right;
    return NET_CompareAdrSigned(&leftServer->address, &rightServer->address);
}

/* Source: CoDUOMP.exe 0x00412510..0x0041252d, recovered from an exporter
 * function-boundary gap.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_SortGlobalServers. */
void CL_SortGlobalServers(void)
{
    coduo_crt_qsort(cls.globalServers, (size_t)cls.numGlobalServers, sizeof(cls.globalServers[0]), CL_CompareAdrSigned);
}

/* Source: CoDUOMP.exe 0x004124a0..0x004124f6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004124a0_004124f7.mcode.
 * Name and by-value address argument: exact same-module Mac symbol
 * CL_InitServerInfo. The function deliberately initializes only this subset
 * of the record. In particular, +0x21 is live state rather than alignment:
 * the master-query path at 0x004164b5 increments it as a saturating age, and
 * LAN_SaveServersToCache rejects records whose age has reached three. */
void CL_InitServerInfo(lan_server_info_t *server, netadr_t address)
{
    server->address = address;
    server->clients = 0;
    server->hostName[0] = '\0';
    server->mapName[0] = '\0';
    server->maxClients = 0;
    server->maxPing = 0;
    server->minPing = 0;
    server->ping = -1;
    server->game[0] = '\0';
    server->gameType[0] = '\0';
    server->netType = 0;
    server->allowAnonymous = 0;
    server->dirty = 1;
    server->masterResponseAge = 0;
}

/* Source: CoDUOMP.exe 0x00412530..0x00412608.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00412530_00412609.mcode.
 * Name and address argument: exact same-module Mac symbol
 * CL_FindServerInfo. A match refreshes every adjacent copy of the address in
 * the sorted global list; the return value tells the master-response parser
 * whether it must append a new record. */
qboolean CL_FindServerInfo(netadr_t address)
{
    int32_t lower = 0;
    int32_t upper = cls.numGlobalServers;

    while (lower < upper) {
        const int32_t middle = (lower + upper) / 2;
        const int32_t comparison = NET_CompareAdrSigned(&address, &cls.globalServers[middle].address);

        if (comparison < 0) {
            upper = middle;
            continue;
        }
        if (comparison > 0) {
            lower = middle + 1;
            continue;
        }

        int32_t first = middle;
        while (first > 0 && NET_CompareAdrSigned(&address, &cls.globalServers[first - 1].address) == 0) {
            --first;
        }

        for (int32_t index = first; index < cls.numGlobalServers; ++index) {
            if (NET_CompareAdrSigned(&address, &cls.globalServers[index].address) != 0) {
                break;
            }
            CL_InitServerInfo(&cls.globalServers[index], address);
        }
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00412390..0x00412492.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00412390_00412493.mcode.
 * Name and address argument: exact same-module Mac symbol CL_MotdPacket.
 * Only the configured update server and the outstanding challenge may update
 * the retained info string and user-visible MOTD cvar. */
void CL_MotdPacket(netadr_t address)
{
    if (NET_CompareAdrSigned(&address, &cls.updateServer) != 0)
        return;

    const char *info = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if (strcmp(Info_ValueForKey(info, "challenge"), cls.updateChallenge) != 0) {
        return;
    }

    const char *motd = Info_ValueForKey(info, "motd");
    strncpy(cls.updateInfoString, info, sizeof(cls.updateInfoString) - 1);
    cls.updateInfoString[sizeof(cls.updateInfoString) - 1] = '\0';
    (void)Cvar_Set2("cl_motdString", motd, qtrue);
}

/* Source: CoDUOMP.exe 0x00412280..0x004122b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00412280_004122ba.mcode.
 * Name and return role: exact same-module Mac symbol PB_Q_Serveraddr. */
const char *PB_Q_Serveraddr(void)
{
    return NET_AdrToString(clc.serverAddress);
}

/* Source: CoDUOMP.exe 0x00416ec0..0x00416ec5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416ec0_00416ec5.mcode.
 * Name: exact same-module Mac symbol CL_ShowIP_f. The original is a tail
 * wrapper around the platform networking implementation. */
void CL_ShowIP_f(void)
{
    Sys_ShowIP();
}

/* Source: CoDUOMP.exe 0x00416ed0..0x00416f3e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416ed0_00416f3f.mcode.
 * Name and return role: exact same-module Mac symbol
 * CL_GetServerIPAddress. The original uses a dedicated 128-byte buffer,
 * rather than NET_AdrToString's rotating storage. */
const char *CL_GetServerIPAddress(void)
{
    if (cls.state < CA_CONNECTED) {
        memset(cl_serverIPAddress, 0, sizeof(cl_serverIPAddress));
        return cl_serverIPAddress;
    }

    Com_sprintf(cl_serverIPAddress, sizeof(cl_serverIPAddress), "%i.%i.%i.%i:%i", clc.serverAddress.ip[0], clc.serverAddress.ip[1],
                clc.serverAddress.ip[2], clc.serverAddress.ip[3], (int32_t)(int16_t)BigShort((int16_t)clc.serverAddress.port));
    return cl_serverIPAddress;
}

/* Source: CoDUOMP.exe 0x00412270..0x0041227b.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00412270_0041227c.mcode.
 * Name and return role: exact same-module Mac symbol PB_Q_Serverinfo. */
const char *PB_Q_Serverinfo(void)
{
    return &cl.gameState.stringData[cl.gameState.stringOffsets[CL_SERVERINFO_CONFIGSTRING]];
}

/* Source: CoDUOMP.exe 0x00412230..0x0041226e.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00412230_0041226f.mcode.
 * Name and argument roles: exact same-module Mac symbol CL_SendPbPacket. */
void CL_SendPbPacket(int32_t length, const uint8_t *data)
{
    NET_OutOfBandPbPacket(NS_CLIENT, clc.serverAddress, data, length);
}

/* Source: CoDUOMP.exe 0x004121d0..0x0041222c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004121d0_0041222d.mcode.
 * Name and destination role: exact same-module Mac symbol PBget_cl_cdkey.
 * PunkBuster receives at most the first 32 alphanumeric CD-key characters;
 * separators and any other punctuation are omitted. */
void PBget_cl_cdkey(char *destination)
{
    destination[0] = '\0';

    size_t sourceLength = strlen(cl_cdkey);
    if (sourceLength > CL_CDKEY_PB_COPY_LIMIT)
        sourceLength = CL_CDKEY_PB_COPY_LIMIT;

    size_t destinationLength = 0;
    for (size_t index = 0; index < sourceLength; ++index) {
        const char character = cl_cdkey[index];
        if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')) {
            destination[destinationLength++] = character;
        }
    }
    destination[destinationLength] = '\0';
}

/* Source: CoDUOMP.exe 0x004122c0..0x0041238c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004122c0_0041238d.mcode.
 * Name and address argument: exact same-module Mac symbol
 * CL_DisconnectPacket. The three-second guard prevents an old or forged
 * connectionless disconnect from immediately tearing down an active channel. */
void CL_DisconnectPacket(netadr_t address)
{
    enum {
        CL_DISCONNECT_PACKET_GRACE_MSEC = 3000,
    };

    if (cls.state == CA_DISCONNECTED)
        return;
    if (NET_CompareAdrSigned(&address, &clc.netchan.remoteAddress) != 0)
        return;
    if (cls.realtime - clc.lastPacketTime < CL_DISCONNECT_PACKET_GRACE_MSEC) {
        return;
    }

    if (cls.wwwDownloadDisconnected == 0) {
        Com_Error(ERR_DROP, "EXE_SERVER_DISCONNECTED");
        CL_Disconnect(qtrue);
        return;
    }

    CL_Disconnect(qfalse);
    (void)Cvar_Set2("ui_dl_running", "1", qtrue);
}

/* Source: CoDUOMP.exe 0x00412610..0x00412875.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00412610_00412876.mcode.
 * Name and message argument: exact same-module Mac symbol
 * CL_ServersResponsePacket. The master protocol packs each address as a
 * backslash, four IPv4 octets, and a two-byte network-order port; `\EOT`
 * terminates the list. */
void CL_ServersResponsePacket(msg_t *message)
{
    typedef struct master_server_address_s {
        uint8_t ip[4];
        uint16_t port;
    } master_server_address_t;
    enum {
        MAX_MASTER_SERVERS_PER_PACKET = 256
    };
#if UINTPTR_MAX == UINT32_MAX
    _Static_assert(_Alignof(master_server_address_t) == 2, "i386 master-response address alignment changed");
    _Static_assert(offsetof(master_server_address_t, ip) == 0x0, "original master-response IP offset");
    _Static_assert(sizeof(((master_server_address_t *)0)->ip) == 0x4, "original master-response IP extent");
    _Static_assert(offsetof(master_server_address_t, port) == 0x4, "original master-response port offset");
    _Static_assert(sizeof(master_server_address_t) == 0x6, "original master-response address extent");
#endif

    master_server_address_t parsedAddresses[MAX_MASTER_SERVERS_PER_PACKET];
    int32_t parsedCount = 0;

    Com_PumpMessageLoop();
    Com_Printf("CL_ServersResponsePacket\n");
    cls.waitingForMasterResponse = qfalse;

    uint8_t *cursor = message->data;
    uint8_t *const end = message->data + message->cursize;
    /* 0x0041263f..0x00412657 uses data + 1 only for this initial
     * minimum-length gate; the scan at 0x00412667 starts from data itself. */
    if (cursor + 1 < end) {
        while (cursor < end) {
            while (cursor < end && *cursor != '\\')
                ++cursor;
            if (cursor >= end)
                break;

            ++cursor;
            if (end - cursor <= 6)
                break;

            master_server_address_t *parsed = &parsedAddresses[parsedCount];
            memcpy(parsed->ip, cursor, sizeof(parsed->ip));
            const uint16_t networkPort = (uint16_t)(((uint16_t)cursor[4] << 8) | cursor[5]);
            parsed->port = (uint16_t)BigShort((int16_t)networkPort);
            cursor += 6;

            if (*cursor != '\\')
                break;

            Com_DPrintf("server: %d ip: %d.%d.%d.%d:%d\n", parsedCount, parsed->ip[0], parsed->ip[1], parsed->ip[2], parsed->ip[3],
                        parsed->port);
            ++parsedCount;

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (parsedCount >= MAX_MASTER_SERVERS_PER_PACKET ||
                (end - cursor >= 4 && cursor[1] == 'E' && cursor[2] == 'O' && cursor[3] == 'T')) {
                break;
            }
        }
    }

    /* Keep the published count fixed while appending. CL_FindServerInfo
     * binary-searches the published, sorted prefix; 0x00412721 snapshots that
     * count, 0x004127b4 increments only the stack-local copy, and 0x00412841
     * publishes it immediately before the final qsort. */
    int32_t globalServerCount = cls.numGlobalServers;
    for (int32_t index = 0; index < parsedCount && globalServerCount < LAN_GLOBAL_SERVER_CAPACITY; ++index) {
        netadr_t address;
        address.type = NA_IP;
        memcpy(address.ip, parsedAddresses[index].ip, sizeof(address.ip));
        address.port = parsedAddresses[index].port;

        if (CL_FindServerInfo(address) == qfalse) {
            CL_InitServerInfo(&cls.globalServers[globalServerCount], address);
            ++globalServerCount;
        }
    }

    cls.numGlobalServers = globalServerCount;
    coduo_crt_qsort(cls.globalServers, (size_t)globalServerCount, sizeof(cls.globalServers[0]), CL_CompareAdrSigned);
    Com_Printf("%d servers parsed (total %d)\n", parsedCount, globalServerCount);
}

/* NOT_FROM_ORIGINAL_SOURCE: hash the fields used by
 * NET_CompareAdrSigned so cached roster metadata follows a server across
 * browser lists and refresh generations. */
static uint32_t coduomp_server_address_hash(netadr_t address)
{
    uint32_t hash = CODUOMP_SERVER_ADDRESS_HASH_OFFSET;
    hash = (hash ^ (uint32_t)address.type) * CODUOMP_SERVER_ADDRESS_HASH_PRIME;

    const uint8_t *addressBytes = NULL;
    size_t addressByteCount = 0;
    if (address.type == NA_IP) {
        addressBytes = address.ip;
        addressByteCount = sizeof(address.ip);
    } else if (address.type == NA_IPX) {
        addressBytes = address.ipx;
        addressByteCount = sizeof(address.ipx);
    }

    if (addressBytes != NULL) {
        hash = (hash ^ (uint32_t)address.port) * CODUOMP_SERVER_ADDRESS_HASH_PRIME;
        for (size_t index = 0; index < addressByteCount; ++index) {
            hash = (hash ^ addressBytes[index]) * CODUOMP_SERVER_ADDRESS_HASH_PRIME;
        }
    }
    return hash;
}

/* NOT_FROM_ORIGINAL_SOURCE: look up the persistent bot-count sidecar without
 * changing the original fixed-layout lan_server_info_t records. */
static coduomp_server_bot_cache_entry_t *coduomp_find_server_bot_cache(netadr_t address, qboolean create)
{
    const uint32_t slotMask = CODUOMP_SERVER_BOT_CACHE_CAPACITY - 1u;
    const uint32_t firstSlot = coduomp_server_address_hash(address) & slotMask;

    for (uint32_t offset = 0; offset < CODUOMP_SERVER_BOT_CACHE_CAPACITY; ++offset) {
        coduomp_server_bot_cache_entry_t *const entry = &coduomp_serverBotCache[(firstSlot + offset) & slotMask];
        if (entry->occupied == 0) {
            if (create == qfalse)
                return NULL;
            entry->address = address;
            entry->cachedBotCount = 0;
            entry->displayedBotCount = 0;
            entry->occupied = 1;
            return entry;
        }
        if (NET_CompareAdrSigned(&address, &entry->address) == 0)
            return entry;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x00415180..0x0041537e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00415180_0041537f.mcode and the exact
 * key strings at 0x0058e7e0/0x00593334..0x005933dc.
 * Name and argument roles: exact same-module Mac symbol CL_SetServerInfo.
 * A null info string updates only the ping, while a non-null response replaces
 * the browser fields represented by the received info keys. */
void CL_SetServerInfo(lan_server_info_t *server, const char *info, int32_t pingTime)
{
    if (server == NULL)
        return;

    if (info != NULL) {
        const uint8_t aggregateClientCount = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "clients"));
        coduomp_server_bot_cache_entry_t *const botCache = coduomp_find_server_bot_cache(server->address, qtrue);
        uint8_t displayedBotCount = 0;
        if (botCache != NULL) {
            displayedBotCount = botCache->cachedBotCount < aggregateClientCount ? botCache->cachedBotCount : aggregateClientCount;
            botCache->displayedBotCount = displayedBotCount;
        }
        server->clients = aggregateClientCount - displayedBotCount;

        strncpy(server->hostName, Info_ValueForKey(info, "hostname"), sizeof(server->hostName) - 1);
        server->hostName[sizeof(server->hostName) - 1] = '\0';

        strncpy(server->mapName, Info_ValueForKey(info, "mapname"), sizeof(server->mapName) - 1);
        server->mapName[sizeof(server->mapName) - 1] = '\0';

        server->maxClients = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "sv_maxclients"));

        strncpy(server->game, Info_ValueForKey(info, "game"), sizeof(server->game) - 1);
        server->game[sizeof(server->game) - 1] = '\0';

        /* The original copies 15 bytes although the containing record reserves
         * 18 bytes for gameType; bytes 16 and 17 retain their prior values. */
        strncpy(server->gameType, Info_ValueForKey(info, "gametype"), 15);
        server->gameType[15] = '\0';

        server->netType = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "nettype"));
        server->minPing = (int16_t)coduo_crt_atoi(Info_ValueForKey(info, "minping"));
        server->maxPing = (int16_t)coduo_crt_atoi(Info_ValueForKey(info, "maxping"));
        server->allowAnonymous = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "sv_allowAnonymous"));
        server->consoleDisabled = (int8_t)coduo_crt_atoi(Info_ValueForKey(info, "con_disabled"));
        server->passwordRequired = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "pswrd"));
        server->pure = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "pure"));
        server->friendlyFire = (int8_t)coduo_crt_atoi(Info_ValueForKey(info, "ff"));
        server->killCam = (int8_t)coduo_crt_atoi(Info_ValueForKey(info, "kc"));
        server->hardware = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "hw"));
        server->mod = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "mod"));
        server->timeoutsAllowed = (int16_t)coduo_crt_atoi(Info_ValueForKey(info, "timeoutsallowed"));
        server->jeepsAllowed = (int16_t)coduo_crt_atoi(Info_ValueForKey(info, "jps"));
        server->tanksAllowed = (int16_t)coduo_crt_atoi(Info_ValueForKey(info, "tnk"));
        server->punkBuster = (uint8_t)coduo_crt_atoi(Info_ValueForKey(info, "pb"));
    }

    server->ping = (int16_t)pingTime;
}

/* Source: CoDUOMP.exe 0x00415380..0x004153eb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00415380_004153ec.mcode.
 * The original helper has no Mac traceback symbol. Its role is proven by the
 * two ui_lastServerRefresh cvar format strings and the clean-server/client
 * accumulation over the selected browser list. */
static void CL_UpdateServerRefreshCvars(lan_server_info_t *servers, int32_t serverCount, lan_server_source_t source)
{
    int32_t cleanServerCount = 0;
    int32_t playerCount = 0;

    for (int32_t index = 0; index < serverCount; ++index) {
        if (servers[index].dirty == 0) {
            playerCount += servers[index].clients;
            ++cleanServerCount;
        }
    }

    const char *value = va("%i", cleanServerCount);
    const char *name = va("ui_lastServerRefreshServers_%i", source);
    (void)Cvar_Set2(name, value, qtrue);

    value = va("%i", playerCount);
    name = va("ui_lastServerRefreshPlayers_%i", source);
    (void)Cvar_Set2(name, value, qtrue);
}

/* Source: CoDUOMP.exe 0x004153f0..0x004155ed.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004153f0_004155ee.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * CL_SetServerInfoByAddress. Local and favorite matching deliberately scan
 * their complete 128-slot storage; the sorted global list uses binary search
 * and then updates every adjacent duplicate address. */
void CL_SetServerInfoByAddress(netadr_t address, const char *info, int32_t pingTime)
{
    Com_PumpMessageLoop();

    for (int32_t index = 0; index < LAN_LOCAL_SERVER_CAPACITY; ++index) {
        if (NET_CompareAdrSigned(&address, &cls.localServers[index].address) == 0) {
            CL_SetServerInfo(&cls.localServers[index], info, pingTime);
            CL_UpdateServerRefreshCvars(cls.localServers, cls.numLocalServers, LAN_SERVER_SOURCE_LOCAL);
        }
    }

    int32_t lower = 0;
    int32_t upper = cls.numGlobalServers;
    while (lower < upper) {
        const int32_t middle = (lower + upper) / 2;
        const int32_t comparison = NET_CompareAdrSigned(&address, &cls.globalServers[middle].address);

        if (comparison < 0) {
            upper = middle;
            continue;
        }
        if (comparison > 0) {
            lower = middle + 1;
            continue;
        }

        int32_t first = middle;
        while (first > 0 && NET_CompareAdrSigned(&address, &cls.globalServers[first - 1].address) == 0) {
            --first;
        }

        for (int32_t index = first; index < cls.numGlobalServers; ++index) {
            if (NET_CompareAdrSigned(&address, &cls.globalServers[index].address) != 0) {
                break;
            }
            CL_SetServerInfo(&cls.globalServers[index], info, pingTime);
            CL_UpdateServerRefreshCvars(cls.globalServers, cls.numGlobalServers, LAN_SERVER_SOURCE_GLOBAL);
        }
        break;
    }

    for (int32_t index = 0; index < LAN_FAVORITE_SERVER_CAPACITY; ++index) {
        if (NET_CompareAdr(address, cls.favoriteServers[index].address) != qfalse) {
            CL_SetServerInfo(&cls.favoriteServers[index], info, pingTime);
            CL_UpdateServerRefreshCvars(cls.favoriteServers, cls.numFavoriteServers, LAN_SERVER_SOURCE_FAVORITES);
        }
    }
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): discard the automatic
 * getstatus generation whenever the UI starts a new server refresh. */
static void coduomp_reset_server_player_queries(void)
{
    coduomp_serverPlayerQueryCount = 0;
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): queue a bounded getstatus
 * request for a nonempty server. The timeout follows the successful getinfo
 * round trip, with limits that prevent one silent server from stalling the
 * browser indefinitely. */
static void coduomp_queue_server_player_query(netadr_t address, int32_t pingMsec)
{
    if (coduomp_serverPlayerQueryCount >= LAN_GLOBAL_SERVER_CAPACITY)
        return;

    int32_t timeoutMsec = CODUOMP_SERVER_STATUS_MIN_TIMEOUT_MSEC;
    if (pingMsec > 0 && pingMsec <= (CODUOMP_SERVER_STATUS_MAX_TIMEOUT_MSEC - CODUOMP_SERVER_STATUS_TIMEOUT_BIAS_MSEC) / 2) {
        timeoutMsec = pingMsec * 2 + CODUOMP_SERVER_STATUS_TIMEOUT_BIAS_MSEC;
        if (timeoutMsec < CODUOMP_SERVER_STATUS_MIN_TIMEOUT_MSEC)
            timeoutMsec = CODUOMP_SERVER_STATUS_MIN_TIMEOUT_MSEC;
    } else if (pingMsec > 0) {
        timeoutMsec = CODUOMP_SERVER_STATUS_MAX_TIMEOUT_MSEC;
    }

    coduomp_server_player_query_t *const query = &coduomp_serverPlayerQueries[coduomp_serverPlayerQueryCount++];
    query->address = address;
    query->requestStartTime = 0;
    query->timeoutMsec = timeoutMsec;
    query->state = CODUOMP_SERVER_PLAYER_QUERY_QUEUED;
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): replace the aggregate
 * getinfo count in every browser list containing this address with the
 * heuristic human count obtained from getstatus. */
static void coduomp_apply_server_human_count(netadr_t address, uint8_t humanCount)
{
    qboolean localChanged = qfalse;
    for (int32_t index = 0; index < LAN_LOCAL_SERVER_CAPACITY; ++index) {
        if (NET_CompareAdrSigned(&address, &cls.localServers[index].address) == 0) {
            cls.localServers[index].clients = humanCount;
            localChanged = qtrue;
        }
    }
    if (localChanged != qfalse) {
        CL_UpdateServerRefreshCvars(cls.localServers, cls.numLocalServers, LAN_SERVER_SOURCE_LOCAL);
    }

    qboolean globalChanged = qfalse;
    int32_t lower = 0;
    int32_t upper = cls.numGlobalServers;
    while (lower < upper) {
        const int32_t middle = (lower + upper) / 2;
        const int32_t comparison = NET_CompareAdrSigned(&address, &cls.globalServers[middle].address);
        if (comparison < 0) {
            upper = middle;
        } else if (comparison > 0) {
            lower = middle + 1;
        } else {
            int32_t first = middle;
            while (first > 0 && NET_CompareAdrSigned(&address, &cls.globalServers[first - 1].address) == 0) {
                --first;
            }
            for (int32_t index = first; index < cls.numGlobalServers; ++index) {
                if (NET_CompareAdrSigned(&address, &cls.globalServers[index].address) != 0) {
                    break;
                }
                cls.globalServers[index].clients = humanCount;
                globalChanged = qtrue;
            }
            break;
        }
    }
    if (globalChanged != qfalse) {
        CL_UpdateServerRefreshCvars(cls.globalServers, cls.numGlobalServers, LAN_SERVER_SOURCE_GLOBAL);
    }

    qboolean favoriteChanged = qfalse;
    for (int32_t index = 0; index < LAN_FAVORITE_SERVER_CAPACITY; ++index) {
        if (NET_CompareAdrSigned(&address, &cls.favoriteServers[index].address) == 0) {
            cls.favoriteServers[index].clients = humanCount;
            favoriteChanged = qtrue;
        }
    }
    if (favoriteChanged != qfalse) {
        CL_UpdateServerRefreshCvars(cls.favoriteServers, cls.numFavoriteServers, LAN_SERVER_SOURCE_FAVORITES);
    }
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): count every well-formed
 * status roster entry, retaining the requested 999-ping bot heuristic by
 * server address. The caller supplies a copied message cursor so the stock
 * status-cache parser can consume the same packet independently. */
static void coduomp_record_server_player_status(netadr_t address, msg_t *message)
{
    qboolean tracked = qfalse;
    for (int32_t index = 0; index < coduomp_serverPlayerQueryCount; ++index) {
        const coduomp_server_player_query_t *const query = &coduomp_serverPlayerQueries[index];
        if (query->state == CODUOMP_SERVER_PLAYER_QUERY_PENDING && NET_CompareAdrSigned(&address, &query->address) == 0) {
            tracked = qtrue;
            break;
        }
    }
    if (tracked == qfalse)
        return;

    (void)MSG_ReadStringLine(message);
    uint8_t humanCount = 0;
    uint8_t botCount = 0;
    qboolean validRoster = qtrue;
    const char *player = MSG_ReadStringLine(message);
    while (player[0] != '\0') {
        int32_t score;
        int32_t ping;
        if (sscanf(player, "%d %d", &score, &ping) != 2) {
            validRoster = qfalse;
        } else if (ping == CODUOMP_SERVER_BOT_PING) {
            if (botCount < UINT8_MAX)
                ++botCount;
        } else if (humanCount < UINT8_MAX) {
            ++humanCount;
        }
        player = MSG_ReadStringLine(message);
    }

    for (int32_t index = 0; index < coduomp_serverPlayerQueryCount; ++index) {
        coduomp_server_player_query_t *const query = &coduomp_serverPlayerQueries[index];
        if (query->state == CODUOMP_SERVER_PLAYER_QUERY_PENDING && NET_CompareAdrSigned(&address, &query->address) == 0) {
            query->state = CODUOMP_SERVER_PLAYER_QUERY_COMPLETE;
        }
    }

    if (validRoster == qfalse)
        return;

    coduomp_server_bot_cache_entry_t *const botCache = coduomp_find_server_bot_cache(address, qtrue);
    if (botCache != NULL) {
        botCache->cachedBotCount = botCount;
        botCache->displayedBotCount = botCount;
    }
    coduomp_apply_server_human_count(address, humanCount);
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): maintain at most the stock
 * status-cache width of concurrent automatic queries. Completed and timed-out
 * records stay inert until the next browser refresh resets the generation. */
static qboolean coduomp_pump_server_player_queries(void)
{
    const int32_t now = (int32_t)Sys_Milliseconds();
    int32_t pendingCount = 0;
    qboolean active = qfalse;

    for (int32_t index = 0; index < coduomp_serverPlayerQueryCount; ++index) {
        coduomp_server_player_query_t *const query = &coduomp_serverPlayerQueries[index];
        if (query->state != CODUOMP_SERVER_PLAYER_QUERY_PENDING)
            continue;
        if ((uint32_t)now - (uint32_t)query->requestStartTime >= (uint32_t)query->timeoutMsec) {
            query->state = CODUOMP_SERVER_PLAYER_QUERY_COMPLETE;
            continue;
        }
        ++pendingCount;
        active = qtrue;
    }

    for (int32_t index = 0; index < coduomp_serverPlayerQueryCount && pendingCount < CL_SERVER_STATUS_SLOT_COUNT; ++index) {
        coduomp_server_player_query_t *const query = &coduomp_serverPlayerQueries[index];
        if (query->state != CODUOMP_SERVER_PLAYER_QUERY_QUEUED)
            continue;
        query->requestStartTime = now;
        query->state = CODUOMP_SERVER_PLAYER_QUERY_PENDING;
        NET_OutOfBandPrint(NS_CLIENT, query->address, "getstatus");
        ++pendingCount;
        active = qtrue;
    }

    return active;
}

/* Source: CoDUOMP.exe 0x004155f0..0x00415a3f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004155f0_00415a40.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * CL_ServerInfoPacket. A response first completes a matching pending ping.
 * An unsolicited response is useful only during local-server discovery: it
 * adds an address-only browser entry which a subsequent ping will populate. */
void CL_ServerInfoPacket(netadr_t address, msg_t *message, int32_t packetTime)
{
    const char *const info = MSG_ReadString(message);
    const int32_t packetProtocol = coduo_crt_atoi(Info_ValueForKey(info, "protocol"));

    const float expectedProtocol =
        Cvar_VariableString("debug_protocol")[0] == '\0' ? (float)CL_NETWORK_PROTOCOL_VERSION : Cvar_VariableValue("debug_protocol");

    if ((float)packetProtocol != expectedProtocol) {
        Com_DPrintf("Different protocol info packet: %s\n", info);
        return;
    }

    cl_ping_t *ping = NULL;
    for (int32_t index = 0; index < CL_MAX_PING_REQUESTS; ++index) {
        cl_ping_t *const candidate = &cl_pingList[index];
        if (candidate->address.port != 0 && candidate->pingMsec == 0 && NET_CompareAdrSigned(&candidate->address, &address) == 0) {
            ping = candidate;
            break;
        }
    }

    if (ping != NULL) {
        ping->pingMsec = packetTime - ping->requestStartTime + 1;
        Com_DPrintf("ping time %dms from %s\n", ping->pingMsec, NET_AdrToString(address));

        Q_strncpyz(ping->serverInfo, info, (int32_t)sizeof(ping->serverInfo));

        int32_t netType;
        switch (address.type) {
        case NA_BROADCAST:
        case NA_IP:
            netType = 1;
            break;
        case NA_IPX:
        case NA_BROADCAST_IPX:
            netType = 2;
            break;
        default:
            netType = 0;
            break;
        }
        Info_SetValueForKey(ping->serverInfo, "nettype", va("%d", netType));
        CL_SetServerInfoByAddress(address, info, ping->pingMsec);
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): getinfo has only an
         * aggregate client count. Ask nonempty servers for the roster needed
         * to remove 999-ping entries from the browser count. */
        if (coduo_crt_atoi(Info_ValueForKey(info, "clients")) > 0) {
            coduomp_queue_server_player_query(address, ping->pingMsec);
        }
        return;
    }

    if (cls.pingUpdateSource != LAN_SERVER_SOURCE_LOCAL)
        return;

    int32_t serverIndex;
    for (serverIndex = 0; serverIndex < LAN_LOCAL_SERVER_CAPACITY; ++serverIndex) {
        lan_server_info_t *const server = &cls.localServers[serverIndex];
        if (server->address.port == 0)
            break;
        if (NET_CompareAdrSigned(&server->address, &address) == 0) {
            return;
        }
    }

    if (serverIndex == LAN_LOCAL_SERVER_CAPACITY) {
        Com_DPrintf("MAX_OTHER_SERVERS hit, dropping infoResponse\n");
        return;
    }

    cls.numLocalServers = serverIndex + 1;
    lan_server_info_t *const server = &cls.localServers[serverIndex];
    server->address = address;
    server->clients = 0;
    server->hostName[0] = '\0';
    server->mapName[0] = '\0';
    server->maxClients = 0;
    server->maxPing = 0;
    server->minPing = 0;
    server->ping = -1;
    server->game[0] = '\0';
    server->gameType[0] = '\0';
    server->netType = (uint8_t)address.type;
    server->allowAnonymous = 0;
    server->punkBuster = 0;

    char printMessage[LAN_SERVER_PRINT_MESSAGE_CAPACITY];
    Q_strncpyz(printMessage, MSG_ReadString(message), (int32_t)sizeof(printMessage));
    if (printMessage[0] == '\0')
        return;

    const size_t messageLength = strlen(printMessage);
    if (printMessage[messageLength - 1] != '\n') {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        printMessage[messageLength] = '\n';
        printMessage[messageLength + 1] = '\0';
    }

    Com_Printf("%s: %s", NET_AdrToString(address), printMessage);
}

/* Source: CoDUOMP.exe 0x0041a4e0..0x0041a5a3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a4e0_0041a5a4.mcode.
 * Name and file-handle argument: exact same-module Mac symbol
 * LAN_LoadCachedServersInternal. Every read must return its complete byte
 * count. The original unsigned bounds reject a cache containing exactly the
 * nominal array capacity as well as corrupt negative counts. */
qboolean LAN_LoadCachedServersInternal(int32_t fileHandle)
{
    int32_t cacheVersion;
    int32_t serializedArrayBytes;

    if (FS_Read(&cacheVersion, (int32_t)sizeof(cacheVersion), fileHandle) != (int32_t)sizeof(cacheVersion) ||
        cacheVersion != LAN_SERVER_CACHE_VERSION) {
        return qfalse;
    }

    if (FS_Read(&cls.numGlobalServers, (int32_t)sizeof(cls.numGlobalServers), fileHandle) != (int32_t)sizeof(cls.numGlobalServers) ||
        (uint32_t)cls.numGlobalServers >= LAN_GLOBAL_SERVER_CAPACITY) {
        return qfalse;
    }

    if (FS_Read(&cls.numFavoriteServers, (int32_t)sizeof(cls.numFavoriteServers), fileHandle) != (int32_t)sizeof(cls.numFavoriteServers) ||
        (uint32_t)cls.numFavoriteServers >= LAN_FAVORITE_SERVER_CAPACITY) {
        return qfalse;
    }

    if (FS_Read(&serializedArrayBytes, (int32_t)sizeof(serializedArrayBytes), fileHandle) != (int32_t)sizeof(serializedArrayBytes) ||
        serializedArrayBytes != lan_serverCacheArrayBytes) {
        return qfalse;
    }

    if (FS_Read(cls.globalServers, (int32_t)sizeof(cls.globalServers), fileHandle) != (int32_t)sizeof(cls.globalServers) ||
        FS_Read(cls.favoriteServers, (int32_t)sizeof(cls.favoriteServers), fileHandle) != (int32_t)sizeof(cls.favoriteServers)) {
        return qfalse;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (size_t serverIndex = 0; serverIndex < LAN_GLOBAL_SERVER_CAPACITY; ++serverIndex) {
        const lan_server_info_t *const server = &cls.globalServers[serverIndex];
        if (memchr(server->hostName, '\0', sizeof(server->hostName)) == NULL ||
            memchr(server->mapName, '\0', sizeof(server->mapName)) == NULL || memchr(server->game, '\0', sizeof(server->game)) == NULL ||
            memchr(server->gameType, '\0', sizeof(server->gameType)) == NULL) {
            return qfalse;
        }
    }
    for (size_t serverIndex = 0; serverIndex < LAN_FAVORITE_SERVER_CAPACITY; ++serverIndex) {
        const lan_server_info_t *const server = &cls.favoriteServers[serverIndex];
        if (memchr(server->hostName, '\0', sizeof(server->hostName)) == NULL ||
            memchr(server->mapName, '\0', sizeof(server->mapName)) == NULL || memchr(server->game, '\0', sizeof(server->game)) == NULL ||
            memchr(server->gameType, '\0', sizeof(server->gameType)) == NULL) {
            return qfalse;
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0041a5b0..0x0041a617.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a5b0_0041a618.mcode.
 * Name: exact same-module Mac symbol LAN_LoadCachedServers. A failed open or
 * malformed/truncated cache discards both active counts but deliberately does
 * not clear the backing arrays. */
void LAN_LoadCachedServers(void)
{
    int32_t fileHandle;

    if (FS_SV_FOpenFileRead(lan_serverCacheFileName, &fileHandle) == 0) {
        cls.numGlobalServers = 0;
        cls.numFavoriteServers = 0;
        return;
    }

    const qboolean loaded = LAN_LoadCachedServersInternal(fileHandle);
    FS_FCloseFile(fileHandle);
    if (loaded == qfalse) {
        cls.numGlobalServers = 0;
        cls.numFavoriteServers = 0;
        return;
    }

    coduo_crt_qsort(cls.globalServers, (size_t)cls.numGlobalServers, sizeof(cls.globalServers[0]), CL_CompareAdrSigned);
}

/* Source: CoDUOMP.exe 0x0041a620..0x0041a73a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a620_0041a73b.mcode.
 * Name: exact same-module Mac symbol LAN_SaveServersToCache. Before writing,
 * stale master responses and adjacent duplicate addresses are removed by
 * replacing each rejected slot with the current last record. The resulting
 * active prefix is then restored to address order. */
void LAN_SaveServersToCache(void)
{
    const int32_t fileHandle = FS_SV_FOpenFileWrite(lan_serverCacheFileName);
    int32_t cacheVersion = LAN_SERVER_CACHE_VERSION;
    int32_t serializedArrayBytes = lan_serverCacheArrayBytes;

    (void)FS_Write(&cacheVersion, (int32_t)sizeof(cacheVersion), fileHandle);

    for (int32_t serverIndex = cls.numGlobalServers - 1; serverIndex >= 0; --serverIndex) {
        lan_server_info_t *server = &cls.globalServers[serverIndex];
        const qboolean stale = server->masterResponseAge >= LAN_SERVER_RECENT_MASTER_RESPONSE_AGE_LIMIT ? qtrue : qfalse;
        const qboolean duplicate =
            serverIndex > 0 && NET_CompareAdrSigned(&server->address, &cls.globalServers[serverIndex - 1].address) == 0 ? qtrue : qfalse;

        if (stale != qfalse || duplicate != qfalse) {
            --cls.numGlobalServers;
            *server = cls.globalServers[cls.numGlobalServers];
        }
    }

    coduo_crt_qsort(cls.globalServers, (size_t)cls.numGlobalServers, sizeof(cls.globalServers[0]), CL_CompareAdrSigned);
    (void)FS_Write(&cls.numGlobalServers, (int32_t)sizeof(cls.numGlobalServers), fileHandle);
    (void)FS_Write(&cls.numFavoriteServers, (int32_t)sizeof(cls.numFavoriteServers), fileHandle);
    (void)FS_Write(&serializedArrayBytes, (int32_t)sizeof(serializedArrayBytes), fileHandle);
    (void)FS_Write(cls.globalServers, (int32_t)sizeof(cls.globalServers), fileHandle);
    (void)FS_Write(cls.favoriteServers, (int32_t)sizeof(cls.favoriteServers), fileHandle);
    FS_FCloseFile(fileHandle);
}

/* Source: CoDUOMP.exe 0x0041aa90..0x0041aaaf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041aa90_0041aab0.mcode.
 * Name: exact same-module Mac symbol LAN_GetServerCount. */
int32_t LAN_GetServerCount(lan_server_source_t source)
{
    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        return cls.numLocalServers;
    case LAN_SERVER_SOURCE_GLOBAL:
        return cls.numGlobalServers;
    case LAN_SERVER_SOURCE_FAVORITES:
        return cls.numFavoriteServers;
    default:
        return 0;
    }
}

/* Source: CoDUOMP.exe 0x0041aab0..0x0041aabb.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041aab0_0041aabc.mcode.
 * Name: exact same-module Mac symbol LAN_WaitServerResponse. Only the global
 * master-server source has an asynchronous response latch; local and favorite
 * sources return false. */
qboolean LAN_WaitServerResponse(lan_server_source_t source)
{
    if (source != LAN_SERVER_SOURCE_GLOBAL)
        return qfalse;
    return cls.waitingForMasterResponse;
}

/* Source: CoDUOMP.exe 0x00416810..0x00416851.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416810_00416852.mcode.
 * Name and 16-slot extent: exact same-module Mac symbol
 * CL_GetPingQueueCount. A pending slot is identified solely by its nonzero
 * network-address port. */
int32_t CL_GetPingQueueCount(void)
{
    int32_t count = 0;
    for (int32_t index = 0; index < CL_MAX_PING_REQUESTS; ++index) {
        if (cl_pingList[index].address.port != 0)
            ++count;
    }
    return count;
}

/* Source: CoDUOMP.exe 0x004167f0..0x00416808.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004167f0_00416809.mcode.
 * Name and 16-slot bounds: exact same-module Mac symbol CL_ClearPing. The
 * address port alone is the pending-slot occupancy marker. */
void CL_ClearPing(int32_t pingIndex)
{
    if (pingIndex >= 0 && pingIndex < CL_MAX_PING_REQUESTS)
        cl_pingList[pingIndex].address.port = 0;
}

/* Source: CoDUOMP.exe 0x004167b0..0x004167e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004167b0_004167e2.mcode.
 * Name and output behavior: exact same-module Mac symbol CL_GetPingInfo. */
void CL_GetPingInfo(int32_t pingIndex, char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (buffer == NULL || bufferSize <= 0)
        return;
    buffer[0] = '\0';
    if ((uint32_t)pingIndex >= CL_MAX_PING_REQUESTS)
        return;

    const cl_ping_t *ping = &cl_pingList[pingIndex];
    if (ping->address.port == 0)
        return;

    const int32_t copySize = bufferSize < CL_PING_INFO_SIZE ? bufferSize : CL_PING_INFO_SIZE;
    strncpy(buffer, ping->serverInfo, (size_t)copySize - 1u);
    buffer[copySize - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x00416860..0x00416944.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416860_00416945.mcode.
 * Name and return role: exact same-module Mac symbol CL_GetFreePing. Empty or
 * 500-ms-old slots are reused immediately and have their address port cleared;
 * if every slot is younger, the request with the greatest age is returned
 * without a preliminary clear. */
cl_ping_t *CL_GetFreePing(void)
{
    enum {
        CL_PING_SLOT_TIMEOUT_MSEC = 500
    };
    const int32_t now = (int32_t)Sys_Milliseconds();

    for (int32_t index = 0; index < CL_MAX_PING_REQUESTS; ++index) {
        cl_ping_t *ping = &cl_pingList[index];
        if (ping->address.port == 0) {
            ping->address.port = 0;
            return ping;
        }
        int32_t age = ping->pingMsec;
        if (age == 0)
            age = now - ping->requestStartTime;
        if (age >= CL_PING_SLOT_TIMEOUT_MSEC) {
            ping->address.port = 0;
            return ping;
        }
    }

    cl_ping_t *oldestPing = &cl_pingList[0];
    int32_t oldestAge = INT32_MIN;
    for (int32_t index = 0; index < CL_MAX_PING_REQUESTS; ++index) {
        const int32_t age = now - cl_pingList[index].requestStartTime;
        if (age > oldestAge) {
            oldestAge = age;
            oldestPing = &cl_pingList[index];
        }
    }

    return oldestPing;
}

/* Source: CoDUOMP.exe 0x00416950..0x00416a80.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416950_00416a81.mcode.
 * Name and command contract: exact same-module Mac symbol CL_Ping_f. The
 * original sends the same connectionless getinfo challenge used by browser
 * refreshes and leaves the slot's old info bytes untouched until a response
 * replaces them. */
void CL_Ping_f(void)
{
    if (Cmd_Argc() != 2) {
        Com_Printf("usage: ping [server]\n");
        return;
    }

    netadr_t address = {0};
    if (NET_StringToAdr(Cmd_Argv(1), &address) == qfalse)
        return;

    cl_ping_t *ping = CL_GetFreePing();
    ping->address = address;
    ping->requestStartTime = (int32_t)Sys_Milliseconds();
    ping->pingMsec = 0;
    CL_SetServerInfoByAddress(ping->address, NULL, 0);
    NET_OutOfBandPrint(NS_CLIENT, ping->address, "getinfo xxx");
}

/* Source: CoDUOMP.exe 0x00416640..0x00416751.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416640_00416752.mcode.
 * Name and argument roles: exact same-module Mac symbol CL_GetPing. An
 * unfinished request reports a provisional elapsed time only after it reaches
 * cl_maxPing (with the original 100 ms floor); server-list metadata is updated
 * with the stored completed time, not that provisional value. */
void CL_GetPing(int32_t pingIndex, char *address, int32_t addressSize, int32_t *pingTime)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (address != NULL && addressSize > 0)
        address[0] = '\0';
    if (pingTime != NULL)
        *pingTime = 0;
    if (address == NULL || addressSize <= 0 || pingTime == NULL || (uint32_t)pingIndex >= CL_MAX_PING_REQUESTS) {
        return;
    }

    cl_ping_t *ping = &cl_pingList[pingIndex];

    if (ping->address.port == 0)
        return;

    const int32_t copySize = addressSize < NET_ADDRESS_STRING_SIZE ? addressSize : NET_ADDRESS_STRING_SIZE;
    strncpy(address, NET_AdrToString(ping->address), (size_t)copySize - 1u);
    address[copySize - 1] = '\0';

    int32_t reportedTime = ping->pingMsec;
    if (reportedTime == 0) {
        reportedTime = (int32_t)(Sys_Milliseconds() - (uint32_t)ping->requestStartTime);
        const cvar_t *maxPing = Cvar_FindVar("cl_maxPing");
        int32_t reportThreshold = 100;
        if (maxPing != NULL && maxPing->integer >= reportThreshold)
            reportThreshold = maxPing->integer;
        if (reportedTime < reportThreshold)
            reportedTime = 0;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const char *responseInfo = ping->pingMsec != 0 ? ping->serverInfo : NULL;
    CL_SetServerInfoByAddress(ping->address, responseInfo, ping->pingMsec);
    *pingTime = reportedTime;
}

/* Source: CoDUOMP.exe 0x00416a90..0x00416d81.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416a90_00416d82.mcode.
 * Name and source selector: exact same-module Mac symbol
 * CL_UpdateDirtyPings. Dirty servers with ping == -1 receive at most one
 * outstanding getinfo request each. Completed or timed-out requests are
 * harvested through CL_GetPing and released by clearing their address port. */
qboolean CL_UpdateDirtyPings(lan_server_source_t source)
{
    if (cls.state != CA_DISCONNECTED || source < LAN_SERVER_SOURCE_LOCAL || source > LAN_SERVER_SOURCE_FAVORITES) {
        return qfalse;
    }

    cls.pingUpdateSource = source;
    int32_t pendingCount = CL_GetPingQueueCount();
    qboolean stillUpdating = qfalse;

    if (pendingCount < CL_MAX_PING_REQUESTS) {
        lan_server_info_t *servers;
        int32_t serverCount;

        switch (source) {
        case LAN_SERVER_SOURCE_LOCAL:
            servers = cls.localServers;
            serverCount = cls.numLocalServers;
            break;
        case LAN_SERVER_SOURCE_GLOBAL:
            servers = cls.globalServers;
            serverCount = cls.numGlobalServers;
            break;
        case LAN_SERVER_SOURCE_FAVORITES:
            servers = cls.favoriteServers;
            serverCount = cls.numFavoriteServers;
            break;
        }

        for (int32_t serverIndex = 0; serverIndex < serverCount; ++serverIndex) {
            lan_server_info_t *server = &servers[serverIndex];
            if (server->dirty == 0 || server->ping != -1)
                continue;
            if (pendingCount >= CL_MAX_PING_REQUESTS)
                break;

            int32_t pingIndex;
            for (pingIndex = 0; pingIndex < CL_MAX_PING_REQUESTS; ++pingIndex) {
                if (cl_pingList[pingIndex].address.port != 0 &&
                    NET_CompareAdrSigned(&server->address, &cl_pingList[pingIndex].address) == 0) {
                    break;
                }
            }
            if (pingIndex < CL_MAX_PING_REQUESTS)
                continue;

            stillUpdating = qtrue;
            for (pingIndex = 0; pingIndex < CL_MAX_PING_REQUESTS; ++pingIndex) {
                if (cl_pingList[pingIndex].address.port == 0)
                    break;
            }

            cl_ping_t *ping = &cl_pingList[pingIndex];
            ping->address = server->address;
            ping->requestStartTime = (int32_t)Sys_Milliseconds();
            ping->pingMsec = 0;
            NET_OutOfBandPrint(NS_CLIENT, ping->address, "getinfo xxx");
            ++pendingCount;
        }
    }

    if (pendingCount != 0)
        stillUpdating = qtrue;

    for (int32_t pingIndex = 0; pingIndex < CL_MAX_PING_REQUESTS; ++pingIndex) {
        if (cl_pingList[pingIndex].address.port == 0)
            continue;

        char address[CL_PING_INFO_SIZE];
        int32_t pingTime;
        CL_GetPing(pingIndex, address, (int32_t)sizeof(address), &pingTime);
        if (pingTime != 0) {
            cl_pingList[pingIndex].address.port = 0;
            stillUpdating = qtrue;
        }
    }

    if (coduomp_pump_server_player_queries() != qfalse)
        stillUpdating = qtrue;

    return stillUpdating;
}

/* Source: CoDUOMP.exe 0x00415bf0..0x00415d4c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00415bf0_00415d4d.mcode.
 * Name and return role: exact same-module Mac symbol CL_GetServerStatus. An
 * address match wins first, then a retrieved slot, then the oldest outstanding
 * request. The round-robin fallback is retained although the fixed non-empty
 * slot array normally makes the oldest-slot selection conclusive. */
cl_server_status_t *CL_GetServerStatus(netadr_t address)
{
    for (int32_t index = 0; index < CL_SERVER_STATUS_SLOT_COUNT; ++index) {
        if (NET_CompareAdrSigned(&address, &cl_serverStatusList[index].address) == 0) {
            return &cl_serverStatusList[index];
        }
    }

    for (int32_t index = 0; index < CL_SERVER_STATUS_SLOT_COUNT; ++index) {
        if (cl_serverStatusList[index].responseRetrieved != qfalse)
            return &cl_serverStatusList[index];
    }

    int32_t oldestIndex = -1;
    int32_t oldestStartTime = 0;
    for (int32_t index = 0; index < CL_SERVER_STATUS_SLOT_COUNT; ++index) {
        if (oldestIndex == -1 || cl_serverStatusList[index].requestStartTime < oldestStartTime) {
            oldestIndex = index;
            oldestStartTime = cl_serverStatusList[index].requestStartTime;
        }
    }

    if (oldestIndex == -1) {
        ++cl_serverStatusNextSlot;
        oldestIndex = cl_serverStatusNextSlot & (CL_SERVER_STATUS_SLOT_COUNT - 1);
    }
    return &cl_serverStatusList[oldestIndex];
}

/* Source: CoDUOMP.exe 0x00415d50..0x00415f82.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00415d50_00415f83.mcode.
 * Name and arguments: exact same-module Mac symbol CL_ServerStatus. Passing a
 * null address resets the cache; passing a null destination releases the
 * selected record. A completed response is copied once, while pending requests
 * are retransmitted only after cl_serverStatusResendTime expires. */
qboolean CL_ServerStatus(const char *address, char *status, int32_t statusSize)
{
    if (address == NULL) {
        for (int32_t index = 0; index < CL_SERVER_STATUS_SLOT_COUNT; ++index) {
            cl_serverStatusList[index].address.port = 0;
            cl_serverStatusList[index].responseRetrieved = qtrue;
        }
        return qfalse;
    }

    netadr_t serverAddress;
    if (NET_StringToAdr(address, &serverAddress) == qfalse)
        return qfalse;

    cl_server_status_t *serverStatus = CL_GetServerStatus(serverAddress);
    if (status == NULL) {
        serverStatus->responseRetrieved = qtrue;
        return qfalse;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (statusSize <= 0)
        return qfalse;

    if (NET_CompareAdrSigned(&serverAddress, &serverStatus->address) == 0) {
        if (serverStatus->requestPending == qfalse) {
            const int32_t copySize = statusSize < CL_SERVER_STATUS_TEXT_SIZE ? statusSize : CL_SERVER_STATUS_TEXT_SIZE;
            strncpy(status, serverStatus->responseText, (size_t)copySize - 1u);
            status[copySize - 1] = '\0';
            serverStatus->responseRetrieved = qtrue;
            serverStatus->requestStartTime = 0;
            return qtrue;
        }

        const int32_t now = (int32_t)Sys_Milliseconds();
        if (serverStatus->requestStartTime >= now - cl_serverStatusResendTime->integer) {
            return qfalse;
        }

        serverStatus->printResponse = qfalse;
        serverStatus->requestPending = qtrue;
        serverStatus->responseRetrieved = qfalse;
        serverStatus->responseReceivedTime = 0;
        serverStatus->requestStartTime = (int32_t)Sys_Milliseconds();
    } else {
        if (serverStatus->responseRetrieved == qfalse)
            return qfalse;

        serverStatus->address = serverAddress;
        serverStatus->printResponse = qfalse;
        serverStatus->requestPending = qtrue;
        serverStatus->responseRetrieved = qfalse;
        const int32_t now = (int32_t)Sys_Milliseconds();
        serverStatus->responseReceivedTime = 0;
        serverStatus->requestStartTime = now;
    }

    NET_OutOfBandPrint(NS_CLIENT, serverAddress, "getstatus");
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00415f90..0x004162d7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00415f90_004162d8.mcode.
 * Name and arguments: exact same-module Mac symbol CL_ServerStatusResponse.
 * The cached form begins with the original 0x15 control marker, followed by
 * the sanitized settings line, backslash-delimited player lines, and a final
 * backslash. Console-query slots also print settings as aligned key/value
 * pairs and players as number/score/ping/name. */
void CL_ServerStatusResponse(netadr_t address, msg_t *message)
{
    msg_t playerCountMessage = *message;
    coduomp_record_server_player_status(address, &playerCountMessage);

    cl_server_status_t *serverStatus = NULL;
    for (int32_t index = 0; index < CL_SERVER_STATUS_SLOT_COUNT; ++index) {
        if (NET_CompareAdrSigned(&address, &cl_serverStatusList[index].address) == 0) {
            serverStatus = &cl_serverStatusList[index];
            break;
        }
    }
    if (serverStatus == NULL)
        return;

    const char *settings = MSG_ReadStringLine(message);
    Com_sprintf(serverStatus->responseText, CL_SERVER_STATUS_TEXT_SIZE, "\x15%s", settings);

    if (serverStatus->printResponse != qfalse) {
        Com_Printf("Server settings:\n");
        const char *cursor = settings;
        while (*cursor != '\0') {
            for (int32_t column = 0; column < 2; ++column) {
                char token[MAX_TOKEN_CHARS];
                int32_t length = 0;

                if (*cursor == '\\')
                    ++cursor;
                while (*cursor != '\0' && *cursor != '\\' && length < (int32_t)sizeof(token) - 1) {
                    token[length++] = *cursor;
                    if (length >= (int32_t)sizeof(token) - 1)
                        break;
                    ++cursor;
                }
                token[length] = '\0';

                if (column == 0)
                    Com_Printf("%-24s", token);
                else
                    Com_Printf("%s\n", token);
            }
        }
    }

    size_t textLength = strlen(serverStatus->responseText);
    Com_sprintf(serverStatus->responseText + textLength, CL_SERVER_STATUS_TEXT_SIZE - (int32_t)textLength, "\\");

    if (serverStatus->printResponse != qfalse) {
        Com_Printf("\nPlayers:\n");
        Com_Printf("num: score: ping: name:\n");
    }

    int32_t playerNumber = 0;
    const char *player = MSG_ReadStringLine(message);
    while (*player != '\0') {
        textLength = strlen(serverStatus->responseText);
        Com_sprintf(serverStatus->responseText + textLength, CL_SERVER_STATUS_TEXT_SIZE - (int32_t)textLength, "\\%s", player);

        if (serverStatus->printResponse != qfalse) {
            int32_t score = 0;
            int32_t ping = 0;
            (void)sscanf(player, "%d %d", &score, &ping);

            const char *name = strchr(player, ' ');
            if (name != NULL) {
                name = strchr(name + 1, ' ');
                if (name != NULL)
                    ++name;
            }
            if (name == NULL)
                name = "unknown";

            Com_Printf("%-2d    %-3d    %-3d    %s\n", playerNumber, score, ping, name);
        }

        player = MSG_ReadStringLine(message);
        ++playerNumber;
    }

    textLength = strlen(serverStatus->responseText);
    Com_sprintf(serverStatus->responseText + textLength, CL_SERVER_STATUS_TEXT_SIZE - (int32_t)textLength, "\\");
    serverStatus->responseReceivedTime = (int32_t)Sys_Milliseconds();
    serverStatus->address = address;
    serverStatus->requestPending = qfalse;
    if (serverStatus->printResponse != qfalse)
        serverStatus->responseRetrieved = qtrue;
}

/* Source: CoDUOMP.exe 0x00416d90..0x00416eb8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416d90_00416eb9.mcode.
 * Name and command behavior: exact same-module Mac symbol CL_ServerStatus_f.
 * With no explicit argument, the command targets the active non-demo server;
 * its response is marked for immediate console formatting. */
void CL_ServerStatus_f(void)
{
    const char *serverName;

    if (Cmd_Argc() == 2) {
        serverName = Cmd_Argv(1);
    } else if (cls.state == CA_ACTIVE && clc.demoPlayback == qfalse) {
        serverName = cls.serverName;
    } else {
        Com_Printf("Not connected to a server.\n");
        Com_Printf("Usage: serverstatus [server]\n");
        return;
    }

    netadr_t serverAddress = {0};
    if (NET_StringToAdr(serverName, &serverAddress) == qfalse)
        return;

    NET_OutOfBandPrint(NS_CLIENT, serverAddress, "getstatus");
    cl_server_status_t *serverStatus = CL_GetServerStatus(serverAddress);
    serverStatus->address = serverAddress;
    serverStatus->printResponse = qtrue;
    serverStatus->requestPending = qtrue;
}

/* Source: CoDUOMP.exe 0x004162e0..0x00416450.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004162e0_00416451.mcode.
 * Name and command behavior: exact same-module Mac symbol CL_LocalServers_f.
 * The two four-port broadcast passes cover 28960..28963. Resetting each local
 * record deliberately preserves its dirty byte, ping, and trailing strings. */
void CL_LocalServers_f(void)
{
    enum {
        LAN_LOCAL_SCAN_FIRST_PORT = 28960,
        LAN_LOCAL_SCAN_PORT_COUNT = 4,
        LAN_LOCAL_SCAN_PASSES = 2
    };
    /* Original .rdata at 0x0059311c is four 0xff marker bytes followed by
     * "getinfo xxx" and its NUL. CL_LocalServers_f measures and sends the
     * first 15 bytes, excluding only that terminator. */
    static const struct cl_local_info_packet_s {
        int32_t oobMarker;
        char command[sizeof("getinfo xxx")];
    } localInfoPacket = {-1, "getinfo xxx"};

    _Static_assert(offsetof(struct cl_local_info_packet_s, command) == 4, "local-info OOB command must follow its four-byte marker");
    _Static_assert(sizeof(struct cl_local_info_packet_s) == 16, "local-info OOB packet storage extent changed");
    _Static_assert(sizeof(localInfoPacket) - 1 == 15, "local-info OOB transmitted extent changed");

    Com_Printf("Scanning for servers on the local network...\n");
    cls.numLocalServers = 0;
    cls.pingUpdateSource = LAN_SERVER_SOURCE_LOCAL;
    cls.numGlobalServerAddresses = 0;

    for (int32_t index = 0; index < LAN_LOCAL_SERVER_CAPACITY; ++index) {
        const uint8_t dirty = cls.localServers[index].dirty;
        memset(&cls.localServers[index], 0, offsetof(lan_server_info_t, ping));
        cls.localServers[index].dirty = dirty;
    }

    netadr_t broadcastAddress = {0};
    broadcastAddress.type = NA_BROADCAST;

    for (int32_t pass = 0; pass < LAN_LOCAL_SCAN_PASSES; ++pass) {
        for (int32_t portOffset = 0; portOffset < LAN_LOCAL_SCAN_PORT_COUNT; ++portOffset) {
            broadcastAddress.port = (uint16_t)BigShort((int16_t)(LAN_LOCAL_SCAN_FIRST_PORT + portOffset));
            CL_Netchan_SendOOBPacket(broadcastAddress, &localInfoPacket, (int32_t)(sizeof(localInfoPacket) - 1));
        }
    }
}

/* Source: CoDUOMP.exe 0x00416460..0x0041663d.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00416460_0041663e.mcode.
 * Name and command behavior: exact same-module Mac symbol
 * CL_GlobalServers_f. The Windows Ghidra export omitted this whole function;
 * direct PE disassembly proves its boundaries between adjacent INT3 padding.
 * The master index argument remains part of the command syntax but this build
 * always resolves the single UO master hostname. */
void CL_GlobalServers_f(void)
{
    enum {
        LAN_MASTER_PORT = 20610,
        LAN_MASTER_QUERY_SIZE = 1024
    };

    if (Cmd_Argc() < 3) {
        Com_Printf("usage: globalservers <master# 0-1> <protocol> [keywords]\n");
        return;
    }

    for (int32_t index = 0; index < cls.numGlobalServers; ++index) {
        ++cls.globalServers[index].masterResponseAge;
        if (cls.globalServers[index].masterResponseAge == 0)
            cls.globalServers[index].masterResponseAge = UINT8_MAX;
        cls.globalServers[index].clients = 0;
    }

    Com_Printf("Requesting servers from the master...\n");

    netadr_t masterAddress;
    (void)NET_StringToAdr("coduomaster.activision.com", &masterAddress);
    cls.waitingForMasterResponse = qtrue;
    cls.pingUpdateSource = LAN_SERVER_SOURCE_GLOBAL;
    cls.numGlobalServerAddresses = 0;
    masterAddress.port = (uint16_t)BigShort(LAN_MASTER_PORT);

    char query[LAN_MASTER_QUERY_SIZE];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    int32_t queryLength = snprintf(query, sizeof(query), "getservers %s", Cmd_Argv(2));
    if (queryLength < 0 || (size_t)queryLength >= sizeof(query)) {
        Com_Printf("globalservers: query exceeds %i bytes\n", LAN_MASTER_QUERY_SIZE - 1);
        return;
    }

    for (int32_t argument = 3; argument < Cmd_Argc(); ++argument) {
        const size_t remaining = sizeof(query) - (size_t)queryLength;
        const int32_t appended = snprintf(query + queryLength, remaining, " %s", Cmd_Argv(argument));
        if (appended < 0 || (size_t)appended >= remaining) {
            Com_Printf("globalservers: query exceeds %i bytes\n", LAN_MASTER_QUERY_SIZE - 1);
            return;
        }
        queryLength += appended;
    }

    const cvar_t *restrictFilesystem = Cvar_FindVar("fs_restrict");
    /* 0x004165d2..0x004165f2 appends this filter only when the x87
     * comparison proves the cvar value differs from zero. */
    if (restrictFilesystem != NULL && restrictFilesystem->value != 0.0f) {
        const size_t remaining = sizeof(query) - (size_t)queryLength;
        const int32_t appended = snprintf(query + queryLength, remaining, " demo");
        if (appended < 0 || (size_t)appended >= remaining) {
            Com_Printf("globalservers: query exceeds %i bytes\n", LAN_MASTER_QUERY_SIZE - 1);
            return;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    NET_OutOfBandPrint(NS_SERVER, masterAddress, "%s", query);
}

/* Source: CoDUOMP.exe 0x0041b260..0x0041b264.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041b260_0041b265.mcode.
 * Name: exact same-module Mac symbol LAN_GetPingQueueCount. */
int32_t LAN_GetPingQueueCount(void)
{
    return CL_GetPingQueueCount();
}

/* Source: CoDUOMP.exe 0x0041b270..0x0041b288.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b270_0041b289.mcode.
 * Name and bounds: exact same-module Mac symbol LAN_ClearPing. */
void LAN_ClearPing(int32_t pingIndex)
{
    CL_ClearPing(pingIndex);
}

/* Source: CoDUOMP.exe 0x0041b290..0x0041b2a2.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041b290_0041b2a3.mcode.
 * Name: exact same-module Mac symbol LAN_GetPing. */
void LAN_GetPing(int32_t pingIndex, char *address, int32_t addressSize, int32_t *pingTime)
{
    CL_GetPing(pingIndex, address, addressSize, pingTime);
}

/* Source: CoDUOMP.exe 0x0041b2b0..0x0041b2e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b2b0_0041b2e2.mcode.
 * Name and output behavior: exact same-module Mac symbol LAN_GetPingInfo. */
void LAN_GetPingInfo(int32_t pingIndex, char *buffer, int32_t bufferSize)
{
    CL_GetPingInfo(pingIndex, buffer, bufferSize);
}

/* Source: CoDUOMP.exe 0x0041b410..0x0041b414.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041b410_0041b415.mcode.
 * Name: exact same-module Mac symbol LAN_UpdateDirtyPings. */
qboolean LAN_UpdateDirtyPings(lan_server_source_t source)
{
    return CL_UpdateDirtyPings(source);
}

/* Source: CoDUOMP.exe 0x0041b420..0x0041b42b.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041b420_0041b42c.mcode.
 * Name and argument roles: exact same-module Mac symbol LAN_GetServerStatus. */
qboolean LAN_GetServerStatus(const char *address, char *status, int32_t statusSize)
{
    return CL_ServerStatus(address, status, statusSize);
}

/* Source: CoDUOMP.exe 0x0041a740..0x0041a77d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a740_0041a77e.mcode.
 * Name: exact same-module Mac symbol LAN_ResetPings. */
void LAN_ResetPings(lan_server_source_t source)
{
    lan_server_info_t *servers;
    int32_t serverCount;

    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        servers = cls.localServers;
        serverCount = LAN_LOCAL_SERVER_CAPACITY;
        break;
    case LAN_SERVER_SOURCE_GLOBAL:
        servers = cls.globalServers;
        serverCount = cls.numGlobalServers;
        break;
    case LAN_SERVER_SOURCE_FAVORITES:
        servers = cls.favoriteServers;
        serverCount = LAN_FAVORITE_SERVER_CAPACITY;
        break;
    default:
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (int32_t pingIndex = 0; pingIndex < CL_MAX_PING_REQUESTS; ++pingIndex) {
        cl_pingList[pingIndex].address.port = 0;
    }

    coduomp_reset_server_player_queries();

    for (int32_t index = 0; index < serverCount; ++index)
        servers[index].ping = -1;
}

/* Source: CoDUOMP.exe 0x0041a780..0x0041a941.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a780_0041a942.mcode.
 * Name, signature, and return meanings: exact same-module Mac symbol
 * LAN_AddServer and UI command 88. Only address, hostname, and dirty are
 * written in the newly exposed slot; the original does not reinitialize the
 * rest of a slot that may previously have been beyond the active count. */
int32_t LAN_AddServer(lan_server_source_t source, const char *name, const char *address)
{
    lan_server_info_t *servers;
    int32_t *serverCount;
    int32_t capacity;
    netadr_t parsedAddress;

    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        servers = cls.localServers;
        serverCount = &cls.numLocalServers;
        capacity = LAN_LOCAL_SERVER_CAPACITY;
        break;
    case LAN_SERVER_SOURCE_GLOBAL:
        servers = cls.globalServers;
        serverCount = &cls.numGlobalServers;
        capacity = LAN_GLOBAL_SERVER_CAPACITY;
        break;
    case LAN_SERVER_SOURCE_FAVORITES:
        servers = cls.favoriteServers;
        serverCount = &cls.numFavoriteServers;
        capacity = LAN_FAVORITE_SERVER_CAPACITY;
        break;
    default:
        return LAN_ADD_SERVER_LIST_FULL;
    }

    if (*serverCount >= capacity)
        return LAN_ADD_SERVER_LIST_FULL;
    if (NET_StringToAdr(address, &parsedAddress) == qfalse)
        return LAN_ADD_SERVER_BAD_ADDRESS;

    for (int32_t index = 0; index < *serverCount; ++index) {
        if (NET_CompareAdrSigned(&parsedAddress, &servers[index].address) == 0) {
            return LAN_ADD_SERVER_DUPLICATE;
        }
    }

    lan_server_info_t *server = &servers[*serverCount];
    server->address = parsedAddress;
    strncpy(server->hostName, name, sizeof(server->hostName) - 1);
    server->hostName[sizeof(server->hostName) - 1] = '\0';
    server->dirty = 1;
    ++*serverCount;

    if (source == LAN_SERVER_SOURCE_GLOBAL) {
        coduo_crt_qsort(cls.globalServers, (size_t)cls.numGlobalServers, sizeof(cls.globalServers[0]), CL_CompareAdrSigned);
    }
    return LAN_ADD_SERVER_ADDED;
}

/* Source: CoDUOMP.exe 0x0041a950..0x0041aa85.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a950_0041aa86.mcode.
 * Name and signature: exact same-module Mac symbol LAN_RemoveServer and UI
 * command 89. Address parse failure is intentionally ignored before the
 * comparison scan, matching the original. */
void LAN_RemoveServer(lan_server_source_t source, const char *address)
{
    lan_server_info_t *servers;
    int32_t *serverCount;
    netadr_t parsedAddress;

    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        servers = cls.localServers;
        serverCount = &cls.numLocalServers;
        break;
    case LAN_SERVER_SOURCE_GLOBAL:
        servers = cls.globalServers;
        serverCount = &cls.numGlobalServers;
        break;
    case LAN_SERVER_SOURCE_FAVORITES:
        servers = cls.favoriteServers;
        serverCount = &cls.numFavoriteServers;
        break;
    default:
        return;
    }

    (void)NET_StringToAdr(address, &parsedAddress);
    for (int32_t index = 0; index < *serverCount; ++index) {
        if (NET_CompareAdrSigned(&parsedAddress, &servers[index].address) != 0) {
            continue;
        }
        for (; index < *serverCount - 1; ++index) {
            memmove(&servers[index], &servers[index + 1], sizeof(servers[0]));
        }
        --*serverCount;
        return;
    }
}

/* Source: CoDUOMP.exe 0x0041aac0..0x0041ab7c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041aac0_0041ab7d.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * LAN_GetServerAddressString and UI syscall command 81. */
void LAN_GetServerAddressString(lan_server_source_t source, int32_t serverIndex, char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (buffer == NULL || bufferSize <= 0)
        return;
    buffer[0] = '\0';

    const lan_server_info_t *server = LAN_GetServerPtr(source, serverIndex);

    if (server == NULL)
        return;

    const int32_t copySize = bufferSize < NET_ADDRESS_STRING_SIZE ? bufferSize : NET_ADDRESS_STRING_SIZE;
    strncpy(buffer, NET_AdrToString(server->address), (size_t)copySize - 1u);
    buffer[copySize - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x0041ab80..0x0041af37.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041ab80_0041af38.mcode and exact
 * .rdata key strings at 0x00593334..0x005933e3.
 * Name and argument roles: exact same-module Mac symbol LAN_GetServerInfo and
 * UI syscall command 82. The signed loads for friendly-fire, kill-cam, and
 * console-disabled values, and the signed word loads for the final six numeric
 * fields, are deliberately retained by the record types. */
void LAN_GetServerInfo(lan_server_source_t source, int32_t serverIndex, char *buffer, int32_t bufferSize)
{
    char info[LAN_SERVER_INFO_STRING_SIZE] = "";
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (buffer == NULL || bufferSize <= 0)
        return;
    buffer[0] = '\0';

    const lan_server_info_t *server = LAN_GetServerPtr(source, serverIndex);
    if (server == NULL)
        return;

    Info_SetValueForKey(info, "hostname", server->hostName);
    Info_SetValueForKey(info, "mapname", server->mapName);
    Info_SetValueForKey(info, "clients", va("%i", server->clients));
    Info_SetValueForKey(info, "sv_maxclients", va("%i", server->maxClients));
    const coduomp_server_bot_cache_entry_t *const botCache = coduomp_find_server_bot_cache(server->address, qfalse);
    Info_SetValueForKey(info, "bots", va("%i", botCache != NULL ? botCache->displayedBotCount : 0));
    Info_SetValueForKey(info, "ping", va("%i", server->ping));
    Info_SetValueForKey(info, "minping", va("%i", server->minPing));
    Info_SetValueForKey(info, "maxping", va("%i", server->maxPing));
    Info_SetValueForKey(info, "game", server->game);
    Info_SetValueForKey(info, "gametype", server->gameType);
    Info_SetValueForKey(info, "nettype", va("%i", server->netType));
    Info_SetValueForKey(info, "addr", NET_AdrToString(server->address));
    Info_SetValueForKey(info, "sv_allowAnonymous", va("%i", server->allowAnonymous));
    Info_SetValueForKey(info, "con_disabled", va("%i", server->consoleDisabled));
    Info_SetValueForKey(info, "pswrd", va("%i", server->passwordRequired));
    Info_SetValueForKey(info, "pure", va("%i", server->pure));
    Info_SetValueForKey(info, "ff", va("%i", server->friendlyFire));
    Info_SetValueForKey(info, "kc", va("%i", server->killCam));
    Info_SetValueForKey(info, "hw", va("%i", server->hardware));
    Info_SetValueForKey(info, "mod", va("%i", server->mod));
    Info_SetValueForKey(info, "timeoutsallowed", va("%i", server->timeoutsAllowed));
    Info_SetValueForKey(info, "jps", va("%i", server->jeepsAllowed));
    Info_SetValueForKey(info, "tnk", va("%i", server->tanksAllowed));
    Info_SetValueForKey(info, "pb", va("%i", server->punkBuster));

    const int32_t copySize = bufferSize < LAN_SERVER_INFO_STRING_SIZE ? bufferSize : LAN_SERVER_INFO_STRING_SIZE;
    strncpy(buffer, info, (size_t)copySize - 1u);
    buffer[copySize - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x0041b000..0x0041b053.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b000_0041b054.mcode.
 * Name: exact same-module Mac symbol LAN_GetServerPtr. */
lan_server_info_t *LAN_GetServerPtr(lan_server_source_t source, int32_t serverIndex)
{
    if (serverIndex < 0)
        return NULL;

    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        if (serverIndex < LAN_LOCAL_SERVER_CAPACITY)
            return &cls.localServers[serverIndex];
        break;
    case LAN_SERVER_SOURCE_GLOBAL:
        if (serverIndex < cls.numGlobalServers)
            return &cls.globalServers[serverIndex];
        break;
    case LAN_SERVER_SOURCE_FAVORITES:
        if (serverIndex < LAN_FAVORITE_SERVER_CAPACITY)
            return &cls.favoriteServers[serverIndex];
        break;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x0041b060..0x0041b091.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b060_0041b092.mcode.
 * Name and argument roles: exact same-module Mac symbol LAN_CleanHostname. */
void LAN_CleanHostname(const char *source, char *destination)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    const char *const sourceEnd = source + LAN_SERVER_HOSTNAME_SIZE - 1;
    while (source < sourceEnd && *source != '\0') {
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the renderer hides
         * ^0 through ^9, and the extended UI convention also hides ^#.
         * Neither byte of those pairs participates in the visible name. */
        if (*source == '^' && source + 1 < sourceEnd && ((source[1] >= '0' && source[1] <= '9') || source[1] == '#')) {
            source += 2;
            continue;
        }
        const unsigned char character = (unsigned char)*source++;
        if (coduo_crt_isalpha(character) != 0 || (character >= '0' && character <= '9'))
            *destination++ = (char)character;
    }
    *destination = '\0';
}

/* NOT_FROM_ORIGINAL_SOURCE: compatibility ordering for human-visible server
 * names. ASCII letters precede digits; names without either use visible
 * punctuation, and names with no visible bytes use their raw byte sequence. */
static int32_t coduomp_server_name_compare_compat(const char *left, const char *right)
{
    enum {
        CODUOMP_SERVER_NAME_SIDE_COUNT = 2,
        CODUOMP_SERVER_NAME_LETTER_COUNT = 26
    };
    char alnumKeys[CODUOMP_SERVER_NAME_SIDE_COUNT][LAN_SERVER_HOSTNAME_SIZE];
    uint8_t visibleKeys[CODUOMP_SERVER_NAME_SIDE_COUNT][LAN_SERVER_HOSTNAME_SIZE];
    size_t visibleLengths[CODUOMP_SERVER_NAME_SIDE_COUNT] = {0, 0};
    const char *const names[CODUOMP_SERVER_NAME_SIDE_COUNT] = {left, right};

    LAN_CleanHostname(left, alnumKeys[0]);
    LAN_CleanHostname(right, alnumKeys[1]);

    if ((alnumKeys[0][0] == '\0') != (alnumKeys[1][0] == '\0'))
        return alnumKeys[0][0] == '\0' ? 1 : -1;

    if (alnumKeys[0][0] != '\0') {
        size_t index = 0;

        for (;;) {
            const unsigned char leftCharacter = (unsigned char)alnumKeys[0][index];
            const unsigned char rightCharacter = (unsigned char)alnumKeys[1][index];
            int32_t leftRank;
            int32_t rightRank;

            if (leftCharacter == '\0' || rightCharacter == '\0') {
                if (leftCharacter == rightCharacter)
                    return 0;
                return leftCharacter == '\0' ? -1 : 1;
            }

            if (leftCharacter >= '0' && leftCharacter <= '9') {
                leftRank = CODUOMP_SERVER_NAME_LETTER_COUNT + leftCharacter - '0';
            } else if (leftCharacter >= 'a' && leftCharacter <= 'z') {
                leftRank = leftCharacter - 'a';
            } else {
                leftRank = leftCharacter - 'A';
            }

            if (rightCharacter >= '0' && rightCharacter <= '9') {
                rightRank = CODUOMP_SERVER_NAME_LETTER_COUNT + rightCharacter - '0';
            } else if (rightCharacter >= 'a' && rightCharacter <= 'z') {
                rightRank = rightCharacter - 'a';
            } else {
                rightRank = rightCharacter - 'A';
            }

            if (leftRank != rightRank)
                return leftRank < rightRank ? -1 : 1;
            ++index;
        }
    }

    for (size_t side = 0; side < CODUOMP_SERVER_NAME_SIDE_COUNT; ++side) {
        const char *source = names[side];
        const char *const sourceEnd = source + LAN_SERVER_HOSTNAME_SIZE - 1;

        while (source < sourceEnd && *source != '\0') {
            if (*source == '^' && source + 1 < sourceEnd && ((source[1] >= '0' && source[1] <= '9') || source[1] == '#')) {
                source += 2;
                continue;
            }

            const uint8_t character = (uint8_t)*source++;
            if (character >= (uint8_t)' ' && character <= (uint8_t)'~') {
                visibleKeys[side][visibleLengths[side]++] = character;
            }
        }
    }

    if ((visibleLengths[0] == 0) != (visibleLengths[1] == 0))
        return visibleLengths[0] == 0 ? 1 : -1;

    if (visibleLengths[0] != 0) {
        const size_t commonLength = visibleLengths[0] < visibleLengths[1] ? visibleLengths[0] : visibleLengths[1];

        for (size_t index = 0; index < commonLength; ++index) {
            if (visibleKeys[0][index] != visibleKeys[1][index]) {
                return visibleKeys[0][index] < visibleKeys[1][index] ? -1 : 1;
            }
        }
        if (visibleLengths[0] != visibleLengths[1])
            return visibleLengths[0] < visibleLengths[1] ? -1 : 1;
        return 0;
    }

    for (size_t index = 0; index < LAN_SERVER_HOSTNAME_SIZE; ++index) {
        const uint8_t leftCharacter = (uint8_t)left[index];
        const uint8_t rightCharacter = (uint8_t)right[index];

        if (leftCharacter != rightCharacter)
            return leftCharacter < rightCharacter ? -1 : 1;
        if (leftCharacter == '\0')
            return 0;
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x0041b0a0..0x0041b108.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b0a0_0041b109.mcode.
 * Name and argument roles: exact same-module Mac symbol LAN_CompareHostname.
 * In strict-stock builds, cleaned names sort first and original spelling
 * breaks normalized ties. Compatibility builds use the visible-name ordering
 * above instead. */
int32_t LAN_CompareHostname(const char *left, const char *right)
{

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (left == NULL || right == NULL)
        return -1;

    return coduomp_server_name_compare_compat(left, right);
}

/* Source: CoDUOMP.exe 0x0041b110..0x0041b230.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b110_0041b231.mcode and jump table
 * 0x0041b234. Name, signature, and sort-key meanings: exact same-module Mac
 * symbol LAN_CompareServers plus UI command 100 and the UI server columns.
 * In strict-stock builds, every equal primary key falls through to ping, game
 * type, then normalized hostname as deterministic tie breakers. Compatibility
 * builds instead use hostname, ping, maximum players, map name, then game type.
 * Hardware value zero is ordered after nonzero hardware values before optional
 * direction reversal. */
int32_t LAN_CompareServers(lan_server_source_t source, lan_server_sort_key_t sortKey, qboolean sortDescending, int32_t firstServerIndex,
                           int32_t secondServerIndex)
{
    const lan_server_info_t *first = LAN_GetServerPtr(source, firstServerIndex);
    const lan_server_info_t *second = LAN_GetServerPtr(source, secondServerIndex);
    int32_t comparison = 0;

    if (first == NULL || second == NULL)
        return 0;

    switch (sortKey) {
    case LAN_SERVER_SORT_PASSWORD:
        comparison = (int32_t)first->passwordRequired - (int32_t)second->passwordRequired;
        break;
    case LAN_SERVER_SORT_HARDWARE:
        comparison = (int32_t)first->hardware - (int32_t)second->hardware;
        if (comparison < 0 && first->hardware == 0)
            comparison = 1;
        else if (comparison > 0 && second->hardware == 0)
            comparison = -1;
        break;
    case LAN_SERVER_SORT_HOSTNAME:
        comparison = LAN_CompareHostname(first->hostName, second->hostName);
        break;
    case LAN_SERVER_SORT_MAPNAME:
        comparison = Q_stricmp(first->mapName, second->mapName);
        break;
    case LAN_SERVER_SORT_CLIENTS:
        comparison = (int32_t)first->clients - (int32_t)second->clients;
        break;
    case LAN_SERVER_SORT_GAMETYPE:
        comparison = Q_stricmp(first->gameType, second->gameType);
        break;
    case LAN_SERVER_SORT_PUNKBUSTER:
        comparison = (int32_t)first->punkBuster - (int32_t)second->punkBuster;
        break;
    case LAN_SERVER_SORT_MOD:
        comparison = (int32_t)first->mod - (int32_t)second->mod;
        break;
    case LAN_SERVER_SORT_PING:
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): ping remains the
         * primary key even though compatibility tie breaking starts at the
         * hostname. */
        comparison = (int32_t)first->ping - (int32_t)second->ping;
        break;
    default:
        return 0;
    }

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): keep equal primary keys
     * stable under the complete user-facing server identity. */
    if (comparison == 0)
        comparison = LAN_CompareHostname(first->hostName, second->hostName);
    if (comparison == 0)
        comparison = (int32_t)first->ping - (int32_t)second->ping;
    if (comparison == 0)
        comparison = (int32_t)first->maxClients - (int32_t)second->maxClients;
    if (comparison == 0)
        comparison = Q_stricmp(first->mapName, second->mapName);
    if (comparison == 0)
        comparison = Q_stricmp(first->gameType, second->gameType);

    return sortDescending != qfalse ? -comparison : comparison;
}

/* Source: CoDUOMP.exe 0x0041af40..0x0041af9e.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041af40_0041af9f.mcode.
 * Name: exact same-module Mac symbol LAN_GetServerPing. */
int32_t LAN_GetServerPing(lan_server_source_t source, int32_t serverIndex)
{
    const lan_server_info_t *server = LAN_GetServerPtr(source, serverIndex);
    return server != NULL ? server->ping : -1;
}

/* Source: CoDUOMP.exe 0x0041afa0..0x0041affe.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041afa0_0041afff.mcode.
 * Name: exact same-module Mac symbol LAN_GetServerPunkBuster. */
int32_t LAN_GetServerPunkBuster(lan_server_source_t source, int32_t serverIndex)
{
    const lan_server_info_t *server = LAN_GetServerPtr(source, serverIndex);
    return server != NULL ? server->punkBuster : -1;
}

/* Source: CoDUOMP.exe 0x0041b2f0..0x0041b3a0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b2f0_0041b3a1.mcode.
 * Name: exact same-module Mac symbol LAN_MarkServerDirty. */
void LAN_MarkServerDirty(lan_server_source_t source, int32_t serverIndex, qboolean dirty)
{
    lan_server_info_t *servers;
    int32_t serverCount;

    Com_PumpMessageLoop();

    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        servers = cls.localServers;
        serverCount = LAN_LOCAL_SERVER_CAPACITY;
        break;
    case LAN_SERVER_SOURCE_GLOBAL:
        servers = cls.globalServers;
        serverCount = cls.numGlobalServers;
        break;
    case LAN_SERVER_SOURCE_FAVORITES:
        servers = cls.favoriteServers;
        serverCount = LAN_FAVORITE_SERVER_CAPACITY;
        break;
    default:
        return;
    }

    if (serverIndex == -1) {
        for (int32_t index = 0; index < serverCount; ++index)
            servers[index].dirty = (uint8_t)dirty;
        return;
    }

    if (serverIndex >= 0 && serverIndex < serverCount)
        servers[serverIndex].dirty = (uint8_t)dirty;
}

/* Source: CoDUOMP.exe 0x0041b3b0..0x0041b409.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0041b3b0_0041b40a.mcode.
 * Name: exact same-module Mac symbol LAN_ServerIsDirty. */
qboolean LAN_ServerIsDirty(lan_server_source_t source, int32_t serverIndex)
{
    const lan_server_info_t *servers;
    int32_t serverCount;

    switch (source) {
    case LAN_SERVER_SOURCE_LOCAL:
        servers = cls.localServers;
        serverCount = LAN_LOCAL_SERVER_CAPACITY;
        break;
    case LAN_SERVER_SOURCE_GLOBAL:
        servers = cls.globalServers;
        serverCount = cls.numGlobalServers;
        break;
    case LAN_SERVER_SOURCE_FAVORITES:
        servers = cls.favoriteServers;
        serverCount = LAN_FAVORITE_SERVER_CAPACITY;
        break;
    default:
        return qfalse;
    }

    if (serverIndex < 0 || serverIndex >= serverCount)
        return qfalse;
    return servers[serverIndex].dirty != 0 ? qtrue : qfalse;
}
