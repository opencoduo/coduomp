#include "server_master.h"

#include "qcommon/game_module_abi_types.h"
#include "qcommon/net_text.h"
#include "qcommon/q_endian.h"
#include "qcommon/qcommon_runtime_types.h"
#include "server_packet.h"
#include "qcommon/vm_runtime.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

enum {
    SERVER_MASTER_DEDICATED_MODE = 2,
    SERVER_MASTER_HEARTBEAT_INTERVAL_MSEC = 180000,
    SERVER_MASTER_STATUS_INTERVAL_MSEC = 600000,
    SERVER_MASTER_PORT = 20610
};

extern serverStatic_t svs;
extern cvar_t *dedicated;
extern vm_t *sv_gameVM;

void Com_Printf(const char *format, ...);

static const char sv_masterServerName[] =
    "coduomaster.activision.com";

/* This cache is separate from the remote-console and authorization addresses
 * stored in serverStatic_t: CoDUOMP.exe 0x009cd930 and coduo_lnxded
 * 0x0829f1c0. */
static netadr_t sv_masterAddress;

/*
 * Complete server-master address, heartbeat, shutdown, and score-query
 * cluster shared by the Windows client engine and Linux dedicated engine:
 *
 * Function                         Windows       Linux
 * SV_MasterAddress                 0x00460f40    0x0809308f
 * SV_MasterHeartbeat               0x00461000    0x08093159
 * SV_MasterGameCompleteStatus      0x004610d0    0x0809324a
 * SV_MasterShutdown                0x00461130    0x080932ab
 * SV_GetClientScore                0x00461150    0x080932c9
 *
 * The operations, intervals, state gates, address cache, packet arguments,
 * VM command, and client-index calculation agree. The supporting Mac client
 * exports these same canonical names. Linux's prior formatted master-name
 * diagnostics were reconstruction artifacts: its rodata and call arguments
 * use the same fixed strings as Windows.
 */

netadr_t *SV_MasterAddress(void)
{
    if (sv_masterAddress.type != NA_BAD) {
        return &sv_masterAddress;
    }

    Com_Printf("Resolving %s\n", sv_masterServerName);
    if (NET_StringToAdr(sv_masterServerName, &sv_masterAddress) == qfalse) {
        Com_Printf("Couldn't resolve address: coduomaster.activision.com\n");
        return &sv_masterAddress;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (strstr(":", sv_masterServerName) == NULL) {
        sv_masterAddress.port =
            (uint16_t)BigShort((int16_t)SERVER_MASTER_PORT);
    }

    Com_Printf(
        "coduomaster.activision.com resolved to %i.%i.%i.%i:%i\n",
        sv_masterAddress.ip[0], sv_masterAddress.ip[1],
        sv_masterAddress.ip[2], sv_masterAddress.ip[3],
        BigShort((int16_t)sv_masterAddress.port));
    return &sv_masterAddress;
}

void SV_MasterHeartbeat(const char *heartbeat)
{
    if (dedicated == NULL ||
        dedicated->integer != SERVER_MASTER_DEDICATED_MODE) {
        return;
    }

    if (svs.realTime >= svs.nextHeartbeatTime) {
        svs.nextHeartbeatTime =
            svs.realTime + SERVER_MASTER_HEARTBEAT_INTERVAL_MSEC;
        netadr_t *const address = SV_MasterAddress();
        if (address->type != NA_BOT) {
            Com_Printf(
                "Sending heartbeat to coduomaster.activision.com\n");
            NET_OutOfBandPrint(NS_SERVER, *address, "heartbeat %s\n",
                               heartbeat);
        }
    }

    if (svs.realTime >= svs.nextStatusResponseTime) {
        svs.nextStatusResponseTime =
            svs.realTime + SERVER_MASTER_STATUS_INTERVAL_MSEC;
        netadr_t *const address = SV_MasterAddress();
        if (address->type != NA_BOT) {
            SVC_Status(*address);
        }
    }
}

void SV_MasterGameCompleteStatus(void)
{
    if (dedicated == NULL ||
        dedicated->integer != SERVER_MASTER_DEDICATED_MODE) {
        return;
    }

    netadr_t *const address = SV_MasterAddress();
    if (address->type != NA_BOT) {
        Com_Printf(
            "Sending gameCompleteStatus to coduomaster.activision.com\n");
        SVC_GameCompleteStatus(*address);
    }
}

void SV_MasterShutdown(void)
{
    svs.nextHeartbeatTime = INT32_MIN;
    SV_MasterHeartbeat("flatline");
}

int32_t SV_GetClientScore(client_t *client)
{
    if (sv_gameVM == NULL) {
        return 0;
    }

    const int32_t clientNum = (int32_t)(client - svs.clients);
    return (int32_t)VM_Call(
        sv_gameVM, GAME_GET_CLIENT_SCORE,
        clientNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
