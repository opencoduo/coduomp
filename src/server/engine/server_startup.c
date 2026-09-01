#include "server_startup.h"

#include "collision/collision_map_load.h"
#include "qcommon/com_parse.h"
#include "filesystem/filesystem.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/hunk.h"
#include "qcommon/net_text.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "server_client_release.h"
#include "server_client_message.h"
#include "server_configstrings.h"
#include "server_frame.h"
#include "server_game_data.h"
#include "server_game_lifecycle.h"
#include "server_master.h"
#include "server_operator_runtime.h"
#include "qcommon/server_runtime_types.h"
#include "server_snapshot_archive.h"
#include "server_snapshot_send.h"
#include "server_startup_services.h"
#include "sound/alias/sound_alias.h"
#include "qcommon/vm_runtime.h"
#include "animation/xanim.h"
#include "animation/xmodel.h"

#include <stdlib.h>
#include <string.h>

enum {
    SERVER_MIN_CLIENTS = 1,
    SERVER_DEDICATED_ENTITY_SNAPSHOTS_PER_CLIENT = 2048,
    SERVER_LOCAL_ENTITY_SNAPSHOTS_PER_CLIENT = 256,
    SERVER_DEDICATED_CLIENT_SNAPSHOTS_PER_PAIR = 32,
    SERVER_LOCAL_CLIENT_SNAPSHOTS_PER_PAIR = 4,
    SERVER_LOADING_RECONNECT_DELAY_MSEC = 250,
    SERVER_ID_SPAWN_STEP = 16,
    SERVER_SYSTEMINFO_BUFFER_SIZE = 8192,
    SERVER_XMODEL_CHECK_FLAGS = CVAR_ARCHIVE | CVAR_LATCH,
    SERVER_GAMETYPE_FLAGS = CVAR_SERVERINFO | CVAR_LATCH
};

extern serverHeader_t sv;
extern serverStatic_t svs;
extern vm_t *sv_gameVM;
extern cvar_t *dedicated;
extern cvar_t *sv_maxclients;
extern cvar_t *g_gametype;
extern cvar_t *sv_pure;
extern char *sv_configstrings[MAX_CONFIGSTRINGS];
extern char *sv_entityParsePoint;
extern char sv_gametypeNormalizeBuffer[MAX_QPATH];
extern int32_t sv_serverId;
extern int32_t sv_timeResidual;
extern int32_t com_frameTime;

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void NET_Sleep(int32_t milliseconds);
/*
 * Complete server startup, resize, clear, and map-spawn subsystem shared by
 * the Windows client engine and Linux dedicated engine. The supporting Mac
 * client exports the same eight canonical function names.
 *
 * Function                    Windows       Linux
 * SV_CreateBaseline           0x0045f130    0x0809127f
 * SV_BoundMaxClients          0x0045f1e0    0x080913e2
 * SV_Startup                  0x0045f230    0x08091473
 * SV_ChangeMaxClients         0x0045f330    0x08091563
 * SV_SetExpectedHunkUsage     0x0045f5c0    0x08091824
 * SV_ClearServer              0x0045f760    0x0809193d
 * SV_InitCvar                 0x0045f9a0    0x08091afd
 * SV_SpawnServer              0x0045fa30    0x08091b72
 *
 * Client shutdown, checksum RNG/call order, runtime-pool realization,
 * signed-byte ctype dispatch, and PunkBuster notification are target-owned
 * boundaries represented by server_startup_services.h.
 */

/* Source: CoDUOMP.exe 0x0045f130..0x0045f1da.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f130_0045f1db.mcode.
 * Name: exact same-module Mac symbol SV_CreateBaseline. The baseline stores
 * the linked entity's absolute bounds at +0x124/+0x130, not its model-local
 * mins/maxs at +0x108/+0x114. */
void SV_CreateBaseline(void)
{
    for (int32_t entityNum = 1;
         entityNum < sv_numGentities;
         ++entityNum) {
        sharedEntity_t *gentity = SV_GentityNum(entityNum);
        if (gentity->linked == qfalse) {
            continue;
        }

        gentity->entityState.number = entityNum;
        archivedEntity_t *baseline =
            &sv_entities[entityNum].baseline;
        baseline->state = gentity->entityState;
        baseline->svFlags = (int32_t)gentity->svFlags;
        baseline->singleClient = gentity->singleClient;
        for (int32_t axis = 0; axis < 3; ++axis) {
            baseline->absmin[axis] = gentity->absMin[axis];
            baseline->absmax[axis] = gentity->absMax[axis];
        }
    }
}

