#include "server_packet.h"

#include "qcommon/com_event_queue.h"
#include "qcommon/com_redirect.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/com_string.h"
#include "animation/dobj.h"
#include "filesystem/filesystem.h"
#include "qcommon/huffman.h"
#include "qcommon/hunk.h"
#include "qcommon/info.h"
#include "qcommon/net_compare.h"
#include "qcommon/net_text.h"
#include "qcommon/netchan.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_string.h"
#include "server_authorize.h"
#include "server_client_message.h"
#include "server_connect.h"
#include "server_master.h"
#include "server_netchan.h"
#include "server_packet_services.h"
#include "qcommon/server_runtime_types.h"

#include <stdlib.h>
#include <string.h>

enum {
    SERVER_STATUS_INFO_BUFFER_SIZE = MAX_STRING_CHARS,
    SERVER_STATUS_LINE_BUFFER_SIZE = MAX_STRING_CHARS,
    SERVER_STATUS_TEXT_APPEND_LIMIT = MAX_MSGLEN - 1,
    SERVER_STATUS_INFO_CVAR_FLAGS = CVAR_SERVERINFO | CVAR_SCRIPT_SETCVAR_SERVERINFO,
    SERVER_RCON_OUTPUT_CHUNK_SIZE = 1300,
    SERVER_RCON_REDIRECT_BUFFER_SIZE = MAX_MSGLEN - 16,
    SERVER_RCON_COMMAND_BUFFER_SIZE = MAX_STRING_CHARS,
    SERVER_RCON_THROTTLE_MSEC = 500,
    SERVER_CONNECTIONLESS_MARKER_SIZE = sizeof(int32_t),
    SERVER_CONNECT_COMPRESSED_PAYLOAD_OFFSET = 12,
    SERVER_PUNKBUSTER_PREFIX_LENGTH = sizeof("pb_") - 1,
    SERVER_CONNECT_PREFIX_LENGTH = sizeof("connect") - 1
};

/* CoDUOMP.exe 0x0389fdc0 and coduo_lnxded 0x080f5108. The first rcon packet
 * is never throttled because zero is the sentinel for an uninitialized
 * prior-command time. */
static uint32_t sv_lastRemoteCommandTime;

extern serverStatic_t svs;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_disableClientConsole;
extern cvar_t *sv_pure;
extern cvar_t *fs_basegame;
extern cvar_t *sv_privateClients;
extern cvar_t *sv_hostname;
extern cvar_t *sv_mapname;
extern cvar_t *g_gametype;
extern cvar_t *sv_minPing;
extern cvar_t *sv_maxPing;
extern cvar_t *sv_allowAnonymous;
extern cvar_t *sv_punkbuster;
extern cvar_t *rconPassword;
extern cvar_t *sv_packet_info;
extern qboolean sv_frameRunning;

void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
/*
 * Complete server query, RCON, connectionless-packet, and sequenced-packet
 * cluster shared by the Windows client engine and Linux dedicated engine.
 * The supporting Mac client exports all seven canonical names.
 *
 * Function                    Windows       Linux
 * SVC_Status                  0x00461190    0x08093316
 * SVC_GameCompleteStatus      0x004614e0    0x08093702
 * SVC_Info                    0x004616f0    0x0809392e
 * SV_FlushRedirect            0x00461ff0    0x08093f49
 * SVC_RemoteCommand           0x004620d0    0x08094031
 * SV_ConnectionlessPacket     0x004623c0    0x080942c3
 * SV_PacketEvent              0x00462ab0    0x080949fb
 *
 * Query construction, throttling, redirect chunking, dispatch, channel
 * matching, decode, acknowledgement, and cleanup agree. The server-browser
 * `hw` value and the optional PunkBuster packet consumer are the only genuine
 * target-owned edges and remain in server_packet_services.h.
 */

/* Source: CoDUOMP.exe 0x00461190..0x004614d8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00461190_004614d8.mcode.
 * Name: exact same-module Mac symbol SVC_Status. */
