#include "server_connect.h"

#include "qcommon/game_module_abi_types.h"
#include "qcommon/info.h"
#include "qcommon/net_compare.h"
#include "qcommon/net_text.h"
#include "qcommon/netchan.h"
#include "qcommon/q_command.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_limits.h"
#include "qcommon/qcommon_runtime_types.h"
#include "scripting/script_variable.h"
#include "server_client_release.h"
#include "server_client_message.h"
#include "server_connect_services.h"
#include "server_game_data.h"
#include "server_operator_runtime.h"
#include "qcommon/server_runtime_types.h"
#include "qcommon/vm_runtime.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    SERVER_CONNECT_MILLISECONDS_PER_SECOND = 1000
};

extern serverStatic_t svs;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_reconnectlimit;
extern cvar_t *sv_minPing;
extern cvar_t *sv_maxPing;
extern cvar_t *sv_privateClients;
extern cvar_t *sv_privatePassword;
extern vm_t *sv_gameVM;

void Com_DPrintf(const char *format, ...);
void Com_Printf(const char *format, ...);
qboolean Sys_IsLANAddress(netadr_t address);
/*
 * Complete direct-connect transaction shared by both server engines:
 *
 *   CoDUOMP.exe   0x00459b20..0x0045a5dc
 *   coduo_lnxded  0x0808ac82..0x0808b9b5
 *
 * Both bodies agree on protocol and reconnect validation, full-address
 * challenge matching, ping gates, client-slot selection, old-client release,
 * record initialization, game-VM admission, the client-indexed challenge
 * reset, state/timestamp initialization, and heartbeat selection. The two
 * original engines reach different PunkBuster implementations through the
 * target-owned service header, but consume the callback result identically.
 */
