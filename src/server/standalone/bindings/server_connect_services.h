#ifndef CODUO_SERVER_CONNECT_SERVICES_H
#define CODUO_SERVER_CONNECT_SERVICES_H

#include "qcommon/net_compare.h"
#include "qcommon/net_text.h"

#include <stdint.h>

const char *PB_InvokeStringQueryCallback(const char *text, intptr_t arg1, const char *arg2);

/* NOT_FROM_ORIGINAL_SOURCE: bind the shared connect routine to the dedicated
 * engine's in-process PunkBuster query boundary. */
static inline const char *server_compat_pb_connect_query(netadr_t from, int32_t enabled, const char *guid)
{
    const char *const address = NET_IsLocalAddress(from) != qfalse ? "localhost" : NET_AdrToString(from);
    return PB_InvokeStringQueryCallback(address, (intptr_t)enabled, guid);
}

#endif
