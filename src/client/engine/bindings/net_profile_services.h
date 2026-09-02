#ifndef CODUOMP_NET_PROFILE_SERVICES_H
#define CODUOMP_NET_PROFILE_SERVICES_H

#include "qcommon/netchan.h"
#include "server/engine/server_netchan.h"

void CL_Netchan_PrintProfileStats(qboolean printHeader);

/* CoDUOMP can profile either half of its listen-client engine. */
#define NET_PROFILE_DUMP_STATS()                                      \
    do {                                                              \
        if (net_profileActiveMode == NET_PROFILE_CLIENT) {            \
            CL_Netchan_PrintProfileStats(qtrue);                       \
        } else {                                                      \
            SV_Netchan_PrintProfileStats(qtrue);                       \
        }                                                             \
    } while (0)

#endif
