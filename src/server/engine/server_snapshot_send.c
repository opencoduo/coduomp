#include "server_snapshot_send.h"

#include "compat/coduo_x87emu.h"
#include "qcommon/huffman.h"
#include "qcommon/msg_delta.h"
#include "qcommon/net_text.h"
#include "qcommon/q_cvar.h"
#include "server_commands.h"
#include "server_client_message.h"
#include "server_download.h"
#include "server_game_data.h"
#include "server_netchan.h"
#include "server_snapshot_archive.h"

#include <stdint.h>
#include <string.h>

extern serverStatic_t svs;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_padPackets;
extern cvar_t *sv_showAverageBPS;

void Com_DPrintf(const char *format, ...);
void Com_Printf(const char *format, ...);
qboolean Sys_IsLANAddress(netadr_t address);
enum {
    SV_PACKET_END_SORT_KEY = 9999,
    SV_SNAPSHOT_ENTITY_NUMBER_BITS = 10,
    SV_SNAPSHOT_ENTITY_END_NUMBER = MAX_GENTITIES - 1,
    SV_SNAPSHOT_MAX_DELTA_FRAMES = SERVER_CLIENT_SNAPSHOT_FRAME_COUNT - 4,
    SV_SNAPSHOT_NO_DELTA = 0,
    SV_SNAPSHOT_RATE_DELAYED_FLAG = 1,
    SV_SNAPSHOT_NOT_ACTIVE_FLAG = 2,
    SV_SNAPSHOT_PAD_BYTE = 1,
    SV_RATE_MAX_ACCOUNTED_MESSAGE_BYTES = 1500,
    SV_RATE_MINIMUM_BYTES_PER_SECOND = 1000,
    SV_RATE_PACKET_OVERHEAD_BYTES = 48,
    SV_RATE_MILLISECONDS_PER_SECOND = 1000,
    SV_MESSAGE_ACKNOWLEDGEMENT_BYTES = sizeof(int32_t),
    SV_CONNECTING_CLIENT_MIN_SNAPSHOT_MSEC = 1000
};

/* Original 0x04907ab0..0x04907b68. SV_SendClientMessages shifts the two
 * 20-frame bandwidth histories and maintains their diagnostic maxima and
 * compression-ratio average when sv_showAverageBPS is enabled. The original
 * executable resets and samples sv_totalUncompressedBytesThisFrame but has no
 * direct instruction that increments it. */
int32_t sv_compressedBpsWindow[SERVER_AVERAGE_BPS_WINDOW_COUNT];
int32_t sv_averageBpsFrameCount;
int32_t sv_totalBytesSentThisFrame;
int32_t sv_compressedBpsMax;
int32_t sv_uncompressedBpsWindow[SERVER_AVERAGE_BPS_WINDOW_COUNT];
int32_t sv_totalUncompressedBytesThisFrame;
int32_t sv_uncompressedBpsMax;
float sv_averageCompressionRatioSum;
int32_t sv_averageCompressionRatioCount;

/*
 * Complete server snapshot-delta, compression, rate-scheduling, and delivery
 * subsystem shared by the Windows client engine and Linux dedicated engine:
 *
 * Function                    Windows       Linux
 * SV_EmitPacketEntities       0x00464000    0x080963b4
 * SV_EmitPacketClients        0x00464170    0x08096527
 * SV_WriteSnapshotToClient    0x00464390    0x08096673
 * SV_RateMsec                 0x00465d40    0x0809899f
 * SV_SendMessageToClient      0x00465da0    0x08098a23
 * SV_SendClientSnapshot       0x00465f40    0x08098c23
 * SV_SendClientMessages       0x00466bd0    0x08099a2c
 *
 * The supporting Mac client exports the same canonical names. The common
 * implementation retains the Windows operation graph where the two i386
 * compilers merely chose different spill patterns; those are not authored
 * platform behaviors.
 */

