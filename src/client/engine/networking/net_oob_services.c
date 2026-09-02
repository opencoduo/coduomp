#include "net_oob_services.h"

#include "net_address.h"
#include "../client/cgame.h"
#include "../server/server.h"

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for CoDUOMP's client-or-server
 * OOB profile selection embedded at the end of all three constructors. */
void net_compat_profile_oob_packet(netsrc_t source, int32_t length)
{
    if (net_profile->integer != 0) {
        netProfileInfo_t **profile = source == NS_SERVER ? &svs.netProfile : &clc.netProfile;
        NetProf_PrepProfiling(profile);
        NetProf_AddPacket(&(*profile)->send, length, qfalse);
    }
}
