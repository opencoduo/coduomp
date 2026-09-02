#include "netchan.h"

#include "net_text.h"
#include "q_command.h"
#include "q_cvar.h"
#include "q_shared_types.h"
#include "q_string.h"
#include "qcommon_limits.h"

#include <stdint.h>
#include <string.h>

enum {
    NETCHAN_SEQUENCE_HEADER_BYTES = sizeof(int32_t),
    NETCHAN_FIRST_OUTGOING_SEQUENCE = 1
};

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);

/* This original string table is consumed by both the transport and profile
 * subsystems.  CoDUOMP.exe stores it at 0x005c5cd0; coduo_lnxded stores it at
 * 0x080f0940. */
const char *const net_profileSocketNames[NET_PROFILE_SOCKET_NAME_COUNT] = {"client", "server"};

/*
 * Complete reliable-channel framing and fragment-reassembly subsystem.
 * Windows and Linux agree on the record transitions, packet bytes, fragment
 * limits, diagnostic gates, and profiling calls:
 *
 *   Netchan_Init
 *     CoDUOMP.exe   0x0044d470..0x0044d529
 *     coduo_lnxded  0x08083e5f..0x08083f56
 *   Netchan_Setup
 *     CoDUOMP.exe   0x0044d530..0x0044d58a
 *     coduo_lnxded  0x08083f57..0x08083fcf
 *   Netchan_TransmitNextFragment
 *     CoDUOMP.exe   0x0044d590..0x0044d787
 *     coduo_lnxded  0x08083fd0..0x080841cd
 *   Netchan_Transmit
 *     CoDUOMP.exe   0x0044d790..0x0044d96f
 *     coduo_lnxded  0x080841ce..0x08084377
 *   Netchan_Process
 *     CoDUOMP.exe   0x0044d970..0x0044dd44
 *     coduo_lnxded  0x08084378..0x08084801
 *
 * Windows's optimized fragment constructors inline MSG_Init's state setup;
 * Linux calls MSG_Init.  Linux also calls the identity body at 0x08085130
 * before rewriting the completed sequence dword, whereas Windows stores the
 * same dword directly.  Neither compiler realization changes the packet.
 */

void Netchan_Init(int32_t qport)
{
    showpackets = Cvar_Get("showpackets", "0", CVAR_TEMP);
    showdrop = Cvar_Get("showdrop", "0", CVAR_TEMP);
    net_qport = Cvar_Get("net_qport", va("%i", (int32_t)((uint32_t)qport & UINT16_MAX)), CVAR_INIT);
    net_profile = Cvar_Get("net_profile", "0", CVAR_TEMP);
    net_showprofile = Cvar_Get("net_showprofile", "0", CVAR_TEMP);
    net_lanauthorize = Cvar_Get("net_lanauthorize", "0", CVAR_NONE);
    Cmd_AddCommand("net_dumpprofile", Net_DumpProfile_f);
}

void Netchan_Setup(netsrc_t source, netchan_t *channel, netadr_t address, int32_t qport)
{
    memset(channel, 0, sizeof(*channel));
    channel->sock = source;
    channel->remoteAddress = address;
    channel->qport = qport;
    channel->incomingSequence = 0;
    channel->outgoingSequence = NETCHAN_FIRST_OUTGOING_SEQUENCE;
    NetProf_PrepProfiling(&channel->profile);
}

void Netchan_TransmitNextFragment(netchan_t *channel)
{
    msg_t message;
    uint8_t packetData[NETCHAN_PACKET_BUFFER_SIZE];
    int32_t fragmentLength;

    NetProf_PrepProfiling(&channel->profile);
    MSG_Init(&message, packetData, (int32_t)sizeof(packetData));
    MSG_WriteLong(&message, (int32_t)((uint32_t)channel->outgoingSequence | UINT32_C(0x80000000)));
    if (channel->sock == NS_CLIENT) {
        MSG_WriteShort(&message, net_qport->integer);
    }

    fragmentLength = NETCHAN_FRAGMENT_SIZE;
    if (channel->unsentFragmentStart + NETCHAN_FRAGMENT_SIZE > channel->unsentLength) {
        fragmentLength = channel->unsentLength - channel->unsentFragmentStart;
    }

    MSG_WriteShort(&message, channel->unsentFragmentStart);
    MSG_WriteShort(&message, fragmentLength);
    MSG_WriteData(&message, channel->unsentBuffer + channel->unsentFragmentStart, fragmentLength);

    NET_SendPacket(channel->sock, message.cursize, message.data, channel->remoteAddress);
    NetProf_NewSendPacket(channel, message.cursize, qtrue);

    if (showpackets->integer != 0) {
        Com_Printf("%s send %4i : s=%i fragment=%i,%i\n", net_profileSocketNames[channel->sock], message.cursize,
                   channel->outgoingSequence - 1, channel->unsentFragmentStart, fragmentLength);
    }

    channel->unsentFragmentStart += fragmentLength;
    if (channel->unsentFragmentStart == channel->unsentLength && fragmentLength != NETCHAN_FRAGMENT_SIZE) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ++channel->outgoingSequence;
        channel->unsentFragments = qfalse;
    }
}

