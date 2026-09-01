#include "net_loopback.h"

#include <stdint.h>
#include <string.h>

void Com_Printf(const char *format, ...);

/*
 * Complete two-endpoint loopback transport.
 *
 * CoDUOMP.exe 0x0044dfa0 and coduo_lnxded 0x08084b49 agree on queue
 * selection, signed counter comparisons, 16-record masking, message copy,
 * cursor publication, and the zeroed NA_LOOPBACK source address.  The send
 * bodies at 0x0044e040 and 0x08084c4c likewise agree on the opposite-endpoint
 * queue, increment-before-copy order, and unchecked signed copy length.
 */
qboolean NET_GetLoopPacket(netsrc_t source, netadr_t *address,
                           msg_t *message)
{
    loopback_t *loop = &net_loopbacks[source];
    loopmsg_t *loopMessage;

    if (loop->send - loop->get > NET_LOOPBACK_MESSAGE_COUNT) {
        loop->get = loop->send - NET_LOOPBACK_MESSAGE_COUNT;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (loop->get >= loop->send) {
        return qfalse;
    }

    loopMessage =
        &loop->msgs[loop->get & (NET_LOOPBACK_MESSAGE_COUNT - 1)];
    ++loop->get;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (loopMessage->datalen < 0 ||
        loopMessage->datalen > NET_LOOPBACK_MESSAGE_BYTES ||
        loopMessage->datalen > message->maxsize) {
        Com_Printf("NET_GetLoopPacket: invalid packet length %i\n", loopMessage->datalen);
        return qfalse;
    }
    if (loopMessage->datalen > 0) {
        memcpy(message->data, loopMessage->data, (size_t)loopMessage->datalen);
    }
    message->cursize = loopMessage->datalen;

    memset(address, 0, sizeof(*address));
    address->type = NA_LOOPBACK;
    return qtrue;
}

void NET_SendLoopPacket(netsrc_t source, int32_t length,
                        const void *data)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0 || length > NET_LOOPBACK_MESSAGE_BYTES) {
        Com_Printf("NET_SendLoopPacket: invalid packet length %i\n", length);
        return;
    }

    loopback_t *loop =
        &net_loopbacks[source ^ NET_LOOPBACK_SOCKET_PAIR_MASK];
    loopmsg_t *message =
        &loop->msgs[loop->send & (NET_LOOPBACK_MESSAGE_COUNT - 1)];

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ++loop->send;

    if (length > 0) {
        memcpy(message->data, data, (size_t)length);
    }
    message->datalen = length;
}