/* Source: CoDUOMP.exe 0x00464000..0x0046416e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464000_0046416f.mcode.
 * Name: exact same-module Mac symbol SV_EmitPacketEntities. The old and new
 * rings are sorted by entity number, so one merge walk emits unchanged,
 * introduced, and removed entities without serializing the ring order. */
void SV_EmitPacketEntities(int32_t oldNumEntities, int32_t oldFirstEntity, int32_t newNumEntities, int32_t newFirstEntity, msg_t *message)
{
    int32_t oldIndex = 0;
    int32_t newIndex = 0;

    while (newIndex < newNumEntities || oldIndex < oldNumEntities) {
        const entityState_t *oldEntity = NULL;
        const entityState_t *newEntity = NULL;
        int32_t oldEntityNum = SV_PACKET_END_SORT_KEY;
        int32_t newEntityNum = SV_PACKET_END_SORT_KEY;

        if (newIndex < newNumEntities) {
            newEntity = &svs.entityStateSnapshots[(newIndex + newFirstEntity) % svs.numEntityStateSnapshots];
            newEntityNum = newEntity->number;
        }

        if (oldIndex < oldNumEntities) {
            oldEntity = &svs.entityStateSnapshots[(oldIndex + oldFirstEntity) % svs.numEntityStateSnapshots];
            oldEntityNum = oldEntity->number;
        }

        if (newEntityNum == oldEntityNum) {
            MSG_WriteDeltaEntity(message, oldEntity, newEntity, qfalse);
            ++oldIndex;
            ++newIndex;
        } else if (newEntityNum < oldEntityNum) {
            MSG_WriteDeltaEntity(message, &sv_entities[newEntityNum].baseline.state, newEntity, qtrue);
            ++newIndex;
        } else {
            MSG_WriteDeltaEntity(message, oldEntity, NULL, qtrue);
            ++oldIndex;
        }
    }

    MSG_WriteBits(message, SV_SNAPSHOT_ENTITY_END_NUMBER, SV_SNAPSHOT_ENTITY_NUMBER_BITS);
}

/* Source: CoDUOMP.exe 0x00464170..0x0046438d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464170_0046438e.mcode.
 * Name: exact same-module Mac symbol SV_EmitPacketClients. The compiler
 * inlines MSG_WriteDeltaClient, including its zero baseline and force bit;
 * the maintained call preserves those proven semantics. */
void SV_EmitPacketClients(int32_t oldNumClients, int32_t oldFirstClient, int32_t newNumClients, int32_t newFirstClient, msg_t *message)
{
    int32_t oldIndex = 0;
    int32_t newIndex = 0;

    while (newIndex < newNumClients || oldIndex < oldNumClients) {
        const clientState_t *oldClient = NULL;
        const clientState_t *newClient = NULL;
        int32_t oldClientNum = SV_PACKET_END_SORT_KEY;
        int32_t newClientNum = SV_PACKET_END_SORT_KEY;

        if (newIndex < newNumClients) {
            newClient = &svs.clientSnapshots[(newIndex + newFirstClient) % svs.numClientSnapshots];
            newClientNum = newClient->clientNum;
        }

        if (oldIndex < oldNumClients) {
            oldClient = &svs.clientSnapshots[(oldIndex + oldFirstClient) % svs.numClientSnapshots];
            oldClientNum = oldClient->clientNum;
        }

        if (newClientNum == oldClientNum) {
            MSG_WriteDeltaClient(message, oldClient, newClient, qfalse);
            ++oldIndex;
            ++newIndex;
        } else if (newClientNum < oldClientNum) {
            MSG_WriteDeltaClient(message, NULL, newClient, qtrue);
            ++newIndex;
        } else {
            MSG_WriteDeltaClient(message, oldClient, NULL, qtrue);
            ++oldIndex;
        }
    }

    MSG_WriteBit0(message);
}

/* Source: CoDUOMP.exe 0x00464390..0x00464578.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00464390_00464579.mcode.
 * Name: exact same-module Mac symbol SV_WriteSnapshotToClient. A requested
 * delta is usable only while its frame and entity-ring entries both remain
 * live; otherwise the client receives a full player-state/entity snapshot. */
