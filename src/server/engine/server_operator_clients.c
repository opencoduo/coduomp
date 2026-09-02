#include "server_operator_clients.h"

#include "qcommon/q_command.h"
#include "qcommon/q_string.h"
#include "server_authorize.h"
#include "server_client_message.h"
#include "server_commands.h"

#include <stddef.h>
#include <stdlib.h>

extern serverStatic_t svs;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_running;

void Com_Printf(const char *format, ...);
/*
 * Complete player-selection and kick/ban operator-command cluster shared by
 * the Windows client engine and Linux dedicated engine.  The supporting Mac
 * client exports the same canonical function names.
 *
 * Function               Windows       Linux
 * SV_GetPlayerByName      0x004573d0    0x08087de8
 * SV_GetPlayerByNum       0x004574f0    0x08087eff
 * SV_KickClient           0x00458060    0x08088aec
 * SV_KickUser_f           0x004580d0    0x08088b8a
 * SV_KickClient_f         0x004581e0    0x08088c9e
 * SV_TempBan_f            0x00458250    0x08088d2b
 * SV_Ban_f                0x004582b0    0x08088d74
 * SV_BanNum_f             0x00458300    0x08088dc8
 * SV_Unban_f              0x00458350    0x08088e1c
 * SV_Drop_f               0x00458380    0x08088e50
 * SV_DropNum_f            0x00458390    0x08088e6c
 * SV_TempBanNum_f         0x004583a0    0x08088e88
 *
 * The bodies agree on the client record stride and offsets, command and state
 * tests, name-cleaning path, loopback protection, drop timing, and GUID-cache
 * updates.  Windows inlines Q_strncpyz in SV_KickClient and inlines the kick
 * body in SV_KickUser_f's "all" loop; Linux emits ordinary calls.  These are
 * compiler realization differences, not source or behavior differences.
 */

client_t *SV_GetPlayerByName(void)
{
    if (sv_running->integer == 0) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No player specified.\n");
        return NULL;
    }

    const char *const name = Cmd_Argv(1);
    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        client_t *const client = &svs.clients[clientNum];

        if (client->state == CS_FREE) {
            continue;
        }

        if (Q_stricmp(client->name, name) == 0) {
            return client;
        }

        char cleanName[SERVER_PLAYER_NAME_BUFFER_SIZE];
        Q_strncpyz(cleanName, client->name, sizeof(cleanName));
        Q_CleanStr(cleanName);
        if (Q_stricmp(cleanName, name) == 0) {
            return client;
        }
    }

    Com_Printf("Player %s is not on the server\n", name);
    return NULL;
}

client_t *SV_GetPlayerByNum(void)
{
    if (sv_running->integer == 0) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No player specified.\n");
        return NULL;
    }

    const char *const slotText = Cmd_Argv(1);
    for (const char *cursor = slotText; *cursor != '\0'; ++cursor) {
        const int8_t digit = (int8_t)*cursor;
        if (digit < '0' || digit > '9') {
            Com_Printf("Bad slot number: %s\n", slotText);
            return NULL;
        }
    }

    const int32_t clientNum = atoi(slotText);
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Printf("Bad client slot: %i\n", clientNum);
        return NULL;
    }

    client_t *const client = &svs.clients[clientNum];
    if (client->state == CS_FREE) {
        Com_Printf("Client %i is not active\n", clientNum);
        return NULL;
    }

    return client;
}

int32_t SV_KickClient(client_t *client, char *nameOut,
                      int32_t nameOutSize)
{
    if (client->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, qfalse,
                             "e \"EXE_CANNOTKICKHOSTPLAYER\"");
        return 0;
    }

    if (nameOut != NULL) {
        Q_strncpyz(nameOut, client->name, nameOutSize);
        Q_CleanStr(nameOut);
    }

    const int32_t guid = client->guid;
    SV_DropClient(client, "EXE_PLAYERKICKED");
    client->lastPacketTime = svs.realTime;
    return guid;
}

int32_t SV_KickUser_f(char *nameOut, int32_t nameOutSize)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return 0;
    }

    if (Cmd_Argc() != 2) {
        const char *const command = Cmd_Argv(0);
        Com_Printf("Usage: %s <player name>\n%s all = kick everyone\n",
                   command, command);
        return 0;
    }

    client_t *const client = SV_GetPlayerByName();
    if (client != NULL) {
        return SV_KickClient(client, nameOut, nameOutSize);
    }

    if (Q_stricmp(Cmd_Argv(1), "all") == 0) {
        for (int32_t clientNum = 0;
             clientNum < sv_maxclients->integer;
             ++clientNum) {
            client_t *const candidate = &svs.clients[clientNum];
            if (candidate->state != CS_FREE) {
                (void)SV_KickClient(candidate, NULL, 0);
            }
        }
    }
    return 0;
}

int32_t SV_KickClient_f(char *nameOut, int32_t nameOutSize)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return 0;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <client number>\n", Cmd_Argv(0));
        return 0;
    }

    client_t *const client = SV_GetPlayerByNum();
    return client != NULL ? SV_KickClient(client, nameOut, nameOutSize) : 0;
}

void SV_TempBan_f(void)
{
    char name[SERVER_PLAYER_NAME_BUFFER_SIZE];
    const int32_t guid = SV_KickUser_f(name, sizeof(name));

    if (guid != 0) {
        Com_Printf("%s (guid %i) was kicked for cheating\n", name, guid);
        SV_AuthorizeGuidCacheStore(guid);
    }
}

void SV_Ban_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: banUser <player name>\n");
        return;
    }

    client_t *const client = SV_GetPlayerByName();
    if (client != NULL) {
        SV_BanClient(client);
    }
}

void SV_BanNum_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: banClient <client number>\n");
        return;
    }

    client_t *const client = SV_GetPlayerByNum();
    if (client != NULL) {
        SV_BanClient(client);
    }
}

void SV_Unban_f(void)
{
    if (Cmd_Argc() == 2) {
        SV_UnbanClient(Cmd_Argv(1));
    } else {
        Com_Printf("Usage: unban <client name>\n");
    }
}

void SV_Drop_f(void)
{
    (void)SV_KickUser_f(NULL, 0);
}

void SV_DropNum_f(void)
{
    (void)SV_KickClient_f(NULL, 0);
}

void SV_TempBanNum_f(void)
{
    char name[SERVER_PLAYER_NAME_BUFFER_SIZE];
    const int32_t guid = SV_KickClient_f(name, sizeof(name));

    if (guid != 0) {
        Com_Printf("%s (guid %i) was kicked for cheating\n", name, guid);
        SV_AuthorizeGuidCacheStore(guid);
    }
}