/* Source: CoDUOMP.exe 0x0045f1e0..0x0045f22b, recovered from an executable
 * gap after repairing the missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f1e0_0045f22c.mcode.
 * Name: exact same-module Mac symbol SV_BoundMaxClients. */
void SV_BoundMaxClients(int32_t minimumClientCount)
{
    (void)Cvar_Get("sv_maxclients", "20", 0);
    sv_maxclients->modified = qfalse;

    if (sv_maxclients->integer < minimumClientCount) {
        (void)Cvar_Set2("sv_maxclients", va("%i", minimumClientCount),
                        qtrue);
    } else if (sv_maxclients->integer > MAX_CLIENTS) {
        (void)Cvar_Set2("sv_maxclients", va("%i", MAX_CLIENTS),
                        qtrue);
    }
}

/* Source: CoDUOMP.exe 0x0045f230..0x0045f328.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f230_0045f329.mcode.
 * Name: exact same-module Mac symbol SV_Startup. Windows inlines
 * SV_BoundMaxClients(SERVER_MIN_CLIENTS). */
void SV_Startup(void)
{
    if (svs.initialized != qfalse) {
        Com_Error(ERR_FATAL,
                  "\x15" "SV_Startup: svs.initialized");
    }

    SV_BoundMaxClients(SERVER_MIN_CLIENTS);

    const int32_t clientCount = sv_maxclients->integer;
    svs.clients = calloc(1, (size_t)clientCount * sizeof(*svs.clients));
    if (svs.clients == NULL) {
        Com_Error(ERR_FATAL,
                  "\x15" "SV_Startup: unable to allocate svs.clients");
    }

    if (dedicated->integer != 0) {
        svs.numEntityStateSnapshots =
            clientCount * SERVER_DEDICATED_ENTITY_SNAPSHOTS_PER_CLIENT;
        svs.numClientSnapshots =
            clientCount * clientCount *
            SERVER_DEDICATED_CLIENT_SNAPSHOTS_PER_PAIR;
    } else {
        svs.numEntityStateSnapshots =
            clientCount * SERVER_LOCAL_ENTITY_SNAPSHOTS_PER_CLIENT;
        svs.numClientSnapshots =
            clientCount * clientCount *
            SERVER_LOCAL_CLIENT_SNAPSHOTS_PER_PAIR;
    }

    svs.initialized = qtrue;
    (void)Cvar_Set2("sv_running", "1", qtrue);
}

/* Source: CoDUOMP.exe 0x0045f330..0x0045f5b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f330_0045f5b2.mcode.
 * Name: exact same-module Mac symbol SV_ChangeMaxClients. The Windows compiler
 * inlines SV_BoundMaxClients, the two full-client copies/clears, and
 * Hunk_FreeTempMemory; the maintained source retains their owning calls. */
