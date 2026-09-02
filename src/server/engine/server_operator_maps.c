#include "server_operator_maps.h"

#include "qcommon/com_parse.h"
#include "filesystem/filesystem.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_string.h"
#include "server_commands.h"
#include "server_client_gamestate.h"
#include "server_client_message.h"
#include "server_game_lifecycle.h"
#include "server_frame.h"
#include "qcommon/server_runtime_types.h"
#include "server_snapshot_archive.h"
#include "server_startup.h"
#include "qcommon/vm_runtime.h"
#include "animation/xanim.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern serverHeader_t sv;
extern serverStatic_t svs;
extern vm_t *sv_gameVM;
extern cvar_t *sv_running;
extern cvar_t *sv_maxclients;
extern cvar_t *g_gametype;
extern cvar_t *sv_mapname;
extern cvar_t *sv_mapRotationCurrent;
extern cvar_t *scr_allow_tanks;
extern cvar_t *sv_mapRotation;
extern cvar_t *scr_allow_jeeps;
extern char sv_gametypeNormalizeBuffer[MAX_QPATH];
extern int32_t com_frameTime;
extern int32_t sv_serverId;

void Com_Printf(const char *format, ...);
/*
 * Complete map operator-command cluster shared by the Windows client engine
 * and Linux dedicated engine.  The supporting Mac client exports the same
 * canonical function names.
 *
 * Function                    Windows       Linux
 * SV_GetMapBaseName           0x004575b0    0x08088020
 * SV_Map_f                    0x00457650    0x080880c9
 * SV_MapRestart_f             0x004578d0    0x08088325
 * SV_GetMapRotationToken      0x00457be0    0x08088655
 * SV_MapRotate_f              0x00457cb0    0x080886e5
 *
 * The bodies agree on map-name handling, cvar changes, restart state,
 * warm-up frames, client reconnection, rotation-token ownership, and command
 * execution.  Windows calls its statically linked _stricmp while Linux calls
 * strcasecmp; every site uses only equality, neither image changes the C
 * locale, and Q_stricmp therefore preserves the common predicate without a
 * target branch.  Windows inlines SV_InitArchivedSnapshot during restart and
 * the Linux compiler emits the ordinary call retained below.
 */

const char *SV_GetMapBaseName(const char *mapName)
{
    if (Q_stricmpn(mapName, "mp", 2) == 0 && (mapName[2] == '/' || mapName[2] == '\\')) {
        mapName += 3;
    }

    size_t length = strlen(mapName);
    const size_t extensionLength = sizeof(".bsp") - 1u;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length >= extensionLength && strcmp(mapName + length - extensionLength, ".bsp") == 0) {
        length -= extensionLength;
    }

    char stripped[SERVER_MAP_NAME_BUFFER_SIZE];
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length >= sizeof(stripped)) {
        Com_Printf("SV_GetMapBaseName: map name exceeds %i characters\n", (int32_t)sizeof(stripped) - 1);
        return NULL;
    }
    memcpy(stripped, mapName, length);
    stripped[length] = '\0';
    return va("%s", stripped);
}

/* SV_MapExists is part of the same map-name/filesystem cluster:
 * CoDUOMP.exe 0x0045d7e0; coduo_lnxded 0x0808ee42. Both bodies strip the
 * optional mp prefix and .bsp suffix, format the same qpath, and treat every
 * nonnegative FS_ReadFile result as existence. */
qboolean SV_MapExists(const char *mapName)
{
    const char *const cleanMapName = SV_GetMapBaseName(mapName);
    if (cleanMapName == NULL) {
        return qfalse;
    }
    const char *const bspPath = va("maps/mp/%s.bsp", cleanMapName);

    return FS_ReadFile(bspPath, NULL) >= 0 ? qtrue : qfalse;
}

