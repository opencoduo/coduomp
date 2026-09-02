#include "server_frame.h"

#include "qcommon/com_frame.h"
#include "qcommon/com_lifecycle.h"
#include "animation/dobj.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/hunk.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_string.h"
#include "scripting/script_runtime_state.h"
#include "server_client_maintenance.h"
#include "server_client_message.h"
#include "server_configstrings.h"
#include "server_frame_services.h"
#include "server_game_data.h"
#include "server_master.h"
#include "server_snapshot_archive.h"
#include "server_snapshot_send.h"
#include "qcommon/vm_runtime.h"
#include "animation/xanim.h"
#include "compat/crt/random_compat.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

enum {
    SERVER_BOT_BUTTON_ATTACK = 1,
    SERVER_BOT_BUTTON_USE = 64,
    SERVER_BOT_FULL_FORWARD_MOVE = 127,
    SERVER_BOT_FULL_BACK_MOVE = -127,
    SERVER_FRAME_MSEC_BASE = 1000,
    SERVER_FRAME_TIME_WRAP_LIMIT = 0x70000000,
    SERVER_FRAME_COUNTER_LIMIT = INT32_MAX - 1,
    SERVER_FRAME_CACHED_ENTITY_LIMIT = SERVER_FRAME_COUNTER_LIMIT - SERVER_CACHED_SNAPSHOT_ENTITY_COUNT,
    SERVER_FRAME_CACHED_CLIENT_LIMIT = SERVER_FRAME_COUNTER_LIMIT - SERVER_CACHED_SNAPSHOT_CLIENT_COUNT,
    SERVER_FRAME_ARCHIVED_FRAME_LIMIT = SERVER_FRAME_COUNTER_LIMIT - SERVER_ARCHIVED_SNAPSHOT_FRAME_COUNT,
    SERVER_FRAME_ARCHIVED_BUFFER_LIMIT = SERVER_FRAME_COUNTER_LIMIT - SERVER_ARCHIVED_SNAPSHOT_BUFFER_SIZE,
    SERVER_FRAME_CACHED_FRAME_LIMIT = SERVER_FRAME_COUNTER_LIMIT - SERVER_CACHED_SNAPSHOT_FRAME_COUNT,
    SERVER_FRAME_SERVERINFO_FLAGS = CVAR_SERVERINFO | CVAR_SCRIPT_SETCVAR_SERVERINFO,
    SERVER_FRAME_SYSTEMINFO_FLAGS = CVAR_SYSTEMINFO,
    SERVER_FRAME_SCRIPT_CONFIGSTRING_FLAGS = CVAR_SCRIPT_MAKE_SERVERINFO
};

static const float sv_botAttackOrUseThreshold = 0.5f;
static const float sv_botForwardThreshold = 0.33000001311302185f;

extern serverStatic_t svs;
extern vm_t *sv_gameVM;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_running;
extern cvar_t *sv_fps;
extern cvar_t *sv_killserver;
extern cvar_t *sv_mapname;
extern cvar_t *com_speeds;
extern int32_t com_timeGame;

/* CoDUOMP.exe 0x0389fdc4 and coduo_lnxded 0x084f6fb0. Server packet,
 * game-frame, and bot-frame entry paths share this re-entrancy/error-cleanup
 * flag. */
qboolean sv_frameRunning;

/* CoDUOMP.exe 0x048a5690 and coduo_lnxded 0x084f6fa8. Accumulates real
 * milliseconds until a complete server-frame quantum is available. */
int32_t sv_timeResidual;

/*
 * Complete server frame and synthetic-bot execution cluster:
 *
 * Function                    Windows       Linux
 * SV_RunFrame                 0x00463010    0x08094fca
 * SV_BotUserMove              0x00463070    0x08095033
 * SV_RunBotFrame              0x004631a0    0x0809517c
 * SV_Frame                    0x00463220    0x08095202
 *
 * Direct instruction comparison proves the same state gates, VM commands,
 * skeleton-cache lifetime, bot command construction, restart thresholds,
 * configstring synchronization, time accumulation, archive cadence, timeout
 * maintenance, packet delivery, and heartbeat. The target-local services
 * retain the selected random-number domain and CoDUOMP's client debug-
 * geometry flush.
 */