void SV_WriteSnapshotToClient(client_t *client, msg_t *message)
{
    clientSnapshot_t *const newFrame = &client->snapshotFrames[client->netchan.outgoingSequence & (SERVER_CLIENT_SNAPSHOT_FRAME_COUNT - 1)];
    clientSnapshot_t *oldFrame = NULL;
    int32_t deltaFrame = SV_SNAPSHOT_NO_DELTA;

    if (client->deltaMessage > SV_SNAPSHOT_NO_DELTA && client->state == CS_ACTIVE) {
        const int32_t requestedDeltaFrames = client->netchan.outgoingSequence - client->deltaMessage;

        if (requestedDeltaFrames <= SV_SNAPSHOT_MAX_DELTA_FRAMES) {
            oldFrame = &client->snapshotFrames[client->deltaMessage & (SERVER_CLIENT_SNAPSHOT_FRAME_COUNT - 1)];
            deltaFrame = requestedDeltaFrames;

            if (oldFrame->firstEntity < svs.nextEntityStateSnapshot - svs.numEntityStateSnapshots) {
                Com_DPrintf("%s: Delta request from out of date entities.\n", client->name);
                oldFrame = NULL;
                deltaFrame = SV_SNAPSHOT_NO_DELTA;
            }
        } else {
            Com_DPrintf("%s: Delta request from out of date packet.\n", client->name);
        }
    }

    MSG_WriteByte(message, SERVER_SVC_SNAPSHOT);
    MSG_WriteLong(message, svs.time);
    MSG_WriteByte(message, deltaFrame);

    int32_t snapshotFlags = svs.snapFlagServerBit;
    if (client->rateDelayed) {
        snapshotFlags |= SV_SNAPSHOT_RATE_DELAYED_FLAG;
    }

    if (client->state == CS_ACTIVE) {
        client->sendAsActive = qtrue;
    } else if (client->state != CS_ZOMBIE) {
        client->sendAsActive = qfalse;
    }
    if (!client->sendAsActive) {
        snapshotFlags |= SV_SNAPSHOT_NOT_ACTIVE_FLAG;
    }
    MSG_WriteByte(message, snapshotFlags);

    int32_t oldNumEntities = 0;
    int32_t oldFirstEntity = 0;
    int32_t oldNumClients = 0;
    int32_t oldFirstClient = 0;
    if (oldFrame != NULL) {
        MSG_WriteDeltaPlayerstate(message, &oldFrame->playerState, &newFrame->playerState);
        oldNumEntities = oldFrame->numEntities;
        oldFirstEntity = oldFrame->firstEntity;
        oldNumClients = oldFrame->numClients;
        oldFirstClient = oldFrame->firstClient;
    } else {
        MSG_WriteDeltaPlayerstate(message, NULL, &newFrame->playerState);
    }

    SV_EmitPacketEntities(oldNumEntities, oldFirstEntity, newFrame->numEntities, newFrame->firstEntity, message);
    SV_EmitPacketClients(oldNumClients, oldFirstClient, newFrame->numClients, newFrame->firstClient, message);

    for (int32_t padIndex = 0; padIndex < sv_padPackets->integer; ++padIndex) {
        MSG_WriteByte(message, SV_SNAPSHOT_PAD_BYTE);
    }
}

/* Source: CoDUOMP.exe 0x00465d40..0x00465d9d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00465d40_00465d9d.mcode.
 * Name: exact same-module Mac symbol SV_RateMsec. The 48-byte addition is
 * packet overhead charged by the original rate scheduler, not message data.
 * Linux spells the same expression as messageSize*1000 + 48000. */
