#include "server_client_message.h"

#include "qcommon/com_sprintf.h"
#include "filesystem/filesystem.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/huffman.h"
#include "qcommon/info.h"
#include "qcommon/msg.h"
#include "qcommon/msg_delta.h"
#include "qcommon/net_compare.h"
#include "qcommon/q_checksum.h"
#include "qcommon/q_command.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_limits.h"
#include "server_authorize.h"
#include "server_client_gamestate.h"
#include "server_client_message_services.h"
#include "server_client_release.h"
#include "server_commands.h"
#include "server_connect.h"
#include "server_download.h"
#include "server_game_data.h"
#include "server_operator_runtime.h"
#include "server_snapshot_send.h"
#include "qcommon/vm_runtime.h"
#include "animation/xanim.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SERVER_DROP_NAME_BUFFER_SIZE = 40,
    SERVER_FS_PAK_FOUND = 1,
    SERVER_MAX_PURE_CHECKSUMS = 1024,
    SERVER_RATE_LOCAL = 99999,
    SERVER_RATE_DEFAULT = 5000,
    SERVER_RATE_MINIMUM = 1000,
    SERVER_RATE_MAXIMUM = 90000,
    SERVER_HANDICAP_MINIMUM = 1,
    SERVER_HANDICAP_MAXIMUM = 100,
    SERVER_HANDICAP_TEXT_MAX_LENGTH = 4,
    SERVER_SNAPS_MINIMUM = 1,
    SERVER_SNAPS_MAXIMUM = 30,
    SERVER_SNAPSHOT_MSEC_DEFAULT = 50,
    SERVER_MSEC_PER_SECOND = 1000,
    SERVER_DEDICATED_SERVER = 2,
    SERVER_CLIENT_COMMAND_FLOOD_DELAY_MSEC = 800,
    SERVER_RELIABLE_COMMAND_HASH_LENGTH = 32,
    SERVER_NEXT_SNAPSHOT_IMMEDIATE = -1,
    SERVER_TEST_CLIENT_NONE = -1,
    SERVER_TEST_CLIENT_CONNECT_BUFFER_SIZE = MAX_STRING_CHARS + 8
};

typedef enum serverClientMessageCommand_e {
    SERVER_CLC_MOVE = 0,
    SERVER_CLC_MOVE_NO_DELTA = 1,
    SERVER_CLC_CLIENT_COMMAND = 2,
    SERVER_CLC_EOF = 3
} serverClientMessageCommand_t;

enum {
    SERVER_CLC_COMMAND_BITS = 2
};

extern serverHeader_t sv;
extern serverStatic_t svs;
extern vm_t *sv_gameVM;
extern cvar_t *cl_running;
extern cvar_t *dedicated;
extern cvar_t *sv_floodProtect;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_pure;
extern cvar_t *sv_showCommands;
extern int32_t sv_reconnectSequence;
extern int32_t sv_serverId;

void Com_DPrintf(const char *format, ...);
void Com_Printf(const char *format, ...);
qboolean Sys_IsLANAddress(netadr_t address);

/*
 * Complete client-message and drop lifecycle shared by the listen-server and
 * dedicated engines. The Windows client and its supporting Mac binary retain
 * the canonical names used here. Linux has the same bodies at:
 *
 *   SV_DropClient             0x0808ba15
 *   PB_DropClient             0x0808bbff
 *   SV_Disconnect_f           0x0808cf08
 *   SV_VerifyPaks_f           0x0808cf23
 *   SV_ResetPureClient_f      0x0808d2b5
 *   SV_UserinfoChanged        0x0808d2c7
 *   SV_UpdateUserinfo_f       0x0808d514
 *   SV_ExecuteClientCommand   0x0808d57a
 *   SV_ClientCommand          0x0808d629
 *   SV_ClientThink            0x0808d829
 *   SV_UserMove               0x0808d8af
 *   SV_ExecuteClientMessage   0x0808dba1
 *   SV_AddTestClient          0x0808de54
 *
 * The corresponding Windows addresses are 0x0045a6b0, 0x0045a8d0, and
 * 0x0045b840..0x0045c710. Target-owned service functions retain the only
 * authored edges: drop-message localization/presentation and the dedicated
 * build's optional external-authorization call.
 */