void Netchan_Transmit(netchan_t *channel, int32_t length, const void *data)
{
    msg_t message;
    uint8_t packetData[NETCHAN_PACKET_BUFFER_SIZE];

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0 || length > MAX_MSGLEN) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Netchan_Transmit: length = %i",
                  length);
        return;
    }

    channel->unsentFragmentStart = 0;
    if (length >= NETCHAN_FRAGMENT_SIZE) {
        channel->unsentFragments = qtrue;
        channel->unsentLength = length;
        memcpy(channel->unsentBuffer, data, (size_t)length);
        Netchan_TransmitNextFragment(channel);
        return;
    }

    NetProf_PrepProfiling(&channel->profile);
    MSG_Init(&message, packetData, (int32_t)sizeof(packetData));
    MSG_WriteLong(&message, channel->outgoingSequence);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ++channel->outgoingSequence;
    if (channel->sock == NS_CLIENT) {
        MSG_WriteShort(&message, net_qport->integer);
    }
    MSG_WriteData(&message, data, length);

    NET_SendPacket(channel->sock, message.cursize, message.data, channel->remoteAddress);
    NetProf_NewSendPacket(channel, message.cursize, qfalse);

    if (showpackets->integer != 0) {
        Com_Printf("%s send %4i : s=%i ack=%i\n", net_profileSocketNames[channel->sock], message.cursize, channel->outgoingSequence - 1,
                   channel->incomingSequence);
    }
}

qboolean Netchan_Process(netchan_t *channel, msg_t *message)
{
    int32_t sequence;
    qboolean fragmented;
    int32_t fragmentStart;
    int32_t fragmentLength;

    NetProf_PrepProfiling(&channel->profile);
    MSG_BeginReading(message);
    sequence = MSG_ReadLong(message);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    fragmented = sequence < 0 ? qtrue : qfalse;
    if (fragmented != qfalse) {
        sequence = (int32_t)((uint32_t)sequence & INT32_MAX);
    }

    if (channel->sock == NS_SERVER) {
        (void)MSG_ReadShort(message);
    }

    if (fragmented != qfalse) {
        fragmentStart = MSG_ReadShort(message);
        fragmentLength = MSG_ReadShort(message);
    } else {
        fragmentStart = 0;
        fragmentLength = 0;
    }

    NetProf_NewRecievePacket(channel, message->cursize, fragmented);

    if (showpackets->integer != 0) {
        if (fragmented != qfalse) {
            Com_Printf("%s recv %4i : s=%i fragment=%i,%i\n", net_profileSocketNames[channel->sock], message->cursize, sequence,
                       fragmentStart, fragmentLength);
        } else {
            Com_Printf("%s recv %4i : s=%i\n", net_profileSocketNames[channel->sock], message->cursize, sequence);
        }
    }

    if (sequence <= channel->incomingSequence) {
        if (showdrop->integer != 0 || showpackets->integer != 0) {
            Com_Printf("%s:Out of order packet %i at %i\n", NET_AdrToString(channel->remoteAddress), sequence, channel->incomingSequence);
        }
        return qfalse;
    }

    channel->dropped = sequence - channel->incomingSequence - 1;
    if (channel->dropped > 0 && (showdrop->integer != 0 || showpackets->integer != 0)) {
        Com_Printf("%s:Dropped %i packets at %i\n", NET_AdrToString(channel->remoteAddress), channel->dropped, sequence);
    }

    if (fragmented == qfalse) {
        channel->incomingSequence = sequence;
        return qtrue;
    }

    if (sequence != channel->fragmentSequence) {
        channel->fragmentSequence = sequence;
        channel->fragmentLength = 0;
    }

    if (fragmentStart != channel->fragmentLength) {
        if (showdrop->integer != 0 || showpackets->integer != 0) {
            /* The original call supplies sequence as an unused extra vararg. */
            Com_Printf("%s:Dropped a message fragment\n", NET_AdrToString(channel->remoteAddress), sequence);
        }
        return qfalse;
    }

    /* Both i386 bodies compare the dword sum against MAX_MSGLEN with an
     * unsigned condition after separately rejecting a negative extent. */
    if (fragmentLength < 0 || message->readcount + fragmentLength > message->cursize ||
        (uint32_t)channel->fragmentLength + (uint32_t)fragmentLength > (uint32_t)MAX_MSGLEN) {
        if (showdrop->integer != 0 || showpackets->integer != 0) {
            Com_Printf("%s:illegal fragment length\n", NET_AdrToString(channel->remoteAddress));
        }
        return qfalse;
    }

    memcpy(channel->fragmentBuffer + channel->fragmentLength, message->data + message->readcount, (size_t)fragmentLength);
    channel->fragmentLength += fragmentLength;

    if (fragmentLength == NETCHAN_FRAGMENT_SIZE) {
        return qfalse;
    }

    if (channel->fragmentLength > message->maxsize) {
        Com_Printf("%s:fragmentLength %i > msg->maxsize\n", NET_AdrToString(channel->remoteAddress), channel->fragmentLength);
        return qfalse;
    }

    /* The two authoritative i386 hosts store this dword directly because
     * they are little-endian.  Spell out the same wire order so native
     * big-endian supporting builds do not inherit the host byte order. */
    message->data[0] = (uint8_t)(uint32_t)sequence;
    message->data[1] = (uint8_t)((uint32_t)sequence >> 8U);
    message->data[2] = (uint8_t)((uint32_t)sequence >> 16U);
    message->data[3] = (uint8_t)((uint32_t)sequence >> 24U);
    memcpy(message->data + NETCHAN_SEQUENCE_HEADER_BYTES, channel->fragmentBuffer, (size_t)channel->fragmentLength);
    message->cursize = channel->fragmentLength + NETCHAN_SEQUENCE_HEADER_BYTES;
    channel->fragmentLength = 0;

    MSG_BeginReading(message);
    (void)MSG_ReadLong(message);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    channel->incomingSequence = sequence;
    return qtrue;
}