void SV_ChangeMaxClients(void)
{
    int32_t preservedClientCount = 0;
    const int32_t oldMaxClients = sv_maxclients->integer;

    for (int32_t clientNum = 0;
         clientNum < oldMaxClients;
         ++clientNum) {
        if (svs.clients[clientNum].state >= CS_CONNECTED &&
            preservedClientCount < clientNum) {
            preservedClientCount = clientNum;
        }
    }
    ++preservedClientCount;

    SV_BoundMaxClients(preservedClientCount);
    if (sv_maxclients->integer == oldMaxClients) {
        return;
    }

    client_t *oldClients = Hunk_AllocateTempMemoryInternal(
        (size_t)preservedClientCount * sizeof(*oldClients));
    for (int32_t clientNum = 0;
         clientNum < preservedClientCount;
         ++clientNum) {
        if (svs.clients[clientNum].state >= CS_CONNECTED) {
            oldClients[clientNum] = svs.clients[clientNum];
        } else {
            memset(&oldClients[clientNum], 0, sizeof(oldClients[clientNum]));
        }
    }

    free(svs.clients);
    const int32_t newMaxClients = sv_maxclients->integer;
    svs.clients = calloc(1, (size_t)newMaxClients * sizeof(*svs.clients));
    if (svs.clients == NULL) {
        Com_Error(ERR_FATAL,
                  "\x15" "SV_Startup: unable to allocate svs.clients");
    }
    memset(svs.clients, 0,
           (size_t)newMaxClients * sizeof(*svs.clients));

    for (int32_t clientNum = 0;
         clientNum < preservedClientCount;
         ++clientNum) {
        if (oldClients[clientNum].state >= CS_CONNECTED) {
            svs.clients[clientNum] = oldClients[clientNum];
        }
    }
    Hunk_FreeTempMemory(oldClients);

    if (dedicated->integer != 0) {
        svs.numEntityStateSnapshots =
            newMaxClients * SERVER_DEDICATED_ENTITY_SNAPSHOTS_PER_CLIENT;
        svs.numClientSnapshots =
            newMaxClients * newMaxClients *
            SERVER_DEDICATED_CLIENT_SNAPSHOTS_PER_PAIR;
    } else {
        svs.numEntityStateSnapshots =
            newMaxClients * SERVER_LOCAL_ENTITY_SNAPSHOTS_PER_CLIENT;
        svs.numClientSnapshots =
            newMaxClients * newMaxClients *
            SERVER_LOCAL_CLIENT_SNAPSHOTS_PER_PAIR;
    }
}

/* Source: CoDUOMP.exe 0x0045f5c0..0x0045f756.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f5c0_0045f757.mcode.
 * Name: exact same-module Mac symbol SV_SetExpectedHunkUsage. The first
 * case-insensitive map-path match supplies the following token as the expected
 * hunk size; a missing file, key, or value restores the original -1 sentinel. */
