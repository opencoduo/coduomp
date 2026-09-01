#include "net_oob_services.h"

#include "server/engine/server_netchan.h"

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated engine profiles only server-source
 * OOB packets; it has no client-side net profile owner. */
void net_compat_profile_oob_packet(netsrc_t source, int32_t length)
{
    if (source == NS_SERVER) {
        SV_Netchan_AddOOBProfilePacket(length);
    }
}
