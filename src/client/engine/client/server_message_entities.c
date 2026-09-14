#include "cgame.h"
#include "console.h"
#include "download.h"
#include "server_browser.h"

#include "client/common/client_legacy_crt.h"
#include "filesystem/filesystem.h"
#include "../filesystem/server_namespace.h"
#include "../math/vector_math.h"
#include "../networking/net_address.h"
#include "../physics/cm_trace.h"
#include "qcommon/fx_types.h"
#include "qcommon/q_string.h"
#include "../system_platform.h"

#include <stdlib.h>
#include <string.h>

enum {
    CL_SERVER_MESSAGE_MAX_BYTES = 32768,
    CL_PACKET_ENTITY_NUMBER_BITS = 10,
    CL_PACKET_CLIENT_NUMBER_BITS = 6,
    CL_PACKET_ENTITY_END = MAX_GENTITIES - 1,
    CL_PARSE_OLD_NUMBER_END = 99999,
    CL_SHOWNET_PACKET_DELTAS = 3,
    CL_ENTITY_RECENTLY_VISIBLE_MSEC = 600
};

enum {
    CL_SNAPSHOT_BACKUP_MASK = CODUO_SNAPSHOT_BACKUP_COUNT - 1,
    CL_SNAPSHOT_PARSE_SAFETY_MARGIN = 128,
    CL_SNAPSHOT_DEFAULT_PING = 999,
    CL_SHOWNET_SNAPSHOT_DETAILS = 3
};

enum {
    CL_SYSTEMINFO_CONFIGSTRING = 1,
    CL_SYSTEMINFO_PAIR_CAPACITY = BIG_INFO_STRING
};

enum {
    CL_GAMESTATE_STRING_DATA_START = 1
};

typedef enum clWwwDownloadFlag_e {
    CL_WWW_DOWNLOAD_DISCONNECT = 1,
    CL_WWW_DOWNLOAD_OPEN_URL = 2
} clWwwDownloadFlag_t;

typedef enum clServerMessageCommand_e {
    CL_SVC_READ_EXHAUSTED = -1,
    CL_SVC_BAD = 0,
    CL_SVC_NOP = 1,
    CL_SVC_GAMESTATE = 2,
    CL_SVC_CONFIGSTRING = 3,
    CL_SVC_BASELINE = 4,
    CL_SVC_SERVER_COMMAND = 5,
    CL_SVC_DOWNLOAD = 6,
    CL_SVC_SNAPSHOT = 7,
    CL_SVC_EOF = 8
} clServerMessageCommand_t;

#define CL_ENTITY_FLAG_NODRAW ((uint32_t)0x80)

/* Original Win32 storage at 0x04957f60. The visibility filter records the
 * server time at which each player entity was last trace-visible. */
int32_t cl_entityLastVisibleTime[MAX_CLIENTS];

/* Source: CoDUOMP.exe 0x00417ba0..0x00417bc2.
 * Name: exact same-module Mac symbol SHOWNET. */
void SHOWNET(const msg_t *message, const char *label)
{
    if (cl_shownet->integer >= 2) {
        Com_Printf("%3i:%s\n", message->readcount - 1, label);
    }
}

/* Source: CoDUOMP.exe 0x00417bd0..0x00418001.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417bd0_00418002.mcode.
 * Name and source-level vector construction: same-module Mac symbol
 * isEntVisible. Windows proves the seven CM_BoxTrace targets and uses only
 * CONTENTS_SOLID against the world collision model. */