void SV_SetExpectedHunkUsage(const char *mapBspPath)
{
    int32_t fileHandle;
    const int32_t fileLength =
        FS_FOpenFileByMode("hunkusage.dat", &fileHandle, FS_READ);
    if (fileLength < 0) {
        (void)Cvar_Set2("com_expectedhunkusage", "-1", qtrue);
        return;
    }

    char *fileBuffer = Z_MallocInternal((size_t)fileLength + 1);
    memset(fileBuffer, 0, (size_t)fileLength + 1);
    (void)FS_Read(fileBuffer, fileLength, fileHandle);
    FS_FCloseFile(fileHandle);

    char *parseData = fileBuffer;
    for (;;) {
        const char *token = Com_Parse(&parseData);
        if (token == NULL || token[0] == '\0') {
            Z_FreeInternal(fileBuffer);
            (void)Cvar_Set2("com_expectedhunkusage", "-1", qtrue);
            return;
        }

        if (Q_strcasecmp(token, mapBspPath) == 0) {
            token = Com_Parse(&parseData);
            if (token != NULL && token[0] != '\0') {
                (void)Cvar_Set2("com_expectedhunkusage", token, qtrue);
                Z_FreeInternal(fileBuffer);
                return;
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x0045f760..0x0045f7e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f760_0045f7e2.mcode.
 * Name: exact same-module Mac symbol SV_ClearServer. The original i386
 * server_t occupies one contiguous 0x6252c-byte block from sv through the
 * byte before sv_pure. Maintained portable source represents its proven
 * subobjects as separate typed globals, so they are cleared by ownership
 * rather than by an invalid cross-object byte span. */
void SV_ClearServer(void)
{
    for (int32_t index = 0;
         index < MAX_CONFIGSTRINGS;
         ++index) {
        if (sv_configstrings[index] != NULL)
            Z_FreeInternal(sv_configstrings[index]);
    }

    memset(&sv, 0, sizeof(sv));
    sv_timeResidual = 0;
    memset(sv_configstrings, 0, sizeof(sv_configstrings));
    memset(sv_entities, 0, sizeof(sv_entities));

    sv_entityParsePoint = NULL;
    sv_gentities = NULL;
    sv_gentitySize = 0;
    sv_numGentities = 0;
    sv_gameClients = NULL;
    sv_gameClientSize = 0;

    memset(sv_compressedBpsWindow, 0,
           sizeof(sv_compressedBpsWindow));
    sv_averageBpsFrameCount = 0;
    sv_totalBytesSentThisFrame = 0;
    sv_compressedBpsMax = 0;
    memset(sv_uncompressedBpsWindow, 0,
           sizeof(sv_uncompressedBpsWindow));
    sv_totalUncompressedBytesThisFrame = 0;
    sv_uncompressedBpsMax = 0;
    sv_averageCompressionRatioSum = 0.0f;
    sv_averageCompressionRatioCount = 0;
    memset(sv_gametypeNormalizeBuffer, 0,
           sizeof(sv_gametypeNormalizeBuffer));
}

/* Source: CoDUOMP.exe 0x0045f9a0..0x0045fa2c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045f9a0_0045fa2c.mcode.
 * Name: exact same-module Mac symbol SV_InitCvar. Besides normalizing the
 * gametype, the original walks the complete cvar list and clears the
 * script-set-serverinfo bit from every cvar. */
void SV_InitCvar(void)
{
    Q_strncpyz(sv_gametypeNormalizeBuffer,
               Cvar_VariableString("g_gametype"),
               sizeof(sv_gametypeNormalizeBuffer));

    for (char *cursor = sv_gametypeNormalizeBuffer;
         *cursor != '\0';
         ++cursor) {
        *cursor = (char)server_compat_tolower_gametype_byte(*cursor);
    }

    (void)Cvar_Set2("g_gametype", sv_gametypeNormalizeBuffer, qtrue);
    Cvar_ClearScriptSetServerinfoFlags();
}

/* Source: CoDUOMP.exe 0x0045fa30..0x004604c2 and coduo_lnxded
 * 0x08091b72..0x080921f5. The common body retains the complete map-spawn
 * lifecycle. Only the five target boundaries documented above are adapters. */
void SV_SpawnServer(const char *serverName)
{
    int32_t mapChecksum;
    char systemInfo[SERVER_SYSTEMINFO_BUFFER_SIZE];
    qboolean restarting;

    XModelEnforceExist(
        Cvar_Get("cl_xmodelcheck", "0", SERVER_XMODEL_CHECK_FLAGS)->integer);
    (void)Cvar_Get("g_gametype", "dm", SERVER_GAMETYPE_FLAGS);

    if (Cvar_VariableValue("sv_running") != 0.0f) {
        restarting = VM_Call(sv_gameVM, GAME_GET_MATCH_STATE,
                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) != 0
                         ? qtrue
                         : qfalse;
        if (restarting != qfalse) {
            Cvar_Set("g_gametype", sv_gametypeNormalizeBuffer);
        }

        for (int32_t clientNum = 0;
             clientNum < sv_maxclients->integer;
             ++clientNum) {
            client_t *const client = &svs.clients[clientNum];
            if (client->state >= CS_PRIMED) {
                NET_OutOfBandPrint(
                    NS_SERVER, client->netchan.remoteAddress,
                    "loadingnewmap\n%s\n%s", serverName,
                    g_gametype->string);
            }
        }
        NET_Sleep(SERVER_LOADING_RECONNECT_DELAY_MSEC);
    } else {
        restarting = qfalse;
    }

    server_compat_begin_map_load();
    SV_ShutdownGameProgs();
    Com_Printf("------ Server Initialization ------\n");
    Com_Printf("Server: %s\n", serverName);
    server_compat_prepare_second_game_shutdown();
    SV_ShutdownGameProgs();
    server_compat_finish_client_shutdown();

    Hunk_Clear();
    VM_Clear();
    SV_ClearServer();
    FS_Shutdown(qtrue);
    FS_ClearPakReferences(qfalse);

    sv.gamestateChecksumFeed = server_compat_generate_checksum_feed();
    FS_Restart(sv.gamestateChecksumFeed);
    FS_RefreshLookupCache();

    for (int32_t index = 0;
         index < MAX_CONFIGSTRINGS;
         ++index) {
        sv_configstrings[index] = CopyStringInternal("");
    }

    if (Cvar_VariableValue("sv_running") == 0.0f) {
        server_compat_initialize_new_server_pools();
        SV_Startup();
    } else {
        server_compat_initialize_restart_pools();
        if (sv_maxclients->modified != qfalse) {
            SV_ChangeMaxClients();
        }
    }

    SV_InitCvar();
    svs.entityStateSnapshots = Hunk_AllocInternal(
        (size_t)svs.numEntityStateSnapshots *
        sizeof(*svs.entityStateSnapshots));
    svs.nextEntityStateSnapshot = 0;
    svs.clientSnapshots = Hunk_AllocInternal(
        (size_t)svs.numClientSnapshots * sizeof(*svs.clientSnapshots));
    svs.nextClientSnapshot = 0;
    SV_InitArchivedSnapshot();
    svs.snapFlagServerBit ^= SERVER_SNAPSHOT_RESTART_FLAG;

    Cvar_Set("nextmap", "map_restart");
    SV_SetExpectedHunkUsage(va("maps/mp/%s.bsp", serverName));
    Cvar_Set("cl_paused", "0");
    CM_LoadMap(va("maps/mp/%s.bsp", serverName), qfalse, &mapChecksum);
    Cvar_Set("mapname", serverName);

    /* Both retail bodies add through AL and then zero-extend it, so the
     * increment wraps at eight bits before the high-nibble repair. */
    sv_serverId = (uint8_t)(sv_serverId + SERVER_ID_SPAWN_STEP);
    if ((sv_serverId & SERVER_ID_HIGH_MASK) == 0) {
        sv_serverId += SERVER_ID_SPAWN_STEP;
    }
    Cvar_Set("sv_serverid", va("%i", sv_serverId));
    sv.serverId = com_frameTime;
    sv.state = SS_LOADING;
    Cvar_Set("sv_serverRestarting", "1");

    Com_LoadSoundAliases(va("maps/mp/%s.bsp", serverName),
                         SND_ALIAS_BANK_GAME);
    XAnimSetUser(XANIM_USER_SERVER);
    SV_InitGameProgs(restarting);

    for (int32_t frame = 0;
         frame < SERVER_RESTART_WARMUP_FRAMES;
         ++frame) {
        svs.realTime += SERVER_RESTART_WARMUP_MSEC;
        svs.time += SERVER_RESTART_WARMUP_MSEC;
        SV_RunFrame();
    }

    SV_CreateBaseline();

    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state >= CS_CONNECTED) {
            const intptr_t reject =
                VM_Call(sv_gameVM, GAME_CLIENT_CONNECT,
                        clientNum, client->scriptId,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            if (reject == 0) {
                client->state = CS_CONNECTED;
            } else {
                SV_DropClient(client, (const char *)reject);
            }
        }
    }

    if (sv_pure->integer == 0) {
        Cvar_Set("sv_paks", "");
        Cvar_Set("sv_pakNames", "");
    } else {
        const char *const loadedPakChecksums = FS_LoadedPakChecksums();
        Cvar_Set("sv_paks", loadedPakChecksums);
        if (loadedPakChecksums[0] == '\0') {
            Com_Printf("WARNING: sv_pure set but no PK3 files loaded\n");
        }
        Cvar_Set("sv_pakNames", FS_LoadedPakNames());
    }
    Cvar_Set("sv_referencedPaks", FS_ReferencedPakChecksums());
    Cvar_Set("sv_referencedPakNames", FS_ReferencedPakNames());

    Q_strncpyz(systemInfo, Cvar_InfoString_Big(CVAR_SYSTEMINFO_SYNC_MASK),
               sizeof(systemInfo));
    cvar_modifiedFlags &= ~CVAR_SYSTEMINFO_SYNC_MASK;
    SV_SetConfigstring(CS_SYSTEMINFO, systemInfo);

    SV_SetConfigstring(CS_SERVERINFO,
                       Cvar_InfoString(CVAR_SERVERINFO_SYNC_MASK));
    cvar_modifiedFlags &= ~CVAR_SERVERINFO_SYNC_MASK;

    Cvar_SetConfigstringValues(CS_CONFIGVALUE_NAMES,
                               CS_CONFIGVALUE_COUNT,
                               CVAR_SYSTEMINFO_KEY_VALUE_SYNC_MASK);
    cvar_modifiedFlags &= ~CVAR_SYSTEMINFO_KEY_VALUE_SYNC_MASK;

    sv.state = SS_GAME;
    SV_Heartbeat_f();
    Cvar_Set("sv_serverRestarting", "0");
    Com_Printf("-----------------------------------\n");

    server_compat_notify_punkbuster_state(
        Cvar_VariableString("sv_punkbuster")[0] == '1'
            ? qtrue
            : qfalse);
}
