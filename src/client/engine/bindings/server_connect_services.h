#ifndef CODUOMP_SERVER_CONNECT_SERVICES_H
#define CODUOMP_SERVER_CONNECT_SERVICES_H

#include "qcommon/net_compare.h"
#include "qcommon/net_text.h"
#include "qcommon/server_punkbuster_types.h"

#include <stdint.h>

extern serverPbState_t sv_pbServerState;

/* NOT_FROM_ORIGINAL_SOURCE: bind the shared connect routine to the Windows
 * client's external PunkBuster callback slot. */
static inline const char *server_compat_pb_connect_query(netadr_t from, int32_t enabled, const char *guid)
{
    if (sv_pbServerState.stringQueryCallback == NULL) {
        return NULL;
    }

    const char *const address = NET_IsLocalAddress(from) != qfalse ? "localhost" : NET_AdrToString(from);
    return sv_pbServerState.stringQueryCallback(&sv_pbServerState, address, (intptr_t)enabled, (intptr_t)guid);
}

#endif