int32_t SV_RateMsec(client_t *client, int32_t messageSize)
{
    if (messageSize > SV_RATE_MAX_ACCOUNTED_MESSAGE_BYTES)
        messageSize = SV_RATE_MAX_ACCOUNTED_MESSAGE_BYTES;

    int32_t rate = client->rate;
    if (sv_maxRate->integer != 0) {
        if (sv_maxRate->integer < SV_RATE_MINIMUM_BYTES_PER_SECOND) {
            (void)Cvar_Set2("sv_MaxRate", "1000", qtrue);
        }
        if (sv_maxRate->integer < rate)
            rate = sv_maxRate->integer;
    }

    return ((messageSize + SV_RATE_PACKET_OVERHEAD_BYTES) * SV_RATE_MILLISECONDS_PER_SECOND) / rate;
}

/* Source: CoDUOMP.exe 0x00465da0..0x00465f3c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00465da0_00465f3c.mcode.
 * Name: exact same-module Mac symbol SV_SendMessageToClient. The leading
 * acknowledgement dword remains uncompressed; the encoded/compressed packet
 * size is the value charged to rate scheduling and bandwidth accounting.
 * Windows's 0x8004 stack probe includes its four-byte security cookie: both
 * originals expose exactly MAX_MSGLEN bytes of packet storage here. */
void SV_SendMessageToClient(msg_t *message, client_t *client)
{
    uint8_t packetData[MAX_MSGLEN];

    memcpy(packetData, message->data, SV_MESSAGE_ACKNOWLEDGEMENT_BYTES);
    const int32_t packetSize =
        MSG_WriteBitsCompress(message->data + SV_MESSAGE_ACKNOWLEDGEMENT_BYTES, packetData + SV_MESSAGE_ACKNOWLEDGEMENT_BYTES,
                              message->cursize - SV_MESSAGE_ACKNOWLEDGEMENT_BYTES) +
        SV_MESSAGE_ACKNOWLEDGEMENT_BYTES;

    if (client->deferredDropReason != NULL) {
        SV_DropClient(client, client->deferredDropReason);
    }

    clientSnapshot_t *const snapshotFrame =
        &client->snapshotFrames[client->netchan.outgoingSequence & (SERVER_CLIENT_SNAPSHOT_FRAME_COUNT - 1)];
    snapshotFrame->messageSize = packetSize;
    snapshotFrame->messageSentTime = svs.realTime;
    snapshotFrame->messageAcknowledgedTime = -1;

    SV_Netchan_Transmit(client, packetData, packetSize);

    if (client->netchan.remoteAddress.type == NA_LOOPBACK || Sys_IsLANAddress(client->netchan.remoteAddress) != qfalse) {
        client->nextSnapshotTime = svs.realTime - 1;
        return;
    }

    int32_t rateMsec = SV_RateMsec(client, packetSize);
    if (rateMsec < client->snapshotMsec) {
        rateMsec = client->snapshotMsec;
        client->rateDelayed = qfalse;
    } else {
        client->rateDelayed = qtrue;
    }

    client->nextSnapshotTime = svs.realTime + rateMsec;
    if (client->state != CS_ACTIVE && client->download.fileName[0] == '\0' &&
        client->nextSnapshotTime < svs.realTime + SV_CONNECTING_CLIENT_MIN_SNAPSHOT_MSEC) {
        client->nextSnapshotTime = svs.realTime + SV_CONNECTING_CLIENT_MIN_SNAPSHOT_MSEC;
    }

    sv_totalBytesSentThisFrame += packetSize;
}

/* Source: CoDUOMP.exe 0x00465f40..0x0046615a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00465f40_0046615a.mcode.
 * Name: exact same-module Mac symbol SV_SendClientSnapshot. Active and zombie
 * clients build snapshot state and append reliable commands; zombie clients
 * deliberately skip download traffic. If the first message overflows, the
 * original retries with only the reliable-command overflow limiter before
 * disconnecting a client whose reduced message still does not fit. */
