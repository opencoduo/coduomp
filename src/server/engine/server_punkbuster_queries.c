#include "server_punkbuster_queries.h"

#include "qcommon/net_text.h"
#include "qcommon/qcommon_limits.h"
#include "qcommon/qcommon_runtime_types.h"
#include "server_master.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    SERVER_PB_CLIENT_NAME_FIELD_SIZE = MAX_NAME_LENGTH + 1,
    SERVER_PB_CLIENT_GUID_OFFSET = SERVER_PB_CLIENT_NAME_FIELD_SIZE,
    SERVER_PB_CLIENT_ADDRESS_OFFSET =
        SERVER_PB_CLIENT_GUID_OFFSET + SERVER_PUNKBUSTER_GUID_SIZE,
    SERVER_PB_CLIENT_INFO_BUFFER_SIZE = 104
};

extern serverStatic_t svs;
extern cvar_t *sv_maxclients;

/*
 * Complete engine-owned PunkBuster query and packet-output cluster:
 *
 * Function                    Windows       Linux
 * Pb_Q_maxclients             0x004628e0    0x0809475b
 * Pb_Q_client                 0x004628f0    0x08094768
 * Pb_Q_stats                  0x004629c0    0x08094895
 * SV_SendPbPacket             0x00462a20    0x08094930
 *
 * Both engines agree on validation, client-state thresholds, the 104-byte
 * output record with fields at +0x00/+0x21/+0x42, score formatting, server
 * initialization gating, client selection, and packet arguments.  Windows
 * expands the clears and copies and uses its optimized internal calling
 * conventions; Linux retains ordinary libc calls and cdecl entry points.
 * The supporting Mac client supplies the canonical names above.
 */

int32_t Pb_Q_maxclients(void)
{
    return sv_maxclients->integer;
}

qboolean Pb_Q_client(int32_t clientNum, char *info)
{
    memset(info, 0, SERVER_PB_CLIENT_INFO_BUFFER_SIZE);
    if (clientNum < 0 || clientNum >= sv_maxclients->integer ||
        svs.clients == NULL || svs.clients[clientNum].state < CS_ACTIVE) {
        return qfalse;
    }

    client_t *const client = &svs.clients[clientNum];
    strcpy(info, client->name);
    strcpy(info + SERVER_PB_CLIENT_GUID_OFFSET, client->punkbusterGuid);
    strcpy(info + SERVER_PB_CLIENT_ADDRESS_OFFSET,
           NET_AdrToString(client->netchan.remoteAddress));
    return qtrue;
}

qboolean Pb_Q_stats(int32_t clientNum, char *info)
{
    info[0] = '\0';
    if (clientNum < 0 || clientNum >= sv_maxclients->integer ||
        svs.clients == NULL || svs.clients[clientNum].state < CS_ACTIVE) {
        return qfalse;
    }

    client_t *const client = &svs.clients[clientNum];
    sprintf(info, "ping=%d score=%d", client->ping,
            SV_GetClientScore(client));
    return qtrue;
}

void SV_SendPbPacket(int32_t length, const void *data, int32_t clientNum)
{
    if (svs.initialized != qtrue) {
        return;
    }

    for (int32_t slot = 0; slot < sv_maxclients->integer; ++slot) {
        client_t *const client = &svs.clients[slot];
        if ((clientNum < 0 || clientNum == slot) &&
            client->state >= CS_CONNECTED) {
            NET_OutOfBandPbPacket(NS_SERVER, client->netchan.remoteAddress,
                                  data, length);
        }
    }
}