void SV_DropClient(client_t *client, const char *dropReason)
{
    if (client->state == CS_ZOMBIE) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (dropReason == NULL) {
        dropReason = "";
    }

    client->deferredDropReason = NULL;

    char name[SERVER_DROP_NAME_BUFFER_SIZE];
    strcpy(name, client->name);
    SV_FreeClient(client);
    Com_DPrintf("Going to CS_ZOMBIE for %s\n", name);
    client->state = CS_ZOMBIE;

    if (client->gentity == NULL) {
        for (int32_t challengeNum = 0; challengeNum < MAX_CHALLENGES; ++challengeNum) {
            challenge_t *const challenge = &svs.challenges[challengeNum];
            if (NET_CompareAdr(client->netchan.remoteAddress, challenge->address) != qfalse) {
                challenge->connected = qfalse;
                break;
            }
        }
    }

    server_compat_emit_drop_messages(client, name, dropReason);

    int32_t connectedClientNum;
    for (connectedClientNum = 0; connectedClientNum < sv_maxclients->integer && svs.clients[connectedClientNum].state < CS_CONNECTED;
         ++connectedClientNum) {
    }
    if (connectedClientNum == sv_maxclients->integer) {
        SV_Heartbeat_f();
    }
}

void PB_DropClient(int32_t clientNum, const char *dropReason)
{
    SV_DropClient(&svs.clients[clientNum], dropReason);
}

void SV_Disconnect_f(client_t *client)
{
    SV_DropClient(client, "EXE_DISCONNECTED");
}

void SV_VerifyPaks_f(client_t *client)
{
    int32_t expectedChecksumA = 0;
    int32_t expectedChecksumB = 0;
    int32_t clientChecksums[SERVER_MAX_PURE_CHECKSUMS];
    int32_t loadedChecksums[SERVER_MAX_PURE_CHECKSUMS];
    qboolean valid = FS_FileIsInPAK(FS_ShiftStr("wqaeicogaoraz:80fnn", -2), &expectedChecksumA) == SERVER_FS_PAK_FOUND;

    if (valid != qfalse) {
        valid = FS_FileIsInPAK(FS_ShiftStr("ztdzndrud}=;3iqq", -5), &expectedChecksumB) == SERVER_FS_PAK_FOUND;
    }

    int32_t argumentCount = Cmd_Argc();
    if (valid != qfalse) {
        if (argumentCount < 6) {
            valid = qfalse;
        } else {
            int32_t argumentIndex = 1;
            const char *argument = Cmd_Argv(argumentIndex++);
            if (argument == NULL || argument[0] == '@' || atoi(argument) != expectedChecksumA) {
                valid = qfalse;
            }

            if (valid != qfalse) {
                argument = Cmd_Argv(argumentIndex++);
                if (argument == NULL || argument[0] == '@' || atoi(argument) != expectedChecksumB) {
                    valid = qfalse;
                }
            }

            if (valid != qfalse) {
                argument = Cmd_Argv(argumentIndex++);
                if (argument[0] != '@') {
                    valid = qfalse;
                }
            }

            int32_t clientChecksumCount = 0;
            while (valid != qfalse && argumentIndex < argumentCount) {
                clientChecksums[clientChecksumCount++] = atoi(Cmd_Argv(argumentIndex++));
            }

            const int32_t checksumCount = clientChecksumCount - 1;
            for (int32_t checksumIndex = 0; valid != qfalse && checksumIndex < checksumCount; ++checksumIndex) {
                for (int32_t comparisonIndex = 0; comparisonIndex < checksumCount; ++comparisonIndex) {
                    if (checksumIndex != comparisonIndex && clientChecksums[checksumIndex] == clientChecksums[comparisonIndex]) {
                        valid = qfalse;
                        break;
                    }
                }
            }

            if (valid != qfalse) {
                Cmd_TokenizeString(FS_LoadedPakPureChecksums());
                int32_t loadedChecksumCount = Cmd_Argc();
                if (loadedChecksumCount > SERVER_MAX_PURE_CHECKSUMS) {
                    loadedChecksumCount = SERVER_MAX_PURE_CHECKSUMS;
                }

                for (int32_t checksumIndex = 0; checksumIndex < loadedChecksumCount; ++checksumIndex) {
                    loadedChecksums[checksumIndex] = atoi(Cmd_Argv(checksumIndex));
                }

                for (int32_t checksumIndex = 0; valid != qfalse && checksumIndex < checksumCount; ++checksumIndex) {
                    int32_t loadedIndex;
                    for (loadedIndex = 0;
                         loadedIndex < loadedChecksumCount && clientChecksums[checksumIndex] != loadedChecksums[loadedIndex];
                         ++loadedIndex) {
                    }
                    if (loadedIndex >= loadedChecksumCount) {
                        valid = qfalse;
                    }
                }
            }

            if (valid != qfalse) {
                int32_t checksum = sv.gamestateChecksumFeed;
                for (int32_t checksumIndex = 0; checksumIndex < checksumCount; ++checksumIndex) {
                    checksum ^= clientChecksums[checksumIndex];
                }
                if ((checksum ^ checksumCount) != clientChecksums[checksumCount]) {
                    valid = qfalse;
                }
            }
        }
    }

    client->pureAuthState = valid != qfalse ? SERVER_CLIENT_PURE_STATE_VALID : SERVER_CLIENT_PURE_STATE_INVALID;
}

