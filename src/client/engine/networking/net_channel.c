#include "net_channel.h"
#include "../client/cgame.h"

#include <string.h>


/* Original network-control globals are populated by Netchan_Init. The
 * contiguous cvar slots contain net_profile, net_lanauthorize, showdrop, and
 * net_showprofile in that order; NetProf_PrepProfiling maintains the separate
 * active-mode slot. */
cvar_t *net_lanauthorize;
cvar_t *showdrop;
cvar_t *net_showprofile;
cvar_t *net_qport;
netProfileMode_t net_profileActiveMode;

/* Source: CoDUOMP.exe 0x0044d450..0x0044d46f.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0044d450_0044d470.mcode.
 * Name: exact same-module Mac symbol Net_DisplayProfile. */
void Net_DisplayProfile(void)
{
    if (net_profileActiveMode == NET_PROFILE_OFF)
        return;

    if (net_profileActiveMode == NET_PROFILE_CLIENT)
        CL_Netchan_PrintProfileStats(qfalse);
    else
        SV_Netchan_PrintProfileStats(qfalse);
}

/* Source: CoDUOMP.exe 0x00417520..0x00417598.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417520_00417599.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * CL_Netchan_SendOOBPacket. The diagnostic verifies the caller supplied the
 * four-byte -1 connectionless marker; the original still sends malformed data
 * after reporting it. */
void CL_Netchan_SendOOBPacket(netadr_t address, const void *data,
                              int32_t length)
{
    int32_t marker;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("CL_Netchan_SendOOBPacket: invalid packet length %i\n", length);
        return;
    }
    if (length < (int32_t)sizeof(marker)) {
        Com_Printf(
            "CL_Netchan_SendOOBPacket used to send non-OOB packet.\n");
    } else {
        memcpy(&marker, data, sizeof(marker));
        if (marker != -1) {
            Com_Printf(
                "CL_Netchan_SendOOBPacket used to send non-OOB packet.\n");
        }
    }

    NetProf_PrepProfiling(&clc.netProfile);
    NET_SendPacket(NS_CLIENT, length, data, address);

    if (net_profile->integer != 0) {
        NetProf_PrepProfiling(&clc.netProfile);
        NetProf_AddPacket(&clc.netProfile->send, length, qfalse);
    }
}