qboolean isEntVisible(const entityState_t *entity)
{
    vec3_t viewOrigin = {
        cl.inputState.clientLerpOrigin[0],
        cl.inputState.clientLerpOrigin[1],
        cl.inputState.clientLerpOrigin[2]
    };
    viewOrigin[2] += cl.snap.ps.viewHeightCurrent - 1.0f;
    AddLeanToPosition(
        viewOrigin, cl.snap.ps.viewAngles[1],
        cl.snap.ps.leanFraction, 16.0f, 20.0f);

    const vec3_t entityOrigin = {
        entity->pos.trBase[0],
        entity->pos.trBase[1],
        entity->pos.trBase[2]
    };
    vec3_t direction = {
        entityOrigin[0] - viewOrigin[0],
        entityOrigin[1] - viewOrigin[1],
        entityOrigin[2] - viewOrigin[2]
    };
    VectorNormalizeFast(direction);

    vec3_t nearSideOffset;
    CrossProductUp(direction, nearSideOffset);
    VectorNormalizeFast(nearSideOffset);

    vec3_t farSideOffset = {
        nearSideOffset[0] * 18.0f,
        nearSideOffset[1] * 18.0f,
        nearSideOffset[2] * 18.0f
    };
    for (int32_t component = 0; component < 3; ++component)
        nearSideOffset[component] *= 10.0f;

    const float centerHeight =
        entity->animMovetype != 0 ? 16.0f : 40.0f;
    vec3_t target = {
        entityOrigin[0],
        entityOrigin[1],
        entityOrigin[2] + centerHeight
    };
    trace_t trace;

    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    if (trace.fraction == 1.0f)
        return qtrue;

    target[2] += 16.0f;
    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    if (trace.fraction == 1.0f)
        return qtrue;

    target[2] -= 16.0f;
    target[2] -= centerHeight;
    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    if (trace.fraction == 1.0f)
        return qtrue;

    target[0] = entityOrigin[0] + nearSideOffset[0];
    target[1] = entityOrigin[1] + nearSideOffset[1];
    target[2] = entityOrigin[2] + nearSideOffset[2];
    target[2] += 8.0f;
    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    if (trace.fraction == 1.0f)
        return qtrue;

    target[0] = entityOrigin[0] + farSideOffset[0];
    target[1] = entityOrigin[1] + farSideOffset[1];
    target[2] = entityOrigin[2] + farSideOffset[2];
    target[2] += entity->animMovetype != 0 ? 28.0f : 52.0f;
    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    if (trace.fraction == 1.0f)
        return qtrue;

    for (int32_t component = 0; component < 3; ++component) {
        nearSideOffset[component] *= -1.0f;
        farSideOffset[component] *= -1.0f;
    }

    target[0] = entityOrigin[0] + farSideOffset[0];
    target[1] = entityOrigin[1] + farSideOffset[1];
    target[2] = entityOrigin[2] + farSideOffset[2];
    target[2] += 2.0f;
    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    if (trace.fraction == 1.0f)
        return qtrue;

    target[0] = entityOrigin[0] + nearSideOffset[0];
    target[1] = entityOrigin[1] + nearSideOffset[1];
    target[2] = entityOrigin[2] + nearSideOffset[2];
    target[2] += entity->animMovetype != 0 ? 16.0f : 36.0f;
    trace.fraction = 1.0f;
    CM_BoxTrace(
        &trace, viewOrigin, target, NULL, NULL,
        CM_WORLD_MODEL, CONTENTS_SOLID, qfalse);
    return trace.fraction == 1.0f ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x00418010..0x004180b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00418010_004180b4.mcode.
 * Name and parameter roles: same-module Mac symbol CL_DeltaEntity and the
 * calls at 0x00418316/0x00418389. */
void CL_DeltaEntity(msg_t *message, clSnapshot_t *frame,
                    int32_t newNumber, const entityState_t *oldEntity,
                    qboolean unchanged)
{
    entityState_t *const entity =
        &cl.parseEntities[
            (uint32_t)cl.parseEntitySequence &
            (CODUO_PARSE_RING_COUNT - 1)];

    if (unchanged != qfalse) {
        *entity = *oldEntity;
    } else if (MSG_ReadDeltaEntity(message, oldEntity, entity,
                                   newNumber) != qfalse) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((entity->eType == ET_PLAYER ||
         entity->eType == ET_PLAYER_CORPSE) &&
        (uint32_t)entity->clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CL_DeltaEntity: invalid client number %i",
                  entity->clientNum);
        return;
    }

    if (clc.onlyVisibleClients != qfalse &&
        entity->number < MAX_CLIENTS) {
        if (isEntVisible(entity) != qfalse) {
            cl_entityLastVisibleTime[entity->number] = frame->serverTime;
            entity->eFlags &= ~CL_ENTITY_FLAG_NODRAW;
        } else if (
            cl_entityLastVisibleTime[entity->number] <
            (int32_t)((uint32_t)frame->serverTime -
                      CL_ENTITY_RECENTLY_VISIBLE_MSEC)) {
            entity->eFlags |= CL_ENTITY_FLAG_NODRAW;
        }
    }

    ++cl.parseEntitySequence;
    ++frame->numEntities;
}

/* Source: CoDUOMP.exe 0x004180c0..0x00418145.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004180c0_00418146.mcode.
 * Name and parameter roles: same-module Mac symbol CL_DeltaClient and the
 * calls at 0x004186cb/0x0041876a. */
void CL_DeltaClient(msg_t *message, clSnapshot_t *frame,
                    int32_t newNumber, const clientState_t *oldClient,
                    qboolean unchanged)
{
    clientState_t *const client =
        &cl.parseClients[
            (uint32_t)cl.parseClientSequence &
            (CODUO_PARSE_RING_COUNT - 1)];

    if (unchanged != qfalse) {
        *client = *oldClient;
    } else if (MSG_ReadDeltaClient(message, oldClient, client,
                                   newNumber) != qfalse) {
        return;
    }

    ++cl.parseClientSequence;
    ++frame->numClients;
}

static const entityState_t *CL_OldPacketEntity(
    const clSnapshot_t *oldFrame, int32_t oldIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: typed source-level spelling of the repeated
     * old-snapshot ring lookup in CL_ParsePacketEntities. */
    if (oldFrame == NULL || oldIndex >= oldFrame->numEntities)
        return NULL;

    return &cl.parseEntities[
        ((uint32_t)oldFrame->firstEntitySequence +
         (uint32_t)oldIndex) &
        (CODUO_PARSE_RING_COUNT - 1)];
}