void SVC_Status(netadr_t from)
{
    char info[SERVER_STATUS_INFO_BUFFER_SIZE];
    char line[SERVER_STATUS_LINE_BUFFER_SIZE];
    char restrictedKeywords[SERVER_STATUS_LINE_BUFFER_SIZE];
    char status[MAX_MSGLEN];

    strcpy(info, Cvar_InfoString(SERVER_STATUS_INFO_CVAR_FLAGS));
    Info_SetValueForKey(info, "challenge", Cmd_Argv(1));

    cvar_t *const fsRestrict = Cvar_FindVar("fs_restrict");
    if (fsRestrict != NULL && fsRestrict->value != 0.0f) {
        Com_sprintf(restrictedKeywords, sizeof(restrictedKeywords), "demo %s", Info_ValueForKey(info, "sv_keywords"));
        Info_SetValueForKey(info, "sv_keywords", restrictedKeywords);
    }

    status[0] = '\0';
    int32_t statusLength = 0;
    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state < CS_CONNECTED)
            continue;

        Com_sprintf(line, sizeof(line), "%i %i \"%s\"\n", SV_GetClientScore(client), client->ping, client->name);
        const int32_t lineLength = (int32_t)strlen(line);
        if (statusLength + lineLength > SERVER_STATUS_TEXT_APPEND_LIMIT)
            break;

        strcpy(status + statusLength, line);
        statusLength += lineLength;
    }

    if (sv_disableClientConsole->integer != 0) {
        Info_SetValueForKey(info, "con_disabled", va("%i", sv_disableClientConsole->integer));
    }

    cvar_t *const password = Cvar_FindVar("g_password");
    Info_SetValueForKey(info, "pswrd", password != NULL && password->string != NULL && password->string[0] != '\0' ? "1" : "0");

    qboolean reportsMod = qfalse;
    cvar_t *const fsGameCvar = Cvar_FindVar("fs_game");
    const char *const fsGame = fsGameCvar != NULL ? fsGameCvar->string : NULL;
    if (sv_pure->integer == 0 || (fsGame != NULL && fsGame[0] != '\0')) {
        reportsMod = qtrue;
    } else {
        cvar_t *const referencedPakNames = Cvar_FindVar("sv_referencedPakNames");
        if (referencedPakNames != NULL && referencedPakNames->string != NULL && referencedPakNames->string[0] != '\0') {
            Cmd_TokenizeString(referencedPakNames->string);
            const int32_t pakCount = Cmd_Argc();
            for (int32_t pakIndex = 0; pakIndex < pakCount; ++pakIndex) {
                if (FS_idPak(Cmd_Argv(pakIndex), "main", fs_basegame->string) == qfalse) {
                    reportsMod = qtrue;
                    break;
                }
            }
        }
    }
    Info_SetValueForKey(info, "mod", va("%i", reportsMod));

    NET_OutOfBandPrint(NS_SERVER, from, "statusResponse\n%s\n%s", info, status);
}

/* Source: CoDUOMP.exe 0x004614e0..0x004616e3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004614e0_004616e3.mcode.
 * Name: exact same-module Mac symbol SVC_GameCompleteStatus. */