void SV_SendClientSnapshot(client_t *client)
{
    if (client->state == CS_ACTIVE || client->state == CS_ZOMBIE) {
        SV_BuildClientSnapshot(client);
    }

    uint8_t messageData[MAX_MSGLEN];
    msg_t message;
    MSG_Init(&message, messageData, sizeof(messageData));
    MSG_WriteLong(&message, client->lastClientCommand);

    if (client->state == CS_ACTIVE || client->state == CS_ZOMBIE) {
        SV_UpdateServerCommandsToClient(client, &message);
        SV_WriteSnapshotToClient(client, &message);
    }

    if (client->state != CS_ZOMBIE)
        SV_WriteDownloadToClient(client, &message);

    MSG_WriteByte(&message, SERVER_SVC_EOF);
    if (message.overflowed != qfalse) {
        Com_Printf("WARNING: msg overflowed for %s, trying to recover\n", client->name);

        if (client->state == CS_ACTIVE || client->state == CS_ZOMBIE) {
            SV_PrintServerCommandsForClient(client);

            MSG_Init(&message, messageData, sizeof(messageData));
            MSG_WriteLong(&message, client->lastClientCommand);
            SV_UpdateServerCommandsToClient_PreventOverflow(client, &message, sizeof(messageData));
            MSG_WriteByte(&message, SERVER_SVC_EOF);
        }

        if (message.overflowed != qfalse) {
            Com_Printf("WARNING: client disconnected for msg overflow: %s\n", client->name);
            NET_OutOfBandPrint(NS_SERVER, client->netchan.remoteAddress, "disconnect");
            SV_DropClient(client, "EXE_SERVERMESSAGEOVERFLOW");
        }
    }

    SV_SendMessageToClient(&message, client);
}

/* Source: CoDUOMP.exe 0x00466bd0..0x00466f41.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00466bd0_00466f41.mcode.
 * Name: exact same-module Mac symbol SV_SendClientMessages. Due clients either
 * send the next queued netchan fragment or build a fresh snapshot. The
 * optional bandwidth report uses the exact 20-frame rolling histories and
 * float constants encoded by the Windows executable. */
void SV_SendClientMessages(void)
{
    int32_t sentClientCount = 0;
    sv_totalBytesSentThisFrame = 0;
    sv_totalUncompressedBytesThisFrame = 0;

    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state == CS_FREE || client->nextSnapshotTime > svs.realTime) {
            continue;
        }

        ++sentClientCount;
        if (client->netchan.unsentFragments == qfalse) {
            SV_SendClientSnapshot(client);
        } else {
            const int32_t remainingFragmentBytes = client->netchan.unsentLength - client->netchan.unsentFragmentStart;
            client->nextSnapshotTime = svs.realTime + SV_RateMsec(client, remainingFragmentBytes);
            SV_Netchan_TransmitNextFragment(&client->netchan);
        }
    }

    if (sv_showAverageBPS->integer == 0 || sentClientCount <= 0) {
        return;
    }

#if EMULATE_X87
    x87f compressedBytes = x87f_load_f32(0.0f);
#else
    long double compressedBytes = 0.0L;
#endif
    float uncompressedBytes = 0.0f;
    for (int32_t sampleIndex = 0; sampleIndex < SERVER_AVERAGE_BPS_WINDOW_COUNT - 1; ++sampleIndex) {
        sv_compressedBpsWindow[sampleIndex] = sv_compressedBpsWindow[sampleIndex + 1];
#if EMULATE_X87
        compressedBytes = x87f_add(compressedBytes, x87f_load_i32(sv_compressedBpsWindow[sampleIndex]));
#else
        compressedBytes += (long double)sv_compressedBpsWindow[sampleIndex];
#endif

        sv_uncompressedBpsWindow[sampleIndex] = sv_uncompressedBpsWindow[sampleIndex + 1];
#if EMULATE_X87
        uncompressedBytes =
            x87f_store_f32(x87f_add(x87f_load_f32(uncompressedBytes), x87f_load_i32(sv_uncompressedBpsWindow[sampleIndex])));
#else
        uncompressedBytes = (float)((long double)uncompressedBytes + (long double)sv_uncompressedBpsWindow[sampleIndex]);
#endif
    }

    sv_compressedBpsWindow[SERVER_AVERAGE_BPS_WINDOW_COUNT - 1] = sv_totalBytesSentThisFrame;