void SV_ResetPureClient_f(client_t *client)
{
    client->pureAuthState = SERVER_CLIENT_PURE_STATE_PENDING;
}

void SV_UserinfoChanged(client_t *client)
{
    const char *value = Info_ValueForKey(client->userinfo, "name");
    Q_strncpyz(client->name, value, sizeof(client->name));

    /* coduo_lnxded 0x0808d306 calls the full Sys_IsLANAddress body at
     * 0x080ca1bc. The former NET_IsLocalAddress spelling was a transcription
     * error, not a Linux behavior difference. */
    if (Sys_IsLANAddress(client->netchan.remoteAddress) != qfalse && dedicated->integer != SERVER_DEDICATED_SERVER) {
        client->rate = SERVER_RATE_LOCAL;
    } else {
        value = Info_ValueForKey(client->userinfo, "rate");
        if (value[0] == '\0') {
            client->rate = SERVER_RATE_DEFAULT;
        } else {
            client->rate = atoi(value);
            if (client->rate < SERVER_RATE_MINIMUM) {
                client->rate = SERVER_RATE_MINIMUM;
            } else if (client->rate > SERVER_RATE_MAXIMUM) {
                client->rate = SERVER_RATE_MAXIMUM;
            }
        }
    }

    value = Info_ValueForKey(client->userinfo, "handicap");
    if (value[0] != '\0') {
        const int32_t handicap = atoi(value);
        if (handicap < SERVER_HANDICAP_MINIMUM || handicap > SERVER_HANDICAP_MAXIMUM || strlen(value) > SERVER_HANDICAP_TEXT_MAX_LENGTH) {
            Info_SetValueForKey(client->userinfo, "handicap", "100");
        }
    }

    value = Info_ValueForKey(client->userinfo, "snaps");
    if (value[0] == '\0') {
        client->snapshotMsec = SERVER_SNAPSHOT_MSEC_DEFAULT;
    } else {
        int32_t snaps = atoi(value);
        if (snaps < SERVER_SNAPS_MINIMUM) {
            snaps = SERVER_SNAPS_MINIMUM;
        } else if (snaps > SERVER_SNAPS_MAXIMUM) {
            snaps = SERVER_SNAPS_MAXIMUM;
        }
        client->snapshotMsec = SERVER_MSEC_PER_SECOND / snaps;
    }

    client->download.redirectAllowedByClient = qfalse;
    value = Info_ValueForKey(client->userinfo, "cl_wwwDownload");
    if (value[0] != '\0' && atoi(value) != 0) {
        client->download.redirectAllowedByClient = qtrue;
    }
}

