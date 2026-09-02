#include "server_game_bridge.h"

#include "qcommon/info.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_runtime_types.h"
#include "server_commands.h"
#include "server_client_message.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SERVER_GAME_BROADCAST_CLIENT = -1
};

extern cvar_t *sv_maxclients;
extern serverStatic_t svs;

void Com_Error(errorParm_t code, const char *format, ...);
/*
 * Complete game-module host bridge shared by the Windows client/listen server
 * and Linux dedicated engine:
 *
 *                                  Windows client       Linux dedicated
 * SV_GameSendServerCommand         0x0045c7e0           0x0808e19a
 * SV_GameDropClient                0x0045c830           0x0808e20d
 * SV_GetServerinfo                 0x0045cc90           0x0808e759
 * SV_GetUsercmd                    0x0045ce20           0x0808e8c5
 * SV_SetUserinfo                   0x0045f040           0x08091153
 * SV_GetUserinfo                   0x0045f0d0           0x080911fd
 *
 * Direct machine-code comparison proves the same validation bounds, client
 * stride and field offsets, string-copy limits, command formatting, and
 * send/drop calls. Linux rodata at 0x080e5640 also proves that the bad-index
 * SV_GetUserinfo diagnostic ends in the same newline as Windows; its absence
 * from the former Linux recovered source was a transcription error.
 */

void SV_GameSendServerCommand(int32_t clientNum, qboolean reliable, const char *command)
{
    if (clientNum == SERVER_GAME_BROADCAST_CLIENT) {
        SV_SendServerCommand(NULL, reliable, "%s", command);
    } else if (clientNum >= 0 && clientNum < sv_maxclients->integer) {
        SV_SendServerCommand(&svs.clients[clientNum], reliable, "%s", command);
    }
}

void SV_GameDropClient(int32_t clientNum, const char *reason)
{
    if (clientNum >= 0 && clientNum < sv_maxclients->integer) {
        SV_DropClient(&svs.clients[clientNum], reason);
    }
}

void SV_GetServerinfo(char *buffer, int32_t bufferSize)
{
    if (bufferSize < 1) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "SV_GetServerinfo: bufferSize == %i",
                  bufferSize);
    }

    Q_strncpyz(buffer, Cvar_InfoString(CVAR_SERVERINFO_SYNC_MASK), bufferSize);
}

void SV_GetUsercmd(int32_t clientNum, usercmd_t *command)
{
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "SV_GetUsercmd: bad clientNum:%i",
                  clientNum);
    }

    *command = svs.clients[clientNum].lastUsercmd;
}

void SV_SetUserinfo(int32_t clientNum, const char *userinfo)
{
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "SV_SetUserinfo: bad index %i\n",
                  clientNum);
    }
    if (userinfo == NULL) {
        userinfo = "";
    }

    client_t *const client = &svs.clients[clientNum];
    Q_strncpyz(client->userinfo, userinfo, sizeof(client->userinfo));
    Q_strncpyz(client->name, Info_ValueForKey(userinfo, "name"), sizeof(client->name));
}

void SV_GetUserinfo(int32_t clientNum, char *buffer, int32_t bufferSize)
{
    if (bufferSize < 1) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "SV_GetUserinfo: bufferSize == %i",
                  bufferSize);
    }
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "SV_GetUserinfo: bad index %i\n",
                  clientNum);
    }

    Q_strncpyz(buffer, svs.clients[clientNum].userinfo, bufferSize);
}