void SVC_GameCompleteStatus(netadr_t from)
{
    char info[SERVER_STATUS_INFO_BUFFER_SIZE];
    char line[SERVER_STATUS_LINE_BUFFER_SIZE];
    char restrictedKeywords[SERVER_STATUS_LINE_BUFFER_SIZE];
    char status[MAX_MSGLEN];

    strcpy(info, Cvar_InfoString(SERVER_STATUS_INFO_CVAR_FLAGS));
    Info_SetValueForKey(info, "challenge", Cmd_Argv(1));

    cvar_t *const fsRestrict = Cvar_FindVar("fs_restrict");
    if (fsRestrict != NULL && fsRestrict->value != 0.0f) {
        Com_sprintf(restrictedKeywords, sizeof(restrictedKeywords), "demo %s", Info_ValueForKey(info, "sv_keywords"));
        Info_SetValueForKey(info, "sv_keywords", restrictedKeywords);
    }

    status[0] = '\0';
    int32_t statusLength = 0;
    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state < CS_CONNECTED)
            continue;

        Com_sprintf(line, sizeof(line), "%i %i \"%s\"\n", SV_GetClientScore(client), client->ping, client->name);
        const int32_t lineLength = (int32_t)strlen(line);
        if (statusLength + lineLength > SERVER_STATUS_TEXT_APPEND_LIMIT)
            break;

        strcpy(status + statusLength, line);
        statusLength += lineLength;
    }

    NET_OutOfBandPrint(NS_SERVER, from, "gameCompleteStatus\n%s\n%s", info, status);
}

/* Source: CoDUOMP.exe 0x004616f0..0x00461fee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004616f0_00461fee.mcode.
 * Name: exact same-module Mac symbol SVC_Info. The repeated Cvar_FindVar and
 * case-insensitive hash-chain walks in the Windows instructions are expressed
 * through the maintained cvar API. */
void SVC_Info(netadr_t from)
{
    int32_t privateClientCount = 0;
    int32_t clientNum = 0;
    for (; clientNum < sv_privateClients->integer; ++clientNum) {
        if (svs.clients[clientNum].state >= CS_CONNECTED)
            ++privateClientCount;
    }

    int32_t connectedClientCount = privateClientCount;
    for (; clientNum < sv_maxclients->integer; ++clientNum) {
        if (svs.clients[clientNum].state >= CS_CONNECTED)
            ++connectedClientCount;
    }

    char info[SERVER_STATUS_INFO_BUFFER_SIZE];
    info[0] = '\0';
    Info_SetValueForKey(info, "challenge", Cmd_Argv(1));
    Info_SetValueForKey(info, "protocol", va("%i", SERVER_PROTOCOL_VERSION));
    Info_SetValueForKey(info, "hostname", sv_hostname->string);
    Info_SetValueForKey(info, "mapname", sv_mapname->string);

    if (connectedClientCount != 0) {
        Info_SetValueForKey(info, "clients", va("%i", connectedClientCount));
    }

    const int32_t publicClientSlots = sv_maxclients->integer - sv_privateClients->integer + privateClientCount;
    if (publicClientSlots > 0) {
        Info_SetValueForKey(info, "sv_maxclients", va("%i", publicClientSlots));
    }

    Info_SetValueForKey(info, "gametype", g_gametype->string);
    if (sv_pure->integer != 0)
        Info_SetValueForKey(info, "pure", va("%i", sv_pure->integer));
    if (sv_minPing->integer != 0) {
        Info_SetValueForKey(info, "minPing", va("%i", sv_minPing->integer));
    }
    if (sv_maxPing->integer != 0) {
        Info_SetValueForKey(info, "maxPing", va("%i", sv_maxPing->integer));
    }

    cvar_t *cvar = Cvar_FindVar("fs_game");
    const char *const fsGame = cvar != NULL && cvar->string != NULL ? cvar->string : "";
    if (fsGame[0] != '\0')
        Info_SetValueForKey(info, "game", fsGame);

    if (sv_allowAnonymous->integer != 0) {
        Info_SetValueForKey(info, "sv_allowAnonymous", va("%i", sv_allowAnonymous->integer));
    }

    cvar = Cvar_FindVar("g_password");
    if (cvar != NULL && cvar->string != NULL && cvar->string[0] != '\0') {
        Info_SetValueForKey(info, "pswrd", "1");
    }

    cvar = Cvar_FindVar("scr_friendlyfire");
    const char *value = cvar != NULL && cvar->string != NULL ? cvar->string : "";
    if (atoi(value) != 0)
        Info_SetValueForKey(info, "ff", value);

    cvar = Cvar_FindVar("scr_killcam");
    value = cvar != NULL && cvar->string != NULL ? cvar->string : "";
    if (atoi(value) != 0)
        Info_SetValueForKey(info, "kc", value);

    cvar = Cvar_FindVar("g_timeoutsallowed");
    value = cvar != NULL && cvar->string != NULL ? cvar->string : "";
    if (atoi(value) != 0)
        Info_SetValueForKey(info, "timeoutsAllowed", value);

    cvar = Cvar_FindVar("scr_allow_jeeps");
    value = cvar != NULL && cvar->string != NULL ? cvar->string : "";
    if (atoi(value) != 0)
        Info_SetValueForKey(info, "jps", value);

    cvar = Cvar_FindVar("scr_allow_tanks");
    value = cvar != NULL && cvar->string != NULL ? cvar->string : "";
    if (atoi(value) != 0)
        Info_SetValueForKey(info, "tnk", value);

    const int32_t hardware = server_compat_info_hardware();
    Info_SetValueForKey(info, "hw", va("%i", hardware));
    Info_SetValueForKey(info, "pb", va("%i", sv_punkbuster->integer));

    qboolean reportsMod = qfalse;
    if (sv_pure->integer == 0 || fsGame[0] != '\0') {
        reportsMod = qtrue;
    } else {
        cvar_t *const referencedPakNames = Cvar_FindVar("sv_referencedPakNames");
        if (referencedPakNames != NULL && referencedPakNames->string != NULL && referencedPakNames->string[0] != '\0') {
            cvar_t *const baseGameCvar = Cvar_FindVar("fs_basegame");
            const char *const baseGame = baseGameCvar != NULL && baseGameCvar->string != NULL ? baseGameCvar->string : "";

            Cmd_TokenizeString(referencedPakNames->string);
            const int32_t pakCount = Cmd_Argc();
            for (int32_t pakIndex = 0; pakIndex < pakCount; ++pakIndex) {
                if (FS_idPak(Cmd_Argv(pakIndex), "main", baseGame) == qfalse) {
                    reportsMod = qtrue;
                    break;
                }
            }
        }
    }
    Info_SetValueForKey(info, "mod", va("%i", reportsMod));

    NET_OutOfBandPrint(NS_SERVER, from, "infoResponse\n%s", info);
}