void SV_UpdateUserinfo_f(client_t *client)
{
    Q_strncpyz(client->userinfo, Cmd_Argv(1), sizeof(client->userinfo));
    SV_UserinfoChanged(client);

    const int32_t clientNum = (int32_t)(client - svs.clients);
    (void)VM_Call(sv_gameVM, GAME_CLIENT_USERINFO_CHANGED, clientNum);
}

static const sv_client_command_handler_t sv_clientCommandHandlers[] = {{"userinfo", SV_UpdateUserinfo_f},
                                                                       {"disconnect", SV_Disconnect_f},
                                                                       {"cp", SV_VerifyPaks_f},
                                                                       {"vdr", SV_ResetPureClient_f},
                                                                       {"download", SV_BeginDownload_f},
                                                                       {"nextdl", SV_NextDownload_f},
                                                                       {"stopdl", SV_StopDownload_f},
                                                                       {"donedl", SV_DoneDownload_f},
                                                                       {"retransdl", SV_RetransmitDownload_f},
                                                                       {"wwwdl", SV_WWWDownload_f},
                                                                       {NULL, NULL}};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(sv_client_command_handler_t) == 4, "i386 client-command handler alignment changed");
_Static_assert(sizeof(sv_client_command_handler_t) == 0x08, "original client-command handler stride");
_Static_assert(sizeof(sv_clientCommandHandlers) == 0x58, "original client-command handler table extent");
#endif

void SV_ExecuteClientCommand(client_t *client, const char *text, qboolean clientOK)
{
    XAnimSetUser(XANIM_USER_SERVER);
    Cmd_TokenizeString(text);

    const sv_client_command_handler_t *handler = sv_clientCommandHandlers;
    while (handler->commandName != NULL) {
        if (strcmp(Cmd_Argv(0), handler->commandName) == 0) {
            handler->handler(client);
            break;
        }
        ++handler;
    }

    if (clientOK != qfalse && handler->commandName == NULL && sv.state == SS_GAME) {
        const int32_t clientNum = (int32_t)(client - svs.clients);
        (void)VM_Call(sv_gameVM, GAME_CLIENT_COMMAND, clientNum);
    }
}

qboolean SV_ClientCommand(client_t *client, msg_t *message)
{
    const int32_t sequence = MSG_ReadLong(message);
    const char *const command = MSG_ReadString(message);

    if (sequence <= client->lastClientCommand) {
        return qtrue;
    }

    if (sv_showCommands->integer != 0) {
        Com_Printf("clientCommand: %i : %s\n", sequence, command);
    }

    if (sequence > client->lastClientCommand + 1) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Com_Printf("Client %s lost %i clientCommands\n", client->name, sequence - client->lastClientCommand + 1);
        SV_DropClient(client, "EXE_LOSTRELIABLECOMMANDS");
        return qfalse;
    }

    qboolean floodProtect = qtrue;
    if (Q_strncmp("team ", command, 5) == 0 || Q_strncmp("score ", command, 6) == 0 || Q_strncmp("mr ", command, 3) == 0) {
        floodProtect = qfalse;
    }

    qboolean clientOK = qtrue;
    if (cl_running->integer == 0 && client->state > CS_PRIMED && sv_floodProtect->integer != 0 && svs.realTime < client->nextReliableTime &&
        floodProtect != qfalse) {
        clientOK = qfalse;
        Com_DPrintf("client text ignored for %s: %s\n", client->name, Cmd_Argv(0));
    }

    if (floodProtect != qfalse) {
        client->nextReliableTime = svs.realTime + SERVER_CLIENT_COMMAND_FLOOD_DELAY_MSEC;
    }

    SV_ExecuteClientCommand(client, command, clientOK);
    client->lastClientCommand = sequence;
    Com_sprintf(client->lastClientCommandString, sizeof(client->lastClientCommandString), "%s", command);
    return qtrue;
}

