#ifndef QCOMMON_NET_TEXT_H
#define QCOMMON_NET_TEXT_H

#include "net_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NET_ADDRESS_STRING_SIZE = 64,
    NET_DEFAULT_PORT = 28960
};

/* NET_AdrToString and the Linux-only NET_BaseAdrToString deliberately share
 * this one original return buffer. */
extern char com_netadrString[NET_ADDRESS_STRING_SIZE];

const char *NET_AdrToString(netadr_t address);
qboolean NET_StringToAdr(const char *text, netadr_t *address);

/* Target socket transport consumed by the common connectionless-packet
 * constructors. */
void NET_SendPacket(netsrc_t source, int32_t length, const void *data,
                    netadr_t address);
void NET_OutOfBandPrint(netsrc_t source, netadr_t address,
                        const char *format, ...);
void NET_OutOfBandData(netsrc_t source, netadr_t address,
                       const uint8_t *data, int32_t length);
void NET_OutOfBandPbPacket(netsrc_t source, netadr_t address,
                           const void *data, int32_t length);

/* Platform resolver boundary used by NET_StringToAdr. */
qboolean Sys_StringToAdr(const char *name, netadr_t *address);

#ifdef __cplusplus
}
#endif

#endif
