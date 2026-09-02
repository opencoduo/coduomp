#include "server_browser.h"

#include "cgame.h"
#include "console.h"
#include "../platform/crt_boundary.h"
#include "qcommon/q_string.h"
#include "../system_platform.h"
#include "../ui/ui_client_state.h"

#include <stdlib.h>
#include <string.h>

/* Original Win32 clientConnection_t at 0x04dc8860..0x04df969b. */
clientConnection_t clc;

/* Autoupdate connection state. CL_CheckAutoUpdate resolves the address and
 * sets the latch at 0x04e19964; the connect response compares that address and
 * the cl_updateavailable control before setting the update-session flag. */
qboolean cls_autoupdateServerResolved;       /* original 0x04e19964 */
qboolean cl_updateStarted;                   /* original 0x0495806c */
qboolean cl_connectedToPureServer;           /* original 0x04958060 */

enum {
    CL_CHALLENGE_IMMEDIATE_RESEND_TIME = -99999,
    CL_CONNECTED_IMMEDIATE_SEND_TIME = -9999,
    CL_AUTOUPDATE_SERVER_COUNT = 5,
    CL_AUTOUPDATE_SERVER_PORT = 28960
};

/* Source: CoDUOMP.exe 0x00413cc0..0x00413e61.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413cc0_00413e62.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_CheckAutoUpdate. The first pass retains only hostnames that resolve, then
 * a randomized start and cyclic fallback choose the server used for the
 * connectionless version query. */
void CL_CheckAutoUpdate(void)
{
    if (cls_autoupdateServerResolved != qfalse)
        return;

    srand(Com_Milliseconds());

    const char *resolvedServerNames[CL_AUTOUPDATE_SERVER_COUNT];
    int32_t resolvedServerCount = 0;
    netadr_t resolvedAddress;

    for (int32_t serverIndex = 0; serverIndex < CL_AUTOUPDATE_SERVER_COUNT; ++serverIndex) {
        if (NET_StringToAdr(cls.autoUpdateServerNames[serverIndex], &resolvedAddress) != qfalse) {
            resolvedServerNames[resolvedServerCount++] = cls.autoUpdateServerNames[serverIndex];
        }
    }

    if (resolvedServerCount == 0) {
        Com_DPrintf("Couldn't resolve an AutoUpdate Server address.\n");
        cls_autoupdateServerResolved = qtrue;
        return;
    }

    const int32_t firstServer = coduo_crt_randrange(0, resolvedServerCount);
    Com_DPrintf("Resolving AutoUpdate Server... \n");

    if (NET_StringToAdr(resolvedServerNames[firstServer], &cls.autoUpdateServer) == qfalse) {
        Com_DPrintf("Couldn't resolve first address, trying others... \n");

        int32_t fallbackOffset;
        for (fallbackOffset = 1; fallbackOffset < resolvedServerCount; ++fallbackOffset) {
            const int32_t serverIndex = (firstServer + fallbackOffset) % resolvedServerCount;
            if (NET_StringToAdr(resolvedServerNames[serverIndex], &cls.autoUpdateServer) != qfalse) {
                Com_DPrintf("Alternate server address resolved... \n");
                break;
            }
        }

        if (fallbackOffset == resolvedServerCount) {
            Com_DPrintf("Failed to resolve any Auto-update servers.\n");
            cls_autoupdateServerResolved = qtrue;
            return;
        }
    }

    cls.autoUpdateServer.port = (uint16_t)BigShort((int16_t)CL_AUTOUPDATE_SERVER_PORT);
    Com_DPrintf("%i.%i.%i.%i:%i", cls.autoUpdateServer.ip[0], cls.autoUpdateServer.ip[1], cls.autoUpdateServer.ip[2],
                cls.autoUpdateServer.ip[3], (int32_t)BigShort((int16_t)cls.autoUpdateServer.port));

    NET_OutOfBandPrint(NS_CLIENT, cls.autoUpdateServer, "getUpdateInfo \"%s\" \"%s\"", "1.51", "win-x86");
    cls_autoupdateServerResolved = qtrue;
}

/* Source: CoDUOMP.exe 0x00413e70..0x00413ee8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413e70_00413ee9.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_GetAutoUpdate. The Windows optimizer inlines Sys_OpenURL here; the Mac
 * body expresses the original source boundary directly as
 * Sys_OpenURL(cl_updateFiles->string, qtrue). */