/* Source: CoDUOMP.exe 0x00461ff0..0x004620ce.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00461ff0_004620cf.mcode.
 * Name and text argument: exact same-module Mac symbol SV_FlushRedirect. */
void SV_FlushRedirect(char *text)
{
    int32_t length = (int32_t)strlen(text);
    while (length > SERVER_RCON_OUTPUT_CHUNK_SIZE) {
        const char saved = text[SERVER_RCON_OUTPUT_CHUNK_SIZE];
        text[SERVER_RCON_OUTPUT_CHUNK_SIZE] = '\0';
        NET_OutOfBandPrint(NS_SERVER, svs.redirectAddress, "print\n%s", text);
        length -= SERVER_RCON_OUTPUT_CHUNK_SIZE;
        text += SERVER_RCON_OUTPUT_CHUNK_SIZE;
        *text = saved;
    }

    NET_OutOfBandPrint(NS_SERVER, svs.redirectAddress, "print\n%s", text);
}

/* Source: CoDUOMP.exe 0x004620d0..0x004623b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004620d0_004623b4.mcode.
 * Name and by-value address argument: exact same-module Mac symbol
 * SVC_RemoteCommand. Com_BeginRedirect/Com_EndRedirect express the three
 * redirect globals whose source helpers the Windows compiler inlined here. */
