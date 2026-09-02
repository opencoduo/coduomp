#include "netchan_private.h"
#include "../core_runtime/core_runtime_private.h"

#include <stdint.h>
#include <string.h>

enum {
    NET_OOB_MARKER = -1
};

void NET_SendPacket(netsrc_t sock,
                    int32_t length,
                    const void *data, netadr_t to)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("NET_SendPacket: invalid packet length %i\n", length);
        return;
    }

    if (showpackets->integer != 0) {
        int32_t marker;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (length >= (int32_t)sizeof(marker)) {
            memcpy(&marker, data, sizeof(marker));
            if (marker == NET_OOB_MARKER) {
                Com_Printf("send packet %4i\n", length);
            }
        }
    }

    if (to.type == NA_LOOPBACK) {
        NET_SendLoopPacket(sock, length, data);
        return;
    }

    if (to.type != NA_BOT && to.type != NA_BAD) {
        Sys_SendPacket(length, data, to);
    }
}
