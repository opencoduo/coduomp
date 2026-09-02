#include "net_text.h"

#include "com_sprintf.h"
#include "q_endian.h"
#include "q_string.h"
#include "qcommon_limits.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* CoDUOMP.exe 0x009b7fe0 and coduo_lnxded 0x0828aea0.  Linux's
 * NET_BaseAdrToString is an additional consumer of this same static-return
 * storage; retaining one shared object preserves that overwrite behavior. */
char com_netadrString[NET_ADDRESS_STRING_SIZE];

/*
 * The original address formatters agree on the three address-class paths,
 * 64-byte static return buffer, byte promotion, signed host-order port, and
 * exact format strings:
 *
 *   CoDUOMP.exe   0x0044dde0..0x0044deb0
 *   coduo_lnxded  0x080848f8..0x080849fd
 */
const char *NET_AdrToString(netadr_t address)
{
    int16_t port;

    if (address.type == NA_LOOPBACK) {
        Com_sprintf(com_netadrString, sizeof(com_netadrString), "loopback");
    } else if (address.type == NA_IP) {
        port = BigShort((int16_t)address.port);
        Com_sprintf(com_netadrString, sizeof(com_netadrString), "%i.%i.%i.%i:%i", address.ip[0], address.ip[1], address.ip[2],
                    address.ip[3], port);
    } else {
        port = BigShort((int16_t)address.port);
        Com_sprintf(com_netadrString, sizeof(com_netadrString), "%02x%02x%02x%02x.%02x%02x%02x%02x%02x%02x:%i", address.ipx[0],
                    address.ipx[1], address.ipx[2], address.ipx[3], address.ipx[4], address.ipx[5], address.ipx[6], address.ipx[7],
                    address.ipx[8], address.ipx[9], port);
    }

    return com_netadrString;
}

/*
 * The retained parsers have the same externally visible behavior.  The
 * Windows optimized body is at CoDUOMP.exe 0x0044e420..0x0044e547; the Linux
 * body is at coduo_lnxded 0x08084fc8..0x0808512f.  Windows inlines its bounded
 * copy while Linux calls Q_strncpyz; both copy at most 1023 bytes and always
 * terminate the 1024-byte local array.  strchr(text, ':') and Linux's
 * strstr(text, ":") identify the same first delimiter.
 */
qboolean NET_StringToAdr(const char *text, netadr_t *address)
{
    char copy[MAX_STRING_CHARS];
    char *portText;

    if (strcmp(text, "localhost") == 0) {
        memset(address, 0, sizeof(*address));
        address->type = NA_LOOPBACK;
        return qtrue;
    }

    Q_strncpyz(copy, text, (int32_t)sizeof(copy));
    portText = strchr(copy, ':');
    if (portText != NULL) {
        *portText = '\0';
        ++portText;
    }

    if (Sys_StringToAdr(copy, address) == qfalse ||
        (address->ip[0] == UINT8_MAX && address->ip[1] == UINT8_MAX && address->ip[2] == UINT8_MAX && address->ip[3] == UINT8_MAX)) {
        address->type = NA_BOT;
        return qfalse;
    }

    address->port = (uint16_t)BigShort((int16_t)(portText != NULL ? atoi(portText) : NET_DEFAULT_PORT));
    return qtrue;
}