/* Source: CoDUOMP.exe 0x00418150..0x004184d3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00418150_004184d4.mcode.
 * Name and signature: same-module Mac symbol CL_ParsePacketEntities. */
void CL_ParsePacketEntities(msg_t *message,
                            const clSnapshot_t *oldFrame,
                            clSnapshot_t *newFrame)
{
    newFrame->firstEntitySequence = cl.parseEntitySequence;
    newFrame->numEntities = 0;

    int32_t oldIndex = 0;
    const entityState_t *oldEntity =
        CL_OldPacketEntity(oldFrame, oldIndex);
    int32_t oldNumber = oldEntity != NULL
        ? oldEntity->number
        : CL_PARSE_OLD_NUMBER_END;

    int32_t newNumber =
        MSG_ReadBits(message, CL_PACKET_ENTITY_NUMBER_BITS);
    while (newNumber != CL_PACKET_ENTITY_END) {
        if (message->readcount > message->cursize) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CL_ParsePacketEntities: end of message");
        }

        while (oldNumber < newNumber) {
            if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
                Com_Printf("%3i:  unchanged: %i\n",
                           message->readcount, oldNumber);
            }
            CL_DeltaEntity(message, newFrame, oldNumber, oldEntity,
                           qtrue);

            ++oldIndex;
            oldEntity = CL_OldPacketEntity(oldFrame, oldIndex);
            oldNumber = oldEntity != NULL
                ? oldEntity->number
                : CL_PARSE_OLD_NUMBER_END;
        }

        if (oldNumber == newNumber) {
            if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
                Com_Printf("%3i:  delta: %i\n",
                           message->readcount, newNumber);
            }
            CL_DeltaEntity(message, newFrame, newNumber, oldEntity,
                           qfalse);

            ++oldIndex;
            oldEntity = CL_OldPacketEntity(oldFrame, oldIndex);
            oldNumber = oldEntity != NULL
                ? oldEntity->number
                : CL_PARSE_OLD_NUMBER_END;
        } else {
            if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
                Com_Printf("%3i:  baseline: %i\n",
                           message->readcount, newNumber);
            }
            CL_DeltaEntity(message, newFrame, newNumber,
                           &cl.entityBaselines[newNumber], qfalse);
        }

        newNumber =
            MSG_ReadBits(message, CL_PACKET_ENTITY_NUMBER_BITS);
    }

    while (oldNumber != CL_PARSE_OLD_NUMBER_END) {
        if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
            Com_Printf("%3i:  unchanged: %i\n",
                       message->readcount, oldNumber);
        }
        CL_DeltaEntity(message, newFrame, oldNumber, oldEntity, qtrue);

        ++oldIndex;
        oldEntity = CL_OldPacketEntity(oldFrame, oldIndex);
        oldNumber = oldEntity != NULL
            ? oldEntity->number
            : CL_PARSE_OLD_NUMBER_END;
    }

    if (cl_shownuments->integer != 0)
        Com_Printf("Entities in packet: %i\n", newFrame->numEntities);
}

static const clientState_t *CL_OldPacketClient(
    const clSnapshot_t *oldFrame, int32_t oldIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: typed source-level spelling of the repeated
     * old-snapshot ring lookup in CL_ParsePacketClients. */
    if (oldFrame == NULL || oldIndex >= oldFrame->numClients)
        return NULL;

    return &cl.parseClients[
        ((uint32_t)oldFrame->firstClientSequence +
         (uint32_t)oldIndex) &
        (CODUO_PARSE_RING_COUNT - 1)];
}

/* Source: CoDUOMP.exe 0x004184e0..0x00418867.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004184e0_00418868.mcode.
 * Name and signature: same-module Mac symbol CL_ParsePacketClients. */
