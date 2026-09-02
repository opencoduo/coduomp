#include "server_operator_runtime.h"

#include "qcommon/info.h"
#include "qcommon/net_text.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_string.h"
#include "scripting/script_memory.h"
#include "scripting/script_usage.h"
#include "server_commands.h"
#include "server_master.h"
#include "server_operator_clients.h"
#include "server_operator_maps.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    SERVER_STATUS_MAX_PING = 9999,
    SERVER_STATUS_NAME_COLUMN_WIDTH = 16,
    SERVER_STATUS_ADDRESS_COLUMN_WIDTH = 22
};

#define SERVER_CONSOLE_CHAT_PREFIX "console: "

extern serverStatic_t svs;
extern cvar_t *sv_running;
extern cvar_t *sv_mapname;
extern cvar_t *sv_maxclients;
extern cvar_t *dedicated;

void Com_Printf(const char *format, ...);
void Com_Shutdown(const char *finalMessage);

/*
 * Complete common server operator presentation and registration subsystem.
 * The authoritative ranges are CoDUOMP.exe 0x00458400..0x00458c48 and
 * coduo_lnxded 0x08088ed1..0x080897d0.  The supporting Mac client exports
 * the same canonical function names.
 *
 * The status fields, command tests, registration order, callback identities,
 * strings, cvar flags, and state changes agree.  Windows inlines
 * SV_GetClientScore in SV_Status_f and performs several small tail calls;
 * Linux emits ordinary calls and full stack frames.  The common source keeps
 * the canonical helper calls without changing behavior.
 *
 * Both retail chat bodies use the command format h "\x15%s".  Windows
 * rodata 0x0059c350 and Linux rodata 0x080e3bf2 contain the same bytes, and
 * both SV_ConTell_f bodies reference that format.  The former recovered
 * Windows spellings without the 0x15 marker, and with command e for tell,
 * were transcription errors rather than a platform difference.
 */

static qboolean svOperatorCommandsInitialized;

void SV_Status_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }

    Com_Printf("map: %s\n", sv_mapname->string);
    Com_Printf("num score ping guid   name            lastmsg address               "
               "qport rate\n");
    Com_Printf("--- ----- ---- ------ --------------- ------- --------------------- "
               "----- -----\n");

    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state == CS_FREE) {
            continue;
        }

        Com_Printf("%3i ", clientNum);
        Com_Printf("%5i ", SV_GetClientScore(client));

        if (client->state == CS_CONNECTED) {
            Com_Printf("CNCT ");
        } else if (client->state == CS_ZOMBIE) {
            Com_Printf("ZMBI ");
        } else {
            int32_t ping = client->ping;
            if (ping > SERVER_STATUS_MAX_PING) {
                ping = SERVER_STATUS_MAX_PING;
            }
            Com_Printf("%4i ", ping);
        }

        Com_Printf("%6i ", client->guid);
        Com_Printf("%s^7", client->name);
        for (int32_t column = Q_DrawStrlen(client->name); column < SERVER_STATUS_NAME_COLUMN_WIDTH; ++column) {
            Com_Printf(" ");
        }

        Com_Printf("%7i ", svs.realTime - client->lastPacketTime);
        const char *const address = NET_AdrToString(client->netchan.remoteAddress);
        Com_Printf("%s", address);
        for (int32_t column = (int32_t)strlen(address); column < SERVER_STATUS_ADDRESS_COLUMN_WIDTH; ++column) {
            Com_Printf(" ");
        }

        Com_Printf("%5i", client->netchan.qport);
        Com_Printf(" %5i", client->rate);
        Com_Printf("\n");
    }
    Com_Printf("\n");
}

void SV_ConSay_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }
    if (Cmd_Argc() <= 1) {
        return;
    }

    char text[MAX_STRING_CHARS] = SERVER_CONSOLE_CHAT_PREFIX;
    const char *message = Cmd_Args(1);
    if (message[0] == '"') {
        char *const unquoted = (char *)message + 1;
        unquoted[strlen(unquoted) - 1] = '\0';
        message = unquoted;
    }
    const size_t messageLength = strlen(message);
    /* NOT_FROM_ORIGINAL_SOURCE: the complete console message, fixed prefix,
     * and final NUL must fit before anything is sent. */
    if (messageLength > sizeof(text) - sizeof(SERVER_CONSOLE_CHAT_PREFIX)) {
        Com_Printf("SV_ConSay_f: console message is too long\n");
        return;
    }
    memcpy(text + sizeof(SERVER_CONSOLE_CHAT_PREFIX) - 1, message, messageLength + 1);
    SV_SendServerCommand(NULL, qfalse, "h \"\x15%s\"", text);
}

