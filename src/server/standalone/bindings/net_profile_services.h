#ifndef ENGINE_NET_PROFILE_SERVICES_H
#define ENGINE_NET_PROFILE_SERVICES_H

#include "qcommon/netchan.h"
#include "server/engine/server_netchan.h"

/* The dedicated engine has no client netchan. */
#define NET_PROFILE_DUMP_STATS() \
    SV_Netchan_PrintProfileStats(qtrue)

#endif
