#include "../core_cvar/cvar_private.h"
#include "../punkbuster/pb_private.h"
#include "qcommon/net_compare.h"
#include "server_packet_services.h"
#include "sv_globals_private.h"

#include <stdint.h>

enum {
    SV_PB_MESSAGE_COMMAND_OFFSET = 7,
    SV_PB_SERVER_COMMAND = 13,
    SV_PB_OOB_MARKER_BYTES = 4
};

static const char svCvarNamePunkBuster[] = "sv_punkbuster";

/* Source: coduo_lnxded 0x080921f6..0x08092210. This target-local bridge is
 * called by the dedicated engine's in-process PunkBuster implementation. */
void SV_SetPunkBusterCvar(const char *value)
{
    Cvar_Set(svCvarNamePunkBuster, value);
}

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Linux-only in-process
 * PunkBuster portion of SV_ConnectionlessPacket at 0x0809434a..0x0809445d. */
void server_compat_handle_pb_packet(netadr_t from, msg_t *message)
{
    int32_t pbClientNum = -1;
    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state != CS_FREE &&
            NET_CompareBaseAdr(from, client->netchan.remoteAddress) !=
                qfalse &&
            client->netchan.remoteAddress.port == from.port) {
            pbClientNum = clientNum;
            break;
        }
    }

    const uint8_t command = message->data[SV_PB_MESSAGE_COMMAND_OFFSET];
    if (command != 'C' && command != '1' && command != 'J') {
        PB_CallServerSbGlobal(
            SV_PB_SERVER_COMMAND, pbClientNum,
            (uint32_t)(message->cursize - SV_PB_OOB_MARKER_BYTES),
            (const char *)message->data + SV_PB_OOB_MARKER_BYTES);
    }
}