void SV_ConTell_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }
    if (Cmd_Argc() <= 2) {
        return;
    }

    const int32_t clientNum = atoi(Cmd_Argv(1));
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        return;
    }

    client_t *const client = &svs.clients[clientNum];
    if (client->state != CS_ACTIVE) {
        return;
    }

    char text[MAX_STRING_CHARS] = SERVER_CONSOLE_CHAT_PREFIX;
    const char *message = Cmd_Args(2);
    if (message[0] == '"') {
        char *const unquoted = (char *)message + 1;
        unquoted[strlen(unquoted) - 1] = '\0';
        message = unquoted;
    }
    const size_t messageLength = strlen(message);
    /* NOT_FROM_ORIGINAL_SOURCE: apply the same complete prefixed-message
     * capacity to directed console chat. */
    if (messageLength > sizeof(text) - sizeof(SERVER_CONSOLE_CHAT_PREFIX)) {
        Com_Printf("SV_ConTell_f: console message is too long\n");
        return;
    }
    memcpy(text + sizeof(SERVER_CONSOLE_CHAT_PREFIX) - 1, message, messageLength + 1);
    SV_SendServerCommand(client, qfalse, "h \"\x15%s\"", text);
}

void SV_Heartbeat_f(void)
{
    svs.nextHeartbeatTime = INT32_MIN;
}

void SV_Serverinfo_f(void)
{
    Com_Printf("Server info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SERVERINFO_SYNC_MASK));
}

void SV_Systeminfo_f(void)
{
    Com_Printf("System info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SYSTEMINFO));
}

void SV_DumpUser_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }
    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: info <userid>\n");
        return;
    }

    client_t *const client = SV_GetPlayerByName();
    if (client != NULL) {
        Com_Printf("userinfo\n");
        Com_Printf("--------\n");
        Info_Print(client->userinfo);
    }
}

void SV_KillServer_f(void)
{
    Com_Shutdown("EXE_SERVERKILLED");
}

void SV_GameCompleteStatus_f(void)
{
    SV_MasterGameCompleteStatus();
}

void SV_ScriptUsage_f(void)
{
    Scr_DumpScriptThreads();
}

void SV_StringUsage_f(void)
{
    MT_DumpTree();
}

void SV_SetDrawFriend_f(void)
{
    if (Cmd_Argc() == 2) {
        const char *const value = Cmd_Argv(1);
        Cvar_Set("scr_drawfriend", value);
        Cvar_Set("ui_drawfriend", value);
    } else {
        Com_Printf("Usage: %s <integer>\n", Cmd_Argv(0));
    }
}

void SV_SetFriendlyFire_f(void)
{
    if (Cmd_Argc() == 1) {
        Com_Printf("Usage: %s <integer>\n", Cmd_Argv(0));
        return;
    }
    const char *const value = Cmd_Argv(1);
    Cvar_Set("scr_friendlyfire", value);
    Cvar_Set("ui_friendlyfire", value);
}

void SV_SetKillcam_f(void)
{
    if (Cmd_Argc() == 1) {
        Com_Printf("Usage: %s <integer>\n", Cmd_Argv(0));
        return;
    }
    const char *const value = Cmd_Argv(1);
    Cvar_Set("scr_killcam", value);
    Cvar_Set("ui_killcam", value);
}

void SV_AddOperatorCommands(void)
{
    if (svOperatorCommandsInitialized != qfalse) {
        return;
    }
    svOperatorCommandsInitialized = qtrue;

    Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
    Cmd_AddCommand("kick", SV_Drop_f);
    Cmd_AddCommand("banUser", SV_Ban_f);
    Cmd_AddCommand("banClient", SV_BanNum_f);
    Cmd_AddCommand("tempBanUser", SV_TempBan_f);
    Cmd_AddCommand("tempBanClient", SV_TempBanNum_f);
    Cmd_AddCommand("unbanUser", SV_Unban_f);
    Cmd_AddCommand("clientkick", SV_DropNum_f);
    Cmd_AddCommand("status", SV_Status_f);
    Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
    Cmd_AddCommand("systeminfo", SV_Systeminfo_f);
    Cmd_AddCommand("dumpuser", SV_DumpUser_f);
    Cmd_AddCommand("map_restart", SV_MapRestart_f);
    Cmd_AddCommand("map", SV_Map_f);
    Cmd_AddCommand("map_rotate", SV_MapRotate_f);
    Cmd_AddCommand("gameCompleteStatus", SV_GameCompleteStatus_f);
    Cmd_AddCommand("devmap", SV_Map_f);
    Cmd_AddCommand("killserver", SV_KillServer_f);

    if (dedicated->integer != 0) {
        SV_AddDedicatedCommands();
    }

    Cmd_AddCommand("scriptUsage", SV_ScriptUsage_f);
    Cmd_AddCommand("stringUsage", SV_StringUsage_f);
    Cmd_AddCommand("setdrawfriend", SV_SetDrawFriend_f);
    Cmd_AddCommand("setfriendlyfire", SV_SetFriendlyFire_f);
    Cmd_AddCommand("setkillcam", SV_SetKillcam_f);
}

void SV_RemoveOperatorCommands(void)
{
}

void SV_AddDedicatedCommands(void)
{
    Cmd_AddCommand("say", SV_ConSay_f);
    Cmd_AddCommand("tell", SV_ConTell_f);
}

void SV_RemoveDedicatedCommands(void)
{
    Cmd_RemoveCommand("say");
    Cmd_RemoveCommand("tell");
}