#if EMULATE_X87
    compressedBytes = x87f_add(compressedBytes, x87f_load_i32(sv_totalBytesSentThisFrame));
#else
    compressedBytes += (long double)sv_totalBytesSentThisFrame;
#endif

    sv_uncompressedBpsWindow[SERVER_AVERAGE_BPS_WINDOW_COUNT - 1] = sv_totalUncompressedBytesThisFrame;
#if EMULATE_X87
    uncompressedBytes = x87f_store_f32(x87f_add(x87f_load_f32(uncompressedBytes), x87f_load_i32(sv_totalUncompressedBytesThisFrame)));
#else
    uncompressedBytes = (float)((long double)uncompressedBytes + (long double)sv_totalUncompressedBytesThisFrame);
#endif

    if (sv_totalBytesSentThisFrame >= sv_compressedBpsMax) {
        sv_compressedBpsMax = sv_totalBytesSentThisFrame;
    }
    if (sv_totalUncompressedBytesThisFrame >= sv_uncompressedBpsMax) {
        sv_uncompressedBpsMax = sv_totalUncompressedBytesThisFrame;
    }

    ++sv_averageBpsFrameCount;
    if (sv_averageBpsFrameCount < SERVER_AVERAGE_BPS_WINDOW_COUNT) {
        return;
    }
    sv_averageBpsFrameCount = 0;

    /* 0x005b9d60 = 0x3d4ccccd, the original float nearest 1/20. */
    /* 0x00466eaf..0x00466f27 keeps both scaled byte totals and the current
     * compression ratio on the x87 stack through all seven variadic
     * arguments. Only the running ratio sum is rounded to float. */
#if EMULATE_X87
    const x87f averageScale = x87f_load_f32(0.05000000074505806f);
    const x87f uncompressedBytesRaw = x87f_mul(x87f_load_f32(uncompressedBytes), averageScale);
    const x87f compressedBytesRaw = x87f_mul(compressedBytes, averageScale);
    const x87f compressionRatioRaw =
        x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_div(compressedBytesRaw, uncompressedBytesRaw)), x87f_load_f32(100.0f));
    sv_averageCompressionRatioSum = x87f_store_f32(x87f_add(x87f_load_f32(sv_averageCompressionRatioSum), compressionRatioRaw));
#else
    const long double uncompressedBytesRaw = (long double)uncompressedBytes * (long double)0.05000000074505806f;
    const long double compressedBytesRaw = compressedBytes * (long double)0.05000000074505806f;
    const long double compressionRatioRaw = ((long double)1.0f - compressedBytesRaw / uncompressedBytesRaw) * (long double)100.0f;
    sv_averageCompressionRatioSum = (float)((long double)sv_averageCompressionRatioSum + compressionRatioRaw);
#endif
    ++sv_averageCompressionRatioCount;

    Com_DPrintf("bpspc(%2.0f) bps(%2.0f) pk(%i) "
                "ubps(%2.0f) upk(%i) cr(%2.2f) "
                "acr(%2.2f)\n",
#if EMULATE_X87
                x87f_store_f64(x87f_div(compressedBytesRaw, x87f_load_i32(sentClientCount))), x87f_store_f64(compressedBytesRaw),
#else
                (double)(compressedBytesRaw / (long double)sentClientCount), (double)compressedBytesRaw,
#endif
                sv_compressedBpsMax,
#if EMULATE_X87
                x87f_store_f64(uncompressedBytesRaw),
#else
                (double)uncompressedBytesRaw,
#endif
                sv_uncompressedBpsMax,
#if EMULATE_X87
                x87f_store_f64(compressionRatioRaw),
                x87f_store_f64(x87f_div(x87f_load_f32(sv_averageCompressionRatioSum), x87f_load_i32(sv_averageCompressionRatioCount))));
#else
                (double)compressionRatioRaw,
                (double)((long double)sv_averageCompressionRatioSum / (long double)sv_averageCompressionRatioCount));
#endif
}