void SV_ClientThink(client_t *client, const usercmd_t *command)
{
    client->lastUsercmd = *command;

    if (client->state == CS_ACTIVE) {
        const int32_t clientNum = (int32_t)(client - svs.clients);
        XAnimSetUser(XANIM_USER_SERVER);
        (void)VM_Call(sv_gameVM, GAME_CLIENT_THINK, clientNum);
    }
}

void SV_UserMove(client_t *client, msg_t *message, qboolean delta)
{
    usercmd_t nullCommand;
    usercmd_t commands[MAX_PACKET_USERCMDS];

    client->deltaMessage = delta != qfalse ? client->messageAcknowledge : SERVER_DELTA_MESSAGE_NONE;

    if (client->reliableSequence - client->reliableAcknowledge >= MAX_RELIABLE_COMMANDS) {
        return;
    }

    const int32_t commandCount = MSG_ReadByte(message);
    if (commandCount < 1) {
        Com_Printf("cmdCount < 1\n");
        return;
    }
    if (commandCount > MAX_PACKET_USERCMDS) {
        Com_Printf("cmdCount > MAX_PACKET_USERCMDS\n");
        return;
    }

    uint32_t key = (uint32_t)client->messageAcknowledge ^ (uint32_t)sv.gamestateChecksumFeed;
    key ^= Com_HashKey(client->reliableCommands[client->reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1)].commandText,
                       SERVER_RELIABLE_COMMAND_HASH_LENGTH);

    const int32_t clientNum = (int32_t)(client - svs.clients);
    playerState_t *const gameClient = SV_GameClientNum(clientNum);
    MSG_SetDefaultUserCmd(gameClient, &nullCommand);

    const usercmd_t *from = &nullCommand;
    for (int32_t index = 0; index < commandCount; ++index) {
        MSG_ReadDeltaUsercmdKey(message, key, from, &commands[index]);
        if (VM_Call(sv_gameVM, GAME_IS_VALID_WEAPON, commands[index].weapon) == 0) {
            commands[index].weapon = (uint8_t)gameClient->currentWeapon;
        }
        from = &commands[index];
    }

    /* NOT_FROM_ORIGINAL_SOURCE: a reader overflow publishes readcount beyond
     * cursize; no partially decoded user command may reach gameplay. */
    if (message->readcount > message->cursize) {
        SV_DropClient(client, "malformed user command");
        return;
    }

    client->snapshotFrames[client->messageAcknowledge & (SERVER_CLIENT_SNAPSHOT_FRAME_COUNT - 1)].messageAcknowledgedTime = svs.realTime;

    if (client->state == CS_PRIMED) {
        XAnimSetUser(XANIM_USER_SERVER);
        SV_ClientEnterWorld(client, commands);
    }

    if (sv_pure->integer != 0 && client->pureAuthState == SERVER_CLIENT_PURE_STATE_PENDING) {
        SV_DropClient(client, "EXE_CANNOTVALIDATEPURECLIENT");
        return;
    }

    if (client->state != CS_ACTIVE) {
        client->deltaMessage = SERVER_DELTA_MESSAGE_NONE;
        return;
    }

    const int32_t newestCommandTime = commands[commandCount - 1].commandTime;
    for (int32_t index = 0; index < commandCount; ++index) {
        if (commands[index].commandTime <= newestCommandTime && commands[index].commandTime > client->lastUsercmd.commandTime) {
            SV_ClientThink(client, &commands[index]);
        }
    }
}