void CL_ParsePacketClients(msg_t *message,
                           const clSnapshot_t *oldFrame,
                           clSnapshot_t *newFrame)
{
    newFrame->firstClientSequence = cl.parseClientSequence;
    newFrame->numClients = 0;

    int32_t oldIndex = 0;
    const clientState_t *oldClient =
        CL_OldPacketClient(oldFrame, oldIndex);
    int32_t oldNumber = oldClient != NULL
        ? oldClient->clientNum
        : CL_PARSE_OLD_NUMBER_END;

    while (MSG_ReadBit(message) != 0) {
        const int32_t newNumber =
            MSG_ReadBits(message, CL_PACKET_CLIENT_NUMBER_BITS);
        if (message->readcount > message->cursize) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CL_ParsePacketClients: end of message");
        }

        while (oldNumber < newNumber) {
            if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
                Com_Printf("%3i:  unchanged: %i\n",
                           message->readcount, oldNumber);
            }
            CL_DeltaClient(message, newFrame, oldNumber, oldClient,
                           qtrue);

            ++oldIndex;
            oldClient = CL_OldPacketClient(oldFrame, oldIndex);
            oldNumber = oldClient != NULL
                ? oldClient->clientNum
                : CL_PARSE_OLD_NUMBER_END;
        }

        if (oldNumber == newNumber) {
            if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
                Com_Printf("%3i:  delta: %i\n",
                           message->readcount, newNumber);
            }
            CL_DeltaClient(message, newFrame, newNumber, oldClient,
                           qfalse);

            ++oldIndex;
            oldClient = CL_OldPacketClient(oldFrame, oldIndex);
            oldNumber = oldClient != NULL
                ? oldClient->clientNum
                : CL_PARSE_OLD_NUMBER_END;
        } else {
            clientState_t nullClient = {0};
            if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
                Com_Printf("%3i:  baseline: %i\n",
                           message->readcount, newNumber);
            }
            CL_DeltaClient(message, newFrame, newNumber, &nullClient,
                           qfalse);
        }
    }

    while (oldNumber != CL_PARSE_OLD_NUMBER_END) {
        if (cl_shownet->integer == CL_SHOWNET_PACKET_DELTAS) {
            Com_Printf("%3i:  unchanged: %i\n",
                       message->readcount, oldNumber);
        }
        CL_DeltaClient(message, newFrame, oldNumber, oldClient, qtrue);

        ++oldIndex;
        oldClient = CL_OldPacketClient(oldFrame, oldIndex);
        oldNumber = oldClient != NULL
            ? oldClient->clientNum
            : CL_PARSE_OLD_NUMBER_END;
    }

    if (cl_shownuments->integer != 0)
        Com_Printf("Clients in packet: %i\n", newFrame->numClients);
}

/* Source: CoDUOMP.exe 0x00418870..0x00418b7a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00418870_00418b7b.mcode.
 * Name and signature: same-module Mac symbol CL_ParseSnapshot. The Windows
 * frame is one complete clSnapshot_t; its 0x4534-byte copy into cl.snap and
 * the 32-entry snapshot ring prove the retained native layout. */
void CL_ParseSnapshot(msg_t *message)
{
    clSnapshot_t newSnapshot = {0};
    const clSnapshot_t *oldSnapshot = NULL;

    newSnapshot.serverCommandSequence = clc.serverCommandSequence;
    newSnapshot.serverTime = MSG_ReadLong(message);
    newSnapshot.messageNum = clc.serverMessageSequence;

    const int32_t deltaDistance = MSG_ReadByte(message);
    if (deltaDistance == 0) {
        newSnapshot.deltaNum = -1;
    } else {
        newSnapshot.deltaNum = (int32_t)(
            (uint32_t)newSnapshot.messageNum -
            (uint32_t)deltaDistance);
    }
    newSnapshot.snapFlags = (uint32_t)MSG_ReadByte(message);

    if (newSnapshot.deltaNum <= 0) {
        clc.demoWaiting = qfalse;
        newSnapshot.valid = qtrue;
    } else {
        oldSnapshot =
            &cl.snapshots[
                (uint32_t)newSnapshot.deltaNum &
                CL_SNAPSHOT_BACKUP_MASK];

        /* On every failure branch the stale ring entry remains the delta
         * parent so the message stream is still consumed record for record;
         * valid stays qfalse and the finished frame is discarded below
         * (0x0041897a..0x004189eb keep ESI, only the success paths reach the
         * valid store at 0x00418922). */
        if (oldSnapshot->valid == qfalse) {
            Com_Printf(
                "Delta from invalid frame (not supposed to happen!).\n");
        } else if (oldSnapshot->messageNum != newSnapshot.deltaNum) {
            Com_DPrintf("Delta frame too old.\n");
        } else if (
            (int32_t)((uint32_t)cl.parseEntitySequence -
                      (uint32_t)oldSnapshot->firstEntitySequence) >
            CODUO_PARSE_RING_COUNT -
                CL_SNAPSHOT_PARSE_SAFETY_MARGIN) {
            Com_DPrintf("Delta parseEntitiesNum too old.\n");
        } else if (
            (int32_t)((uint32_t)cl.parseClientSequence -
                      (uint32_t)oldSnapshot->firstClientSequence) >
            CODUO_PARSE_RING_COUNT -
                CL_SNAPSHOT_PARSE_SAFETY_MARGIN) {
            Com_DPrintf("Delta parseClientsNum too old.\n");
        } else {
            newSnapshot.valid = qtrue;
        }
    }

    SHOWNET(message, "playerstate");
    MSG_ReadDeltaPlayerstate(
        message,
        oldSnapshot != NULL ? &oldSnapshot->ps : NULL,
        &newSnapshot.ps);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)newSnapshot.ps.psClientNum >=
        (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CL_ParseSnapshot: invalid player client number %i",
                  newSnapshot.ps.psClientNum);
        return;
    }

    SHOWNET(message, "packet entities");
    CL_ParsePacketEntities(message, oldSnapshot, &newSnapshot);

    SHOWNET(message, "packet clients");
    CL_ParsePacketClients(message, oldSnapshot, &newSnapshot);

    /* 0x00418a62 tests the frame dword at +0x00 (valid) before the argument
     * pop; a frame parsed against a lost or aged delta parent is dropped
     * here without touching cl.snap or the snapshot ring. */
    if (newSnapshot.valid == qfalse)
        return;

    int32_t invalidateMessageNum = cl.snap.messageNum + 1;
    if ((int32_t)((uint32_t)newSnapshot.messageNum -
                  (uint32_t)invalidateMessageNum) >=
        CODUO_SNAPSHOT_BACKUP_COUNT) {
        invalidateMessageNum =
            newSnapshot.messageNum -
            (CODUO_SNAPSHOT_BACKUP_COUNT - 1);
    }
    while (invalidateMessageNum < newSnapshot.messageNum) {
        cl.snapshots[
            (uint32_t)invalidateMessageNum &
            CL_SNAPSHOT_BACKUP_MASK].valid = qfalse;
        ++invalidateMessageNum;
    }

    cl.previousSnapshotServerTime = cl.snap.serverTime;
    cl.snap = newSnapshot;
    cl.snap.ping = CL_SNAPSHOT_DEFAULT_PING;

    int32_t packetSequence = clc.netchan.outgoingSequence - 1;
    for (int32_t packetAge = 0;
         packetAge < CODUO_SNAPSHOT_BACKUP_COUNT;
         ++packetAge, --packetSequence) {
        const clOutPacket_t *const packet =
            &cl.outPackets[
                (uint32_t)packetSequence &
                CL_SNAPSHOT_BACKUP_MASK];
        if (cl.snap.ps.commandTime >= packet->lastCommandTime) {
            cl.snap.ping = cls.realtime - packet->sendRealTime;
            break;
        }
    }

    cl.snapshots[
        (uint32_t)cl.snap.messageNum &
        CL_SNAPSHOT_BACKUP_MASK] = cl.snap;

    if (cl_shownet->integer == CL_SHOWNET_SNAPSHOT_DETAILS) {
        Com_Printf("   snapshot:%i  delta:%i  ping:%i\n",
                   cl.snap.messageNum, cl.snap.deltaNum, cl.snap.ping);
    }
    cl.newSnapshots = qtrue;
}