void CL_GetAutoUpdate(void)
{
    if (cls_autoupdateServerResolved == qfalse)
        return;
    if (cl_updateFiles->string[0] == '\0')
        return;

    Sys_OpenURL(cl_updateFiles->string, qtrue);
}

/* Source: CoDUOMP.exe 0x00415a50..0x00415be8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00415a50_00415be9.mcode.
 * Name and by-value address argument: exact same-module Mac symbol
 * CL_UpdateInfoPacket. The original byte copies around NET_CompareAdr are the
 * i386 calling convention's two by-value netadr_t arguments, not independent
 * address fragments. */
void CL_UpdateInfoPacket(netadr_t address)
{
    if (cls.autoUpdateServer.type == NA_BOT) {
        Com_DPrintf("CL_UpdateInfoPacket:  Auto-Updater has bad address\n");
        return;
    }

    Com_DPrintf("Auto-Updater resolved to %i.%i.%i.%i:%i\n", cls.autoUpdateServer.ip[0], cls.autoUpdateServer.ip[1],
                cls.autoUpdateServer.ip[2], cls.autoUpdateServer.ip[3], (int32_t)BigShort((int16_t)cls.autoUpdateServer.port));

    if (NET_CompareAdr(cls.autoUpdateServer, address) == qfalse) {
        Com_DPrintf("CL_UpdateInfoPacket:  Received packet from "
                    "%i.%i.%i.%i:%i\n",
                    address.ip[0], address.ip[1], address.ip[2], address.ip[3], (int32_t)BigShort((int16_t)address.port));
        return;
    }

    (void)Cvar_Set2("cl_updateavailable", Cmd_Argv(1), qtrue);
    if (cl_updateAvailable->string == NULL || Q_stricmp(cl_updateAvailable->string, "1") != 0) {
        return;
    }

    (void)Cvar_Set2("cl_updatefiles", Cmd_Argv(2), qtrue);
    (void)Cvar_Set2("cl_updateversion", Cmd_Argv(3), qtrue);
    (void)Cvar_Set2("cl_updateoldversion", "1.51", qtrue);
}

/* Source: CoDUOMP.exe 0x00412880..0x004131a6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00412880_004131a7.mcode.
 * Name and arguments: exact same-module Mac symbol CL_ConnectionlessPacket.
 * The embedded PunkBuster implementation originally consumed PB_ packets at
 * the top of this function. Modern builds intentionally reject that optional,
 * retired boundary as documented in coduomp/PUNKBUSTER_BOUNDARY.md. */