void SV_Map_f(void)
{
    const char *const argument = Cmd_Argv(1);
    if (argument[0] == '\0') {
        return;
    }

    char mapName[SERVER_MAP_NAME_BUFFER_SIZE];
    const size_t argumentLength = strlen(argument);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (argumentLength >= sizeof(mapName)) {
        Com_Printf("SV_Map_f: map name exceeds %i characters\n", (int32_t)sizeof(mapName) - 1);
        return;
    }
    memcpy(mapName, argument, argumentLength + 1u);
    Q_strlwr(mapName);

    const char *const cleanMapName = SV_GetMapBaseName(mapName);
    if (cleanMapName == NULL) {
        return;
    }
    const char *const bspPath = va("maps/mp/%s.bsp", cleanMapName);
    if (FS_ReadFile(bspPath, NULL) == -1) {
        Com_Printf("Can't find map %s\n", bspPath);
        return;
    }

    const qboolean devmap = Q_stricmp(Cmd_Argv(0), "devmap") == 0 ? qtrue : qfalse;
    if (sv_running->integer != 0 && com_timescale->value <= 0.0f) {
        Cvar_SetValue("timescale", 1.0f);
    }

    if (Cmd_Argc() < 3 || (Cmd_Argv(2) != NULL && Q_stricmp(Cmd_Argv(2), "noautoexec") != 0)) {
        char gametype[16];
        Q_strncpyz(gametype, Cvar_VariableString("g_gametype"), sizeof(gametype));
        Q_strlwr(gametype);
        Cbuf_ExecuteText(EXEC_NOW, va("exec %s_%s.cfg\n", cleanMapName, gametype));
    }

    if (sv_running->integer != 0 && Q_stricmp(cleanMapName, sv_mapname->string) == 0) {
        SV_MapRestart_f();
    } else {
        char spawnMapName[SERVER_MAP_NAME_BUFFER_SIZE];
        Q_strncpyz(spawnMapName, cleanMapName, sizeof(spawnMapName));
        SV_SpawnServer(spawnMapName);
    }

    Cvar_Set("sv_cheats", devmap != qfalse ? "1" : "0");
}

void SV_MapRestart_f(void)
{
    if (sv_running->integer == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (com_timescale->value <= 0.0f) {
        Cvar_SetValue("timescale", 1.0f);
    }

    (void)Cvar_Get("g_gametype", "dm", CVAR_SERVERINFO | CVAR_LATCH);
    /* The Linux instruction at 0x0808838a passes command 18.  The former
     * recovered GAME_CONSOLE_COMMAND spelling was a transcription error. */
    const qboolean restartGate = VM_Call(sv_gameVM, GAME_GET_MATCH_STATE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) != 0 ? qtrue : qfalse;

    if (restartGate == qfalse) {
        if (Q_stricmp(sv_gametypeNormalizeBuffer, g_gametype->string) != 0) {
            char mapName[SERVER_MAP_NAME_BUFFER_SIZE];
            Com_Printf("g_gametype variable change -- restarting.\n");
            Q_strncpyz(mapName, Cvar_VariableString("mapname"), sizeof(mapName));
            SV_SpawnServer(mapName);
            return;
        }

        if (sv_maxclients->modified != qfalse) {
            char mapName[SERVER_MAP_NAME_BUFFER_SIZE];
            Com_Printf("sv_maxclients variable change -- restarting.\n");
            Q_strncpyz(mapName, Cvar_VariableString("mapname"), sizeof(mapName));
            SV_SpawnServer(mapName);
            return;
        }
    } else {
        Cvar_Set("g_gametype", sv_gametypeNormalizeBuffer);
    }

    /* Linux 0x08088466 and 0x080884cb use the same 0x0848871c global that
     * Com_Frame fills from Com_EventLoop.  The former separate
     * sv_serverId_source reconstruction was not an original datum. */
    if (sv.serverId == com_frameTime) {
        return;
    }

    SV_InitCvar();
    SV_InitArchivedSnapshot();
    svs.snapFlagServerBit ^= SERVER_SNAPSHOT_RESTART_FLAG;

    sv_serverId = ((sv_serverId + 1) & SERVER_ID_LOW_MASK) + (sv_serverId & SERVER_ID_HIGH_MASK);
    Cvar_Set("sv_serverid", va("%i", sv_serverId));
    sv.serverId = com_frameTime;
    sv.state = SS_LOADING;
    sv.restarting = qtrue;
    Cvar_Set("sv_serverRestarting", "1");

    XAnimSetUser(XANIM_USER_SERVER);
    SV_RestartGameProgs(restartGate);

    for (int32_t frame = 0; frame < SERVER_RESTART_WARMUP_FRAMES; ++frame) {
        svs.realTime += SERVER_RESTART_WARMUP_MSEC;
        svs.time += SERVER_RESTART_WARMUP_MSEC;
        SV_RunFrame();
    }

    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state < CS_CONNECTED) {
            continue;
        }

        SV_AddServerCommand(client, qtrue, restartGate != qfalse ? "n" : "B");
        const char *const reject =
            (const char *)VM_Call(sv_gameVM, GAME_CLIENT_CONNECT, clientNum, client->scriptId, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (reject != NULL) {
            SV_DropClient(client, reject);
            Com_Printf("SV_MapRestart_f: dropped client %i - denied!\n", clientNum);
        } else if (client->state == CS_ACTIVE) {
            SV_ClientEnterWorld(client, &client->lastUsercmd);
        }
    }

    sv.state = SS_GAME;
    sv.restarting = qfalse;
    Cvar_Set("sv_serverRestarting", "0");
}