/* Source: CoDUOMP.exe 0x00418d10..0x00418f8f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00418d10_00418f90.mcode.
 * Name and signature: same-module Mac symbol CL_ParseGamestate. Configstrings
 * and entity baselines are the two commands legal inside a gamestate message;
 * the trailing client number and checksum feed become connection state only
 * after the embedded command stream reaches svc_EOF. */
void CL_ParseGamestate(msg_t *message)
{
    Con_Close();
    CL_ClearState();
    clc.connectPacketCount = 0;

    clc.serverCommandSequence = MSG_ReadLong(message);
    cl.gameState.dataCount = CL_GAMESTATE_STRING_DATA_START;

    for (;;) {
        const clServerMessageCommand_t command =
            (clServerMessageCommand_t)MSG_ReadByte(message);
        if (command == CL_SVC_EOF)
            break;

        if (command == CL_SVC_CONFIGSTRING) {
            const int32_t configstringIndex = MSG_ReadShort(message);
            if (configstringIndex < 0 ||
                configstringIndex >= MAX_CONFIGSTRINGS) {
                Com_Error(
                    ERR_DROP,
                    "\x15" "configstring > MAX_CONFIGSTRINGS");
            }

            const char *const value = MSG_ReadBigString(message);
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (configstringIndex >= CS_EFFECTS && configstringIndex < CS_FX &&
                strcspn(value, ".") >= FX_EFFECT_TEMPLATE_NAME_CAPACITY) {
                Com_Error(ERR_DROP, "\x15" "CL_ParseGamestate: effect name is too long");
            }

            const size_t valueBytes = strlen(value) + 1;
            if ((size_t)cl.gameState.dataCount + valueBytes >
                MAX_GAMESTATE_CHARS) {
                Com_Error(
                    ERR_DROP,
                    "\x15MAX_GAMESTATE_CHARS exceeded");
            }

            cl.gameState.stringOffsets[configstringIndex] =
                cl.gameState.dataCount;
            memcpy(
                &cl.gameState.stringData[cl.gameState.dataCount],
                value, valueBytes);
            cl.gameState.dataCount += (int32_t)valueBytes;
            continue;
        }

        if (command == CL_SVC_BASELINE) {
            const int32_t entityNumber =
                MSG_ReadBits(message, CL_PACKET_ENTITY_NUMBER_BITS);
            if (entityNumber < 0 ||
                entityNumber >= MAX_GENTITIES) {
                Com_Error(
                    ERR_DROP,
                    "\x15" "Baseline number out of range: %i",
                    entityNumber);
            }

            entityState_t nullEntity = {0};
            (void)MSG_ReadDeltaEntity(
                message, &nullEntity,
                &cl.entityBaselines[entityNumber], entityNumber);
            continue;
        }

        Com_Error(
            ERR_DROP,
            "\x15" "CL_ParseGamestate: bad command byte");
    }

    clc.clientNum = MSG_ReadLong(message);
    clc.checksumFeed = MSG_ReadLong(message);

    const char *serverDisplayName = cls.serverName;
    const char *const serverInfo =
        &cl.gameState.stringData[
            cl.gameState.stringOffsets[CS_SERVERINFO]];
    const char *advertisedName =
        Info_ValueForKey(serverInfo, "sv_hostname");
    if (advertisedName[0] == '\0')
        advertisedName = Info_ValueForKey(serverInfo, "hostname");
    if (advertisedName[0] != '\0')
        serverDisplayName = advertisedName;
    const qboolean namespaceChanged =
        coduomp_server_namespace_activate(
            &clc.serverAddress, serverDisplayName,
            sv_running->integer == 0 &&
                    clc.demoPlayback == qfalse &&
                    cl_updateStarted == qfalse
                ? qtrue
                : qfalse);

    CL_SystemInfoChanged();
    /* NOT_FROM_ORIGINAL_SOURCE: seed the newly selected isolated namespace
     * from checksum-matched ordinary-root paks before restart/download. */
    const qboolean cachedRootPaks =
        coduomp_server_namespace_cache_referenced_paks();

    if (sv_running->integer == 0) {
        if (namespaceChanged != qfalse || cachedRootPaks != qfalse)
            FS_Restart(clc.checksumFeed);
        else if (fs_game->modified != qfalse ||
                 clc.checksumFeed != fs_checksumFeed)
            (void)FS_ConditionalRestart(clc.checksumFeed);
    }

    if (net_lanauthorize->integer != 0 ||
        Sys_IsLANAddress(clc.serverAddress) == qfalse) {
        CL_RequestAuthorization();
    }
    CL_InitDownloads();
    Cvar_Set("cl_paused", "0");
}