void SVC_RemoteCommand(netadr_t from)
{
    char redirectBuffer[SERVER_RCON_REDIRECT_BUFFER_SIZE];
    char commandBuffer[SERVER_RCON_COMMAND_BUFFER_SIZE];

    const uint32_t now = Com_Milliseconds();
    const int32_t elapsed = (int32_t)(now - sv_lastRemoteCommandTime);
    if (sv_lastRemoteCommandTime != 0 && elapsed < SERVER_RCON_THROTTLE_MSEC) {
        return;
    }
    sv_lastRemoteCommandTime = now;

    qboolean validPassword = qfalse;
    if (rconPassword->string[0] != '\0' && strcmp(Cmd_Argv(1), rconPassword->string) == 0) {
        validPassword = qtrue;
        Com_Printf("Rcon from %s:\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
    } else {
        Com_Printf("Bad rcon from %s:\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
    }

    svs.redirectAddress = from;
    Com_BeginRedirect(redirectBuffer, SERVER_RCON_REDIRECT_BUFFER_SIZE, SV_FlushRedirect);

    if (rconPassword->string[0] == '\0') {
        Com_Printf("No rconpassword set on the server.\n");
    } else if (validPassword == qfalse) {
        Com_Printf("Bad rconpassword.\n");
    } else {
        int32_t commandLength = 0;
        const int32_t argumentCount = Cmd_Argc();
        for (int32_t argument = 2; argument < argumentCount; ++argument) {
            commandLength = Com_AddToString(Cmd_Argv(argument), commandBuffer, commandLength, SERVER_RCON_COMMAND_BUFFER_SIZE, qtrue);
            commandLength = Com_AddToString(" ", commandBuffer, commandLength, SERVER_RCON_COMMAND_BUFFER_SIZE, qfalse);
        }

        if (commandLength < SERVER_RCON_COMMAND_BUFFER_SIZE) {
            commandBuffer[commandLength] = '\0';
            Cmd_ExecuteString(commandBuffer);
            if (Q_stricmpn(commandBuffer, "pb_sv_", 6) == 0)
                PB_CallServerSaCommandDrain();
        }
    }

    Com_EndRedirect();
}

/* Source: CoDUOMP.exe 0x004623c0..0x004628d2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004623c0_004628d3.mcode.
 * Name and arguments: exact same-module Mac symbol
 * SV_ConnectionlessPacket. The target adapter retains Linux's in-process
 * PunkBuster packet consumer and the retired Windows boundary documented in
 * coduomp/PUNKBUSTER_BOUNDARY.md. Both consume that packet class before
 * normal connectionless parsing, as in the originals. */
void SV_ConnectionlessPacket(netadr_t from, msg_t *message)
{
    message->readcount = 0;
    message->bit = 0;
    if (message->cursize >= SERVER_CONNECTIONLESS_MARKER_SIZE)
        message->readcount = SERVER_CONNECTIONLESS_MARKER_SIZE;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (net_profile->integer != 0) {
        NetProf_PrepProfiling(&svs.netProfile);
        NetProf_AddPacket(&svs.netProfile->send, message->cursize, qfalse);
    }

    const char *const packetData = (const char *)message->data + SERVER_CONNECTIONLESS_MARKER_SIZE;
    const int32_t payloadLength = message->cursize - SERVER_CONNECTIONLESS_MARKER_SIZE;

    /* NOT_FROM_ORIGINAL_SOURCE: gate every prefix comparison by the received
     * payload length; packet contents and protocol bytes remain unchanged. */
    if (payloadLength >= SERVER_PUNKBUSTER_PREFIX_LENGTH && Q_stricmpn(packetData, "pb_", SERVER_PUNKBUSTER_PREFIX_LENGTH) == 0) {
        server_compat_handle_pb_packet(from, message);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: a failed transform leaves its result
     * unpublished, so discard it before command tokenization. */
    if (payloadLength >= SERVER_CONNECT_PREFIX_LENGTH && Q_strncmp(packetData, "connect", SERVER_CONNECT_PREFIX_LENGTH) == 0 &&
        Huff_Decompress(message, SERVER_CONNECT_COMPRESSED_PAYLOAD_OFFSET) == qfalse) {
        return;
    }

    const char *const commandText = MSG_ReadStringLine(message);
    Cmd_TokenizeString(commandText);
    const char *const command = Cmd_Argv(0);

    if (sv_packet_info->integer != 0) {
        Com_Printf("SV packet %s : %s\n", NET_AdrToString(from), command);
    }

    if (Q_stricmp(command, "getstatus") == 0) {
        SVC_Status(from);
    } else if (Q_stricmp(command, "getinfo") == 0) {
        SVC_Info(from);
    } else if (Q_stricmp(command, "getchallenge") == 0) {
        SV_GetChallenge(from);
    } else if (Q_stricmp(command, "connect") == 0) {
        PB_InvokeEventCallback(NET_IsLocalAddress(from) != qfalse ? "localhost" : NET_AdrToString(from), message->data);
        SV_DirectConnect(from);
    } else if (Q_stricmp(command, "ipAuthorize") == 0) {
        SV_AuthorizeIpPacket(from);
    } else if (Q_stricmp(command, "rcon") == 0) {
        SVC_RemoteCommand(from);
    } else if (Q_stricmp(command, "disconnect") != 0) {
        Com_DPrintf("bad connectionless packet from %s:\n%s\n", NET_AdrToString(from), commandText);
    }
}

/* Source: CoDUOMP.exe 0x00462ab0..0x00462d46.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00462ab0_00462d47.mcode.
 * Name and by-value address/message arguments: exact same-module Mac symbol
 * SV_PacketEvent. Address matching deliberately ignores the UDP port until
 * the translated-port repair below; qport selects the client channel. */
void SV_PacketEvent(netadr_t from, msg_t *message)
{
    if (message->cursize >= SERVER_CONNECTIONLESS_MARKER_SIZE) {
        int32_t marker;
        memcpy(&marker, message->data, sizeof(marker));
        if (marker == -1) {
            SV_ConnectionlessPacket(from, message);
            return;
        }
    }

    dobj_skelCacheKey = (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
    if (dobj_skelCacheKey == 0)
        ++dobj_skelCacheKey;

    sv_frameRunning = qtrue;
    message->readcount = 0;
    message->bit = 0;
    if (message->cursize >= SERVER_CONNECTIONLESS_MARKER_SIZE)
        message->readcount = SERVER_CONNECTIONLESS_MARKER_SIZE;
    const int32_t qport = MSG_ReadShort(message) & 0xffff;

    client_t *client = svs.clients;
    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
        if (client->state == CS_FREE || NET_CompareBaseAdr(from, client->netchan.remoteAddress) == qfalse ||
            client->netchan.qport != qport) {
            continue;
        }

        if (client->netchan.remoteAddress.port != from.port) {
            Com_Printf("SV_ReadPackets: fixing up a translated port\n");
            client->netchan.remoteAddress.port = from.port;
        }

        if (Netchan_Process(&client->netchan, message) != qfalse) {
            client->serverId = MSG_ReadByte(message);
            client->messageAcknowledge = MSG_ReadLong(message);
            if (client->messageAcknowledge >= 0) {
                client->reliableAcknowledge = MSG_ReadLong(message);
                const int32_t reliableBacklog = (int32_t)((uint32_t)client->reliableSequence - (uint32_t)client->reliableAcknowledge);
                if (reliableBacklog < MAX_RELIABLE_COMMANDS) {
                    SV_Netchan_Decode(client, message->data + message->readcount, message->cursize - message->readcount);
                    if (client->state != CS_ZOMBIE) {
                        client->lastPacketTime = svs.realTime;
                        SV_ExecuteClientMessage(client, message);
                    }
                } else {
                    client->reliableAcknowledge = client->reliableSequence;
                }
            }
        }

        sv_frameRunning = qfalse;
        Hunk_ClearTempMemory();
        return;
    }

    NET_OutOfBandPrint(NS_SERVER, from, "disconnect");
    sv_frameRunning = qfalse;
    Hunk_ClearTempMemory();
}