const char *SV_GetMapRotationToken(void)
{
    char *parseData = sv_mapRotationCurrent->string;
    const char *const token = Com_Parse(&parseData);

    if (parseData == NULL) {
        Cvar_Set("sv_mapRotationCurrent", "");
        return NULL;
    }

    char remainingRotation[strlen(parseData) + 1];
    strcpy(remainingRotation, parseData);
    Cvar_Set("sv_mapRotationCurrent", remainingRotation);
    return token;
}

void SV_MapRotate_f(void)
{
    qboolean execSeen = qfalse;

    Com_Printf("map_rotate...\n\n");
    Com_Printf("\"sv_mapRotation\" is:\"%s\"\n\n", sv_mapRotation->string);
    Com_Printf("\"sv_mapRotationCurrent\" is:\"%s\"\n\n", sv_mapRotationCurrent->string);

    if (sv_mapRotationCurrent->string[0] == '\0') {
        Cvar_Set("sv_mapRotationCurrent", sv_mapRotation->string);
    }

    const char *token = SV_GetMapRotationToken();
    if (token == NULL) {
        Cvar_Set("sv_mapRotationCurrent", sv_mapRotation->string);
        token = SV_GetMapRotationToken();
    }

    for (;;) {
        if (token == NULL) {
            Com_Printf("No map specified in sv_mapRotation - forcing map_restart.\n");
            SV_MapRestart_f();
            return;
        }

        if (Q_stricmp(token, "exec") == 0) {
            token = SV_GetMapRotationToken();
            if (token == NULL) {
                Com_Printf("No value specified after 'exec' keyword in "
                           "sv_mapRotation - forcing map_restart.\n");
                SV_MapRestart_f();
                return;
            }
            Com_Printf("Execing: %s.\n", token);
            Cbuf_ExecuteText(EXEC_NOW, va("exec %s\n", token));
            execSeen = qtrue;
        } else if (Q_stricmp(token, "allow_jeeps") == 0) {
            token = SV_GetMapRotationToken();
            if (token == NULL) {
                Com_Printf("No value specified after 'allow_jeeps' keyword in "
                           "sv_mapRotation - forcing map_restart.\n");
                SV_MapRestart_f();
                return;
            }
            Com_Printf("Setting scr_allow_jeeps: %s.\n", token);
            if (sv_running->integer != 0 && Q_stricmp(scr_allow_jeeps->string, token) != 0) {
                (void)VM_Call(sv_gameVM, GAME_SET_MATCH_STATE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            Cvar_Set("scr_allow_jeeps", token);
        } else if (Q_stricmp(token, "allow_tanks") == 0) {
            token = SV_GetMapRotationToken();
            if (token == NULL) {
                Com_Printf("No value specified after 'allow_tanks' keyword in "
                           "sv_mapRotation - forcing map_restart.\n");
                SV_MapRestart_f();
                return;
            }
            Com_Printf("Setting scr_allow_tanks: %s.\n", token);
            if (sv_running->integer != 0 && Q_stricmp(scr_allow_tanks->string, token) != 0) {
                (void)VM_Call(sv_gameVM, GAME_SET_MATCH_STATE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            Cvar_Set("scr_allow_tanks", token);
        } else if (Q_stricmp(token, "gametype") == 0) {
            token = SV_GetMapRotationToken();
            if (token == NULL) {
                Com_Printf("No gametype specified after 'gametype' keyword in "
                           "sv_mapRotation - forcing map_restart.\n");
                SV_MapRestart_f();
                return;
            }
            Com_Printf("Setting g_gametype: %s.\n", token);
            if (sv_running->integer != 0 && Q_stricmp(g_gametype->string, token) != 0) {
                (void)VM_Call(sv_gameVM, GAME_SET_MATCH_STATE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            Cvar_Set("g_gametype", token);
        } else if (Q_stricmp(token, "map") == 0) {
            token = SV_GetMapRotationToken();
            if (token == NULL) {
                Com_Printf("No map specified after 'map' keyword in "
                           "sv_mapRotation - forcing map_restart.\n");
                SV_MapRestart_f();
                return;
            }
            Com_Printf("Setting map: %s.\n", token);
            Cbuf_ExecuteText(EXEC_NOW, execSeen != qfalse ? va("map %s noautoexec\n", token) : va("map %s\n", token));
            return;
        } else {
            Com_Printf("Unknown keyword '%s' in sv_mapRotation.\n", token);
        }

        token = SV_GetMapRotationToken();
    }
}