/* Source: CoDUOMP.exe 0x00418f90..0x0041949a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00418f90_0041949b.mcode.
 * Name and signature: same-module Mac symbol CL_ParseDownload. A block number
 * of -1 introduces the WWW redirect record; all other values belong to the
 * ordered in-band block stream. The two streams deliberately share the
 * download names and completion state but have different file ownership.
 * The redirect-refusal search at 0x00419108 uses badChecksumList
 * (0x04de91c8), not the successful redirectedList (0x04de8dc8). */
void CL_ParseDownload(msg_t *message)
{
    uint8_t blockData[CL_SERVER_MESSAGE_MAX_BYTES];
    const int32_t blockNumber = MSG_ReadShort(message);

    if (blockNumber == -1) {
        if (clc.wwwDownloadActive != qfalse) {
            (void)MSG_ReadString(message);
            (void)MSG_ReadLong(message);
            (void)MSG_ReadLong(message);
            return;
        }

        strncpy(
            cls.staticDownload.originalDownloadName,
            cls.staticDownload.downloadName,
            sizeof(cls.staticDownload.originalDownloadName) - 1);
        cls.staticDownload.originalDownloadName[
            sizeof(cls.staticDownload.originalDownloadName) - 1] = '\0';

        const char *const redirectedUrl = MSG_ReadString(message);
        strncpy(
            cls.staticDownload.downloadName, redirectedUrl,
            sizeof(cls.staticDownload.downloadName) - 1);
        cls.staticDownload.downloadName[
            sizeof(cls.staticDownload.downloadName) - 1] = '\0';

        clc.downloadSize = MSG_ReadLong(message);
        clc.downloadFlags = MSG_ReadLong(message);

        if ((clc.downloadFlags & CL_WWW_DOWNLOAD_OPEN_URL) != 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: ordinary package redirects remain
             * downloads; only the selected update service may delegate a
             * target to the host URL handler. */
            if (cl_updateStarted == qfalse ||
                NET_CompareAdrSigned(
                    &clc.serverAddress, &cls.autoUpdateServer) != 0) {
                Com_Printf(
                    "Refusing external open for package download\n");
                CL_AddReliableCommand("wwwdl fail");
                clc.wwwDownloadAborting = qtrue;
                return;
            }
            Sys_OpenURL(cls.staticDownload.downloadName, qtrue);
            Cbuf_AddText("quit\n");
            CL_AddReliableCommand("wwwdl bbl8r");
            clc.wwwDownloadAborting = qtrue;
            return;
        }

        Cvar_SetValue(
            "cl_downloadSize", (float)clc.downloadSize);
        Com_DPrintf(
            "Server redirected download: %s\n",
            cls.staticDownload.downloadName);
        clc.wwwDownloadActive = qtrue;
        CL_AddReliableCommand("wwwdl ack");

        if (strstr(
                clc.badChecksumList,
                va("@%s",
                   cls.staticDownload.originalDownloadName)) != NULL) {
            Com_Printf(
                "refusing redirect to %s by server "
                "(bad checksum)\n",
                cls.staticDownload.downloadName);
            CL_AddReliableCommand("wwwdl fail");
            clc.wwwDownloadAborting = qtrue;
            return;
        }

        char localTempPath[MAX_OSPATH];
        if (coduomp_server_namespace_build_download_path(
                cls.staticDownload.downloadTempName,
                localTempPath, sizeof(localTempPath)) == qfalse) {
            CL_AddReliableCommand("wwwdl fail");
            clc.wwwDownloadAborting = qtrue;
            Com_Printf("Refusing invalid redirected download path\n");
            return;
        }
        strncpy(
            cls.staticDownload.downloadTempName, localTempPath,
            sizeof(cls.staticDownload.downloadTempName) - 1);
        cls.staticDownload.downloadTempName[
            sizeof(cls.staticDownload.downloadTempName) - 1] = '\0';
        if (DL_BeginDownload(
                cls.staticDownload.downloadTempName,
                cls.staticDownload.downloadName) == qfalse) {
            CL_AddReliableCommand("wwwdl fail");
            clc.wwwDownloadAborting = qtrue;
            Com_Printf(
                "Failed to initialize download for '%s'\n",
                cls.staticDownload.downloadName);
        }

        if ((clc.downloadFlags &
             CL_WWW_DOWNLOAD_DISCONNECT) != 0) {
            CL_AddReliableCommand("wwwdl bbl8r");
            cls.wwwDownloadDisconnected = qtrue;
        }
        return;
    }

    if (blockNumber == 0) {
        clc.downloadSize = MSG_ReadLong(message);
        Cvar_SetValue(
            "cl_downloadSize", (float)clc.downloadSize);
        if (clc.downloadSize < 0) {
            const char *const errorMessage = MSG_ReadString(message);
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            Com_Error(ERR_DROP, "%s", errorMessage);
        }
    }

    const int32_t blockLength = MSG_ReadShort(message);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (blockLength < 0) {
        Com_Error(
            ERR_DROP,
            "\x15" "CL_ParseDownload: invalid block length %i",
            blockLength);
    }

    if (blockLength > 0)
        MSG_ReadData(message, blockData, blockLength);

    if (clc.downloadBlock != blockNumber) {
        Com_DPrintf(
            "CL_ParseDownload: Expected block %d, got %d\n",
            clc.downloadBlock, blockNumber);
        if (blockNumber <= clc.downloadBlock)
            return;

        Com_DPrintf(
            "CL_ParseDownload: Sending retransmitt request "
            "to get the missed block\n");
        CL_AddReliableCommand(
            va("retransdl %d", clc.downloadBlock));
        return;
    }

    if (clc.downloadFile == 0) {
        if (cls.staticDownload.downloadTempName[0] == '\0') {
            Com_Printf(
                "Server sending download, but no download was "
                "requested\n");
            CL_AddReliableCommand("stopdl");
            return;
        }

        clc.downloadFile = coduomp_server_namespace_open_download_write(
            cls.staticDownload.downloadTempName);
        if (clc.downloadFile == 0) {
            Com_Printf(
                "Could not create %s\n",
                cls.staticDownload.downloadTempName);
            CL_AddReliableCommand("stopdl");
            CL_NextDownload();
            return;
        }
    }

    if (blockLength != 0) {
        (void)FS_Write(
            blockData, blockLength, clc.downloadFile);
    }

    CL_AddReliableCommand(
        va("nextdl %d", clc.downloadBlock));
    clc.downloadCount += blockLength;
    ++clc.downloadBlock;
    Cvar_SetValue(
        "cl_downloadCount", (float)clc.downloadCount);

    if (blockLength != 0)
        return;

    if (clc.downloadFile != 0) {
        FS_FCloseFile(clc.downloadFile);
        clc.downloadFile = 0;
        coduomp_server_namespace_rename_download(
            cls.staticDownload.downloadTempName,
            cls.staticDownload.downloadName);
    }

    (void)Cvar_Set2(
        "cl_downloadName", "", qtrue);
    cls.staticDownload.downloadName[0] = '\0';
    cls.staticDownload.downloadTempName[0] = '\0';
    SCR_UpdateScreen();
    SCR_UpdateScreen();
    CL_NextDownload();
}