void SV_DirectConnect(netadr_t from)
{
    char userinfo[MAX_STRING_CHARS];

    Com_DPrintf("SVC_DirectConnect ()\n");
    Q_strncpyz(userinfo, Cmd_Argv(1), sizeof(userinfo));

    const int32_t protocol = atoi(Info_ValueForKey(userinfo, "protocol"));
    if (protocol != SERVER_PROTOCOL_VERSION) {
        NET_OutOfBandPrint(NS_SERVER, from, "error\n%s",
                           "EXE_SERVER_IS_DIFFERENT_VER");
        Com_DPrintf(
            "    rejected connect from protocol version %i (should be %i)\n",
            protocol, SERVER_PROTOCOL_VERSION);
        return;
    }

    const int32_t challengeNumber =
        atoi(Info_ValueForKey(userinfo, "challenge"));
    const int32_t qport = atoi(Info_ValueForKey(userinfo, "qport"));

    client_t *client = svs.clients;
    int32_t clientNum;
    for (clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum, ++client) {
        if (NET_CompareBaseAdr(from, client->netchan.remoteAddress) != qfalse &&
            (client->netchan.qport == qport ||
             from.port == client->netchan.remoteAddress.port)) {
            if (svs.realTime - client->lastConnectTime <
                sv_reconnectlimit->integer *
                    SERVER_CONNECT_MILLISECONDS_PER_SECOND) {
                Com_DPrintf("%s:reconnect rejected : too soon\n",
                            NET_AdrToString(from));
                return;
            }
            break;
        }
    }

    int32_t guid = 0;
    int32_t challengeIndex = 0;
    if (NET_IsLocalAddress(from) == qfalse) {
        for (challengeIndex = 0;
             challengeIndex < MAX_CHALLENGES;
             ++challengeIndex) {
            challenge_t *const challenge = &svs.challenges[challengeIndex];
            /* Both authoritative bodies require the original source port as
             * well as the address bytes when matching a challenge row. */
            if (NET_CompareAdr(from, challenge->address) != qfalse &&
                challengeNumber == challenge->challengeNumber) {
                guid = challenge->numericGuid;
                break;
            }
        }

        if (challengeIndex == MAX_CHALLENGES) {
            NET_OutOfBandPrint(NS_SERVER, from,
                               "error\nEXE_BAD_CHALLENGE");
            return;
        }

        challenge_t *const challenge = &svs.challenges[challengeIndex];
        int32_t ping;
        if (challenge->firstPingMsec == 0) {
            ping = svs.realTime - challenge->pingStartTime;
            challenge->firstPingMsec = ping;
        } else {
            ping = challenge->firstPingMsec;
        }

        Com_Printf("Client %i connecting with %i challenge ping from %s\n",
                   challengeIndex, ping, NET_AdrToString(from));
        challenge->connected = qtrue;

        if (Sys_IsLANAddress(from) == qfalse) {
            /* Both x87 bodies compare the exact FILD result directly with the
             * binary32 cvar value; no float store occurs. */
            if (sv_minPing->value != 0.0f &&
                (long double)ping < (long double)sv_minPing->value) {
                NET_OutOfBandPrint(NS_SERVER, from,
                                   "error\nEXE_ERR_HIGH_PING_ONLY");
                Com_DPrintf("Client %i rejected on a too low ping\n",
                            challengeIndex);
                return;
            }
            if (sv_maxPing->value != 0.0f &&
                (long double)sv_maxPing->value < (long double)ping) {
                NET_OutOfBandPrint(NS_SERVER, from,
                                   "error\nEXE_ERR_LOW_PING_ONLY");
                Com_DPrintf(
                    "Client %i rejected on a too high ping: %i\n",
                    challengeIndex, ping);
                return;
            }
        }
    }

    const int32_t clPunkbuster =
        atoi(Info_ValueForKey(userinfo, "cl_punkbuster"));
    const char *const clGuid = Info_ValueForKey(userinfo, "cl_guid");
    const char *const pbReject =
        server_compat_pb_connect_query(from, clPunkbuster, clGuid);
    if (pbReject != NULL) {
        if (Q_stricmpn(pbReject, "error\n", 6) == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: forward callback text as formatter
             * data through a literal conversion. */
            NET_OutOfBandPrint(NS_SERVER, from, "%s", pbReject);
        }
        return;
    }

    client_t emptyClient;
    memset(&emptyClient, 0, sizeof(emptyClient));

    client = svs.clients;
    for (clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum, ++client) {
        if (client->state != CS_FREE &&
            NET_CompareBaseAdr(from, client->netchan.remoteAddress) != qfalse &&
            (client->netchan.qport == qport ||
             from.port == client->netchan.remoteAddress.port)) {
            Com_Printf("%s:reconnect\n", NET_AdrToString(from));
            if (client->state >= CS_CONNECTED) {
                SV_FreeClient(client);
            }
            break;
        }
    }

    if (clientNum == sv_maxclients->integer) {
        const char *const password = Info_ValueForKey(userinfo, "password");
        const int32_t firstAvailableClient =
            strcmp(password, sv_privatePassword->string) == 0
                ? 0
                : sv_privateClients->integer;

        client = NULL;
        for (clientNum = firstAvailableClient;
             clientNum < sv_maxclients->integer;
             ++clientNum) {
            client_t *const candidate = &svs.clients[clientNum];
            if (candidate->state == CS_FREE) {
                client = candidate;
                break;
            }
        }
        if (client == NULL) {
            NET_OutOfBandPrint(NS_SERVER, from,
                               "error\nEXE_SERVERISFULL");
            Com_DPrintf("Rejected a connection.\n");
            return;
        }

        client->reliableAcknowledge = 0;
        client->reliableSequence = 0;
    }

    *client = emptyClient;
    clientNum = (int32_t)(client - svs.clients);
    client->gentity = SV_GentityNum(clientNum);
    client->scriptId = Scr_AllocArray();
    client->challenge = challengeNumber;
    client->guid = guid;
    Netchan_Setup(NS_SERVER, &client->netchan, from, qport);
    Q_strncpyz(client->userinfo, userinfo, sizeof(client->userinfo));

    const char *const gameReject = (const char *)VM_Call(
        sv_gameVM, GAME_CLIENT_CONNECT,
        clientNum, client->scriptId,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (gameReject != NULL) {
        NET_OutOfBandPrint(NS_SERVER, from, "error\n%s", gameReject);
        Com_DPrintf("Game rejected a connection: %s.\n", gameReject);
        SV_FreeClientScriptId(client);
        return;
    }

    SV_UserinfoChanged(client);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    svs.challenges[clientNum].firstPingMsec = 0;
    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");
    Com_Printf(
        "Going from CS_FREE to CS_CONNECTED for %s (num %i guid %i)\n",
        client->name, clientNum, client->guid);
    client->state = CS_CONNECTED;
    client->lastPacketTime = svs.realTime;
    client->lastConnectTime = svs.realTime;
    client->nextSnapshotTime = svs.realTime;
    client->gamestateMessageNum = -1;

    int32_t connectedCount = 0;
    for (clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        if (svs.clients[clientNum].state >= CS_CONNECTED) {
            ++connectedCount;
        }
    }
    if (connectedCount == 1 || connectedCount == sv_maxclients->integer) {
        SV_Heartbeat_f();
    }
}