void SV_RunFrame(void)
{
    (void)VM_Call(sv_gameVM, GAME_UPDATE_CVARS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    dobj_skelCacheKey = (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
    if (dobj_skelCacheKey == 0) {
        ++dobj_skelCacheKey;
    }

    sv_frameRunning = qtrue;
    (void)VM_Call(sv_gameVM, GAME_RUN_FRAME, svs.time, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sv_frameRunning = qfalse;
    Hunk_ClearTempMemory();
}

void SV_BotUserMove(client_t *client)
{
    if (client->gentity == NULL) {
        return;
    }

    usercmd_t command;
    memset(&command, 0, sizeof(command));
    const int32_t clientNum = (int32_t)(client - svs.clients);
    const playerState_t *const playerState = SV_GameClientNum(clientNum);
    command.weapon = (uint8_t)playerState->currentWeapon;

    if (SV_GetClientArchiveTime(clientNum) == 0) {
        if (coduo_server_rand_unit() < sv_botAttackOrUseThreshold) {
            command.buttons |= SERVER_BOT_BUTTON_ATTACK;
        }
        if (coduo_server_rand_unit() < sv_botAttackOrUseThreshold) {
            command.buttons |= SERVER_BOT_BUTTON_USE;
        }
        if (coduo_server_rand_unit() < sv_botForwardThreshold) {
            command.forwardmove = SERVER_BOT_FULL_FORWARD_MOVE;
        } else if (coduo_server_rand_unit() < sv_botAttackOrUseThreshold) {
            command.forwardmove = SERVER_BOT_FULL_BACK_MOVE;
        }
    }

    client->deltaMessage = client->netchan.outgoingSequence - 1;
    SV_ClientThink(client, &command);
}

void SV_RunBotFrame(void)
{
    dobj_skelCacheKey = (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
    if (dobj_skelCacheKey == 0) {
        ++dobj_skelCacheKey;
    }

    sv_frameRunning = qtrue;
    client_t *client = svs.clients;
    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum, ++client) {
        if (client->state != CS_FREE && client->netchan.remoteAddress.type == NA_BAD) {
            SV_BotUserMove(client);
        }
    }
    sv_frameRunning = qfalse;
    Hunk_ClearTempMemory();
}

void SV_Frame(int32_t msec)
{
    if (sv_killserver->integer != 0) {
        Com_Shutdown("EXE_SERVERKILLED");
        Cvar_Set("sv_killserver", "0");
        return;
    }

    if (sv_running->integer == 0 || SV_CheckPaused() != qfalse) {
        return;
    }

    if (sv_fps->integer < 1) {
        Cvar_Set("sv_fps", "10");
    }

    const int32_t frameMsec = SERVER_FRAME_MSEC_BASE / sv_fps->integer;
    sv_timeResidual += msec;
    if (sv_timeResidual < frameMsec) {
        return;
    }

    const char *restartReason = NULL;
    if (svs.realTime > SERVER_FRAME_TIME_WRAP_LIMIT || svs.time > SERVER_FRAME_TIME_WRAP_LIMIT) {
        restartReason = "EXE_SERVERRESTARTTIMEWRAP";
    } else if (svs.nextEntityStateSnapshot >= SERVER_FRAME_COUNTER_LIMIT - svs.numEntityStateSnapshots) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "numSnapshotEntities";
    } else if (svs.nextCachedSnapshotEntities >= SERVER_FRAME_CACHED_ENTITY_LIMIT) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "nextCachedSnapshotEntities";
    } else if (svs.nextCachedSnapshotClients >= SERVER_FRAME_CACHED_CLIENT_LIMIT) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "nextCachedSnapshotClients";
    } else if (svs.nextArchivedSnapshotFrames >= SERVER_FRAME_ARCHIVED_FRAME_LIMIT) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "nextArchivedSnapshotFrames";
    } else if (svs.nextArchivedSnapshotBuffer >= SERVER_FRAME_ARCHIVED_BUFFER_LIMIT) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "nextArchivedSnapshotBuffer";
    } else if (svs.nextCachedSnapshotFrames >= SERVER_FRAME_CACHED_FRAME_LIMIT) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "nextCachedSnapshotFrames";
    } else if (svs.nextClientSnapshot >= SERVER_FRAME_COUNTER_LIMIT - svs.numClientSnapshots) {
        restartReason = "EXE_SERVERRESTARTMISC\x15"
                        "numSnapshotClients";
    }

    if (restartReason != NULL) {
        char mapName[MAX_QPATH];
        Q_strncpyz(mapName, sv_mapname->string, sizeof(mapName));
        Com_Shutdown(restartReason);
        Cbuf_AddText(va("map %s\n", mapName));
        return;
    }

    XAnimSetUser(XANIM_USER_SERVER);
    if ((cvar_modifiedFlags & SERVER_FRAME_SERVERINFO_FLAGS) != 0) {
        SV_SetConfigstring(CS_SERVERINFO, Cvar_InfoString(SERVER_FRAME_SERVERINFO_FLAGS));
        cvar_modifiedFlags &= ~SERVER_FRAME_SERVERINFO_FLAGS;
    }
    if ((cvar_modifiedFlags & SERVER_FRAME_SYSTEMINFO_FLAGS) != 0) {
        SV_SetConfigstring(CS_SYSTEMINFO, Cvar_InfoString_Big(SERVER_FRAME_SYSTEMINFO_FLAGS));
        cvar_modifiedFlags &= ~SERVER_FRAME_SYSTEMINFO_FLAGS;
    }
    if ((cvar_modifiedFlags & SERVER_FRAME_SCRIPT_CONFIGSTRING_FLAGS) != 0) {
        Cvar_SetConfigstringValues(CS_CONFIGVALUE_NAMES, CS_CONFIGVALUE_COUNT, SERVER_FRAME_SCRIPT_CONFIGSTRING_FLAGS);
        cvar_modifiedFlags &= ~SERVER_FRAME_SCRIPT_CONFIGSTRING_FLAGS;
    }

    SV_RunBotFrame();
    int32_t frameStartTime = 0;
    if (com_speeds->integer != 0) {
        frameStartTime = SERVER_FRAME_MILLISECONDS();
    }

    SV_CalcPings();
    const int32_t scaledFrameMsec = Com_ModifyMsec(frameMsec);
    for (;;) {
        sv_timeResidual -= frameMsec;
        svs.realTime += frameMsec;
        svs.time += scaledFrameMsec;
        SERVER_FRAME_PRE_GAME_VM();
        SV_RunFrame();
        Scr_SetLoading(qfalse);

        if (frameMsec < 1 || sv_timeResidual < frameMsec) {
            break;
        }
        SV_ArchiveSnapshot();
    }

    if (com_speeds->integer != 0) {
        com_timeGame = SERVER_FRAME_MILLISECONDS() - frameStartTime;
    }

    SV_CheckTimeouts();
    SV_SendClientMessages();
    if (com_timescale->value > 0.0f) {
        SV_ArchiveSnapshot();
    }
    SV_MasterHeartbeat("COD-1");
}