void SV_ExecuteClientMessage(client_t *client, msg_t *message)
{
    uint8_t decodedData[MAX_MSGLEN];
    msg_t decoded;
    MSG_Init(&decoded, decodedData, sizeof(decodedData));
    const int32_t decodedSize = MSG_ReadBitsCompress(message->data + message->readcount, decodedData, message->cursize - message->readcount,
                                                     (int32_t)sizeof(decodedData));
    /* NOT_FROM_ORIGINAL_SOURCE: a failed transform may leave partial output;
     * do not publish or parse it as client commands. */
    if (decodedSize == HUFFMAN_TRANSFORM_ERROR) {
        return;
    }
    decoded.cursize = decodedSize;

    if (client->serverId != sv_serverId && client->download.fileName[0] == '\0') {
        if (((uint32_t)client->serverId ^ (uint32_t)sv_serverId) & SERVER_ID_HIGH_MASK) {
            if (client->messageAcknowledge <= client->gamestateMessageNum) {
                return;
            }

            Com_DPrintf("%s : dropped gamestate, resending\n", client->name);
            SV_SendClientGameState(client);
            server_compat_authorize_resent_gamestate(client);
            return;
        }

        if (client->state == CS_PRIMED) {
            XAnimSetUser(XANIM_USER_SERVER);
            SV_ClientEnterWorld(client, &client->lastUsercmd);
        }
        return;
    }

    serverClientMessageCommand_t command = (serverClientMessageCommand_t)MSG_ReadBits(&decoded, SERVER_CLC_COMMAND_BITS);
    while (command != SERVER_CLC_EOF && command == SERVER_CLC_CLIENT_COMMAND) {
        if (SV_ClientCommand(client, &decoded) == qfalse) {
            return;
        }
        if (client->state == CS_ZOMBIE) {
            return;
        }
        command = (serverClientMessageCommand_t)MSG_ReadBits(&decoded, SERVER_CLC_COMMAND_BITS);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: a command selector is valid only when its
     * complete bit field remained inside the decoded message. */
    if (decoded.readcount > decoded.cursize) {
        SV_DropClient(client, "malformed client message");
        return;
    }

    if (sv_pure->integer != 0 && client->pureAuthState == SERVER_CLIENT_PURE_STATE_INVALID) {
        client->nextSnapshotTime = SERVER_NEXT_SNAPSHOT_IMMEDIATE;
        client->state = CS_ACTIVE;
        SV_SendClientSnapshot(client);
        SV_DropClient(client, "EXE_UNPURECLIENTDETECTED");
    }

    if (command == SERVER_CLC_MOVE) {
        SV_UserMove(client, &decoded, qtrue);
    } else if (command == SERVER_CLC_MOVE_NO_DELTA) {
        SV_UserMove(client, &decoded, qfalse);
    } else if (command != SERVER_CLC_EOF) {
        const int32_t clientNum = (int32_t)(client - svs.clients);
        Com_Printf("WARNING: bad command byte %i for client %i\n", command, clientNum);
    }
}

int32_t SV_AddTestClient(void)
{
    int32_t clientNum;
    client_t *client = svs.clients;
    for (clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
        if (client->state == CS_FREE) {
            break;
        }
    }
    if (clientNum == sv_maxclients->integer) {
        return SERVER_TEST_CLIENT_NONE;
    }

    char connectCommand[SERVER_TEST_CLIENT_CONNECT_BUFFER_SIZE];
    sprintf(connectCommand,
            "connect \"\\cl_guid\\unknown\\cg_predictItems\\1"
            "\\cl_punkbuster\\0\\cl_anonymous\\0\\handicap\\100"
            "\\color\\4\\head\\default\\model\\multi\\snaps\\20"
            "\\rate\\5000\\name\\bot%d\\protocol\\%d\"",
            sv_reconnectSequence, SERVER_PROTOCOL_VERSION);
    Cmd_TokenizeString(connectCommand);

    netadr_t address;
    memset(&address, 0, sizeof(address));
    address.port = (uint16_t)sv_reconnectSequence;
    ++sv_reconnectSequence;
    SV_DirectConnect(address);

    client = svs.clients;
    for (clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
        if (client->state != CS_FREE && NET_CompareBaseAdr(client->netchan.remoteAddress, address) != qfalse) {
            break;
        }
    }
    if (clientNum == sv_maxclients->integer) {
        return SERVER_TEST_CLIENT_NONE;
    }

    XAnimSetUser(XANIM_USER_SERVER);
    client->isTestClient = qtrue;
    SV_SendClientGameState(client);

    usercmd_t command;
    memset(&command, 0, sizeof(command));
    SV_ClientEnterWorld(client, &command);
    return clientNum;
}