/* Source: CoDUOMP.exe 0x00418b80..0x00418d01.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00418b80_00418d02.mcode.
 * Name and role: same-module Mac symbol CL_SystemInfoChanged. The system-info
 * configstring is index 1; the Windows source uses two 8192-byte Info_NextPair
 * work buffers before force-setting the received cvars. */
void CL_SystemInfoChanged(void)
{
    const char *const systemInfo =
        &cl.gameState.stringData[
            cl.gameState.stringOffsets[CL_SYSTEMINFO_CONFIGSTRING]];

    cl.serverId = coduo_crt_atoi(Info_ValueForKey(systemInfo, "sv_serverid"));
    memset(cl_entityLastVisibleTime, 0,
           sizeof(cl_entityLastVisibleTime));

    if (clc.demoPlayback != qfalse)
        return;

    if (coduo_crt_atoi(Info_ValueForKey(systemInfo, "sv_cheats")) == 0)
        Cvar_SetCheatState();

    const char *const loadedPakChecksums =
        Info_ValueForKey(systemInfo, "sv_paks");
    const char *const loadedPakNames =
        Info_ValueForKey(systemInfo, "sv_pakNames");
    FS_PureServerSetLoadedPaks(loadedPakChecksums, loadedPakNames);

    const char *const referencedPakChecksums =
        Info_ValueForKey(systemInfo, "sv_referencedPaks");
    const char *const referencedPakNames =
        Info_ValueForKey(systemInfo, "sv_referencedPakNames");
    FS_PureServerSetReferencedPaks(
        referencedPakChecksums, referencedPakNames);

    if (sv_running->integer == 0 && systemInfo != NULL) {
        const char *cursor = systemInfo;
        char key[CL_SYSTEMINFO_PAIR_CAPACITY];
        char value[CL_SYSTEMINFO_PAIR_CAPACITY];

        for (;;) {
            Info_NextPair(&cursor, key, value);
            if (key[0] == '\0')
                break;
            Cvar_Set(key, value);
        }
    }

    cl_connectedToPureServer =
        (qboolean)Cvar_VariableValue("sv_pure");
}