void CL_ConnectionlessPacket(netadr_t address, msg_t *message, int32_t packetTime)
{
    message->readcount = 0;
    message->bit = 0;
    if (message->cursize >= (int32_t)sizeof(int32_t))
        message->readcount = (int32_t)sizeof(int32_t);

    if (net_profile->integer != 0) {
        NetProf_PrepProfiling(&clc.netProfile);
        NetProf_AddPacket(&clc.netProfile->send, message->cursize, qfalse);
    }

    if (Q_stricmpn((const char *)message->data + sizeof(int32_t), "PB_", 3) == 0) {
        return;
    }

    const char *packetText = MSG_ReadStringLine(message);
    Cmd_TokenizeString(packetText);
    const char *command = Cmd_Argc() > 0 ? Cmd_Argv(0) : "";

    Com_DPrintf("CL packet %s: %s\n", NET_AdrToString(address), command);

    if (Q_stricmp(command, "challengeResponse") == 0) {
        if (cls.state != CA_CONNECTING) {
            Com_Printf("Unwanted challenge response received.  Ignored.\n");
            return;
        }

        clc.challenge = coduo_crt_atoi(Cmd_Argv(1));
        clc.onlyVisibleClients = Cmd_Argc() > 2 ? coduo_crt_atoi(Cmd_Argv(2)) : qfalse;
        cls.state = CA_CHALLENGING;
        clc.connectPacketCount = 0;
        clc.connectTime = CL_CHALLENGE_IMMEDIATE_RESEND_TIME;
        clc.serverAddress = address;
        Com_DPrintf("challenge: %d\n", clc.challenge);
        return;
    }

    if (Q_stricmp(command, "connectResponse") == 0) {
        if (cls.state >= CA_CONNECTED) {
            Com_Printf("Dup connect received.  Ignored.\n");
            return;
        }
        if (cls.state != CA_CHALLENGING) {
            Com_Printf("connectResponse packet while not connecting.  Ignored.\n");
            return;
        }
        if (NET_CompareBaseAdr(address, clc.serverAddress) == qfalse) {
            Com_Printf("connectResponse from a different address.  Ignored.\n");

            /* NET_AdrToString intentionally uses one static buffer. These
             * sequenced calls retain the original diagnostic's aliasing. */
            const char *expectedAddress = NET_AdrToString(clc.serverAddress);
            const char *receivedAddress = NET_AdrToString(address);
            Com_Printf("%s should have been %s\n", receivedAddress, expectedAddress);
            return;
        }

        if (cls_autoupdateServerResolved != qfalse && NET_CompareAdr(clc.serverAddress, cls.autoUpdateServer) != qfalse &&
            cl_updateAvailable->integer != 0) {
            cl_updateStarted = qtrue;
        }

        const int32_t qport = (int32_t)Cvar_VariableValue("net_qport");
        Netchan_Setup(NS_CLIENT, &clc.netchan, address, qport);
        cls.state = CA_CONNECTED;
        clc.lastPacketTime = cls.realTime;
        clc.lastPacketSentTime = CL_CONNECTED_IMMEDIATE_SEND_TIME;
        return;
    }

    if (Q_stricmp(command, "infoResponse") == 0) {
        CL_ServerInfoPacket(address, message, packetTime);
        return;
    }
    if (Q_stricmp(command, "statusResponse") == 0) {
        CL_ServerStatusResponse(address, message);
        return;
    }
    if (Q_stricmp(command, "disconnect") == 0) {
        CL_DisconnectPacket(address);
        return;
    }
    if (Q_stricmp(command, "echo") == 0) {
        NET_OutOfBandPrint(NS_CLIENT, address, "%s", Cmd_Argv(1));
        return;
    }
    if (Q_stricmp(command, "keyAuthorize") == 0)
        return;
    if (Q_stricmp(command, "motd") == 0) {
        CL_MotdPacket(address);
        return;
    }
    if (Q_stricmp(command, "print") == 0) {
        const char *text = MSG_ReadBigString(message);
        strncpy(clc.serverMessage, text, CL_SERVER_MESSAGE_SIZE - 1);
        clc.serverMessage[CL_SERVER_MESSAGE_SIZE - 1] = '\0';
        Com_PrintMessage(CON_DEST_MINICONSOLE, text);
        return;
    }
    if (Q_stricmp(command, "error") == 0) {
        if (cls.state == CA_DISCONNECTED || NET_CompareBaseAdr(address, clc.serverAddress) == qfalse) {
            return;
        }

        const char *errorText = MSG_ReadBigString(message);
        const char *localizedError = SEH_LocalizeTextMessage(errorText, "server error", LOCMSG_SAFE);
        Com_Error(ERR_DROP, "\x15%s", localizedError);
        return;
    }
    if (Q_stricmp(command, "updateResponse") == 0) {
        CL_UpdateInfoPacket(address);
        return;
    }
    if (strncmp(command, "getserversResponse", 18) == 0) {
        CL_ServersResponsePacket(message);
        return;
    }
    if (strncmp(command, "needcdkey", 9) == 0) {
        strncpy(clc.serverMessage, "EXE_AWAITINGCDKEYAUTH", CL_SERVER_MESSAGE_SIZE - 1);
        clc.serverMessage[CL_SERVER_MESSAGE_SIZE - 1] = '\0';
        (void)SEH_LocalizeTextMessage("EXE_AWAITINGCDKEYAUTH", "need cd key message", LOCMSG_SAFE);
        Com_Printf("%s\n", clc.serverMessage);
        CL_RequestAuthorization();
        return;
    }
    if (Q_stricmp(command, "loadingnewmap") == 0) {
        if (NET_CompareBaseAdr(address, clc.serverAddress) == qfalse)
            return;

        const char *mapName = va("%s", MSG_ReadStringLine(message));
        const char *gameType = MSG_ReadStringLine(message);
        CL_MapLoading(mapName, gameType);
        return;
    }

    Com_DPrintf("Unknown connectionless packet command.\n");
}