/* Source: CoDUOMP.exe 0x004194a0..0x004194f8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004194a0_004194f9.mcode.
 * Name and signature: same-module Mac symbol CL_ParseCommandString. */
void CL_ParseCommandString(msg_t *message)
{
    const int32_t sequence = MSG_ReadLong(message);
    const char *const command = MSG_ReadString(message);

    if (sequence <= clc.serverCommandSequence)
        return;

    clc.serverCommandSequence = sequence;
    Q_strncpyz(
        clc.serverCommands[
            (uint32_t)sequence &
            (CODUO_RELIABLE_COMMAND_COUNT - 1)],
        command, CODUO_RELIABLE_COMMAND_CAPACITY);
}

/* Source: CoDUOMP.exe 0x00419500..0x004196b0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419500_004196b1.mcode.
 * Name and signature: same-module Mac symbol CL_ParseServerMessage. The
 * Windows body inlines MSG_Init and the byte reader around the decompressed
 * temporary message. */
void CL_ParseServerMessage(msg_t *message)
{
    /* Original pointer table 0x005c45a0.
     * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows
     * all eight ordered service-command name pointers. */
    static const char *const commandNames[] = {
        "svc_bad",
        "svc_nop",
        "svc_gamestate",
        "svc_configstring",
        "svc_baseline",
        "svc_serverCommand",
        "svc_download",
        "svc_snapshot"
    };
    uint8_t decompressedData[CL_SERVER_MESSAGE_MAX_BYTES];
    msg_t decompressed;

    if (cl_shownet->integer == 1) {
        Com_Printf("%i ", message->cursize);
    } else if (cl_shownet->integer >= 2) {
        Com_Printf("------------------\n");
    }

    MSG_Init(&decompressed, decompressedData,
             (int32_t)sizeof(decompressedData));
    const int32_t decompressedSize = MSG_ReadBitsCompress(
        message->data + message->readcount, decompressed.data,
        message->cursize - message->readcount,
        (int32_t)sizeof(decompressedData));
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (decompressedSize == HUFFMAN_TRANSFORM_ERROR)
        return;
    decompressed.cursize = decompressedSize;

    for (;;) {
        if (decompressed.readcount > decompressed.cursize) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CL_ParseServerMessage: read past end of server message");
        }

        int32_t command;
        if (decompressed.readcount < decompressed.cursize) {
            command =
                decompressed.data[decompressed.readcount++];
        } else {
            /* The PE permits equality at the explicit bounds check, then its
             * inlined byte reader supplies the conventional -1 sentinel. */
            command = CL_SVC_READ_EXHAUSTED;
        }

        if (command == CL_SVC_EOF) {
            SHOWNET(&decompressed, "END OF MESSAGE");
            return;
        }

        if (cl_shownet->integer >= 2) {
            const char *commandName;
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if ((uint32_t)command <
                sizeof(commandNames) / sizeof(commandNames[0])) {
                commandName = commandNames[command];
            } else {
                /* Retail's following 248 dwords are zero-filled. */
                commandName = NULL;
            }
            if (commandName != NULL) {
                SHOWNET(&decompressed, commandName);
            } else {
                Com_Printf("%3i:BAD CMD %i\n",
                           decompressed.readcount - 1, command);
            }
        }

        switch ((clServerMessageCommand_t)command) {
        case CL_SVC_NOP:
            break;
        case CL_SVC_GAMESTATE:
            CL_ParseGamestate(&decompressed);
            break;
        case CL_SVC_SERVER_COMMAND:
            CL_ParseCommandString(&decompressed);
            break;
        case CL_SVC_DOWNLOAD:
            CL_ParseDownload(&decompressed);
            break;
        case CL_SVC_SNAPSHOT:
            CL_ParseSnapshot(&decompressed);
            break;
        case CL_SVC_BAD:
        case CL_SVC_CONFIGSTRING:
        case CL_SVC_BASELINE:
        case CL_SVC_EOF:
        default:
            Com_Error(
                ERR_DROP,
                "\x15"
                "CL_ParseServerMessage: Illegible server message %d\n",
                command);
            break;
        }
    }
}
