#include "server_netchan.h"
#include "server_netchan_profile_services.h"

#include "qcommon/com_sprintf.h"
#include "qcommon/net_text.h"
#include "qcommon/netchan.h"

#include <stdint.h>
#include <string.h>

enum {
    SV_NETCHAN_HEADER_BYTES = 4,
    SV_NETCHAN_CONNECTIONLESS_MARKER = -1,
    SV_PROFILE_INITIAL_Y = 80,
    SV_PROFILE_LINE_HEIGHT = 10,
    SV_PROFILE_LINE_BYTES = 1024,
    SV_PROFILE_CLIENT_NAME_BYTES = 17,
    SV_PROFILE_CLIENT_NAME_LAST = 16,
    SV_PROFILE_EMPTY_MIN_BYTES = 9999,
    SV_PROFILE_PERCENT_SCALE = 100
};

extern serverStatic_t svs;
extern cvar_t *sv_maxclients;

void Com_Printf(const char *format, ...);

/*
 * Complete common server-netchan transport and profile-maintenance core.
 * The Windows client engine and Linux dedicated engine retain the same
 * operation graph; compiler inlining and calling conventions account for the
 * instruction-level differences.
 *
 * Function                              Windows       Linux
 * SV_Netchan_Encode                     0x00463640    0x08095700
 * SV_Netchan_Decode                     0x004636a0    0x0809578a
 * SV_Netchan_TransmitNextFragment       0x00463710    0x08095838
 * SV_Netchan_Transmit                   0x00463720    0x0809584b
 * SV_Netchan_AddOOBProfilePacket        0x00463750    0x08095890
 * SV_Netchan_SendOOBPacket              0x00463780    0x080958cb
 * SV_Netchan_UpdateProfileStats         0x00463800    0x0809593b
 * SV_Netchan_PrintProfileStats          0x004638a0    0x080959e7
 *
 * The supporting Mac client exports every canonical name above except the
 * diagnostic SendOOB wrapper; its diagnostic string and both i386 bodies
 * prove that wrapper's role. Only the report's client drawing sink remains
 * target-local; the dedicated target has no cgame VM and compiles that
 * unavailable presentation edge to a no-op.
 */

void SV_Netchan_Encode(client_t *client, uint8_t *data,
                       int32_t length)
{
    uint8_t key = (uint8_t)client->netchan.outgoingSequence ^
                  (uint8_t)client->challenge;
    int32_t commandIndex = 0;

    for (int32_t byteIndex = 0; byteIndex < length; ++byteIndex) {
        if (client->lastClientCommandString[commandIndex] == '\0')
            commandIndex = 0;

        key ^= (uint8_t)(
            (uint8_t)client->lastClientCommandString[commandIndex]
            << (byteIndex & 1));
        ++commandIndex;
        data[byteIndex] ^= key;
    }
}

void SV_Netchan_Decode(client_t *client, uint8_t *data,
                       int32_t length)
{
    const char *const command =
        client->reliableCommands[
            client->reliableAcknowledge &
            (MAX_RELIABLE_COMMANDS - 1)].commandText;
    uint8_t key = (uint8_t)client->challenge ^
                  (uint8_t)client->serverId ^
                  (uint8_t)client->messageAcknowledge;
    int32_t commandIndex = 0;

    for (int32_t byteIndex = 0; byteIndex < length; ++byteIndex) {
        if (command[commandIndex] == '\0')
            commandIndex = 0;

        key ^= (uint8_t)((uint8_t)command[commandIndex]
                         << (byteIndex & 1));
        ++commandIndex;
        data[byteIndex] ^= key;
    }
}

void SV_Netchan_TransmitNextFragment(netchan_t *channel)
{
    Netchan_TransmitNextFragment(channel);
}

void SV_Netchan_Transmit(client_t *client, uint8_t *data,
                         int32_t length)
{
    SV_Netchan_Encode(client, data + SV_NETCHAN_HEADER_BYTES,
                      length - SV_NETCHAN_HEADER_BYTES);
    Netchan_Transmit(&client->netchan, length, data);
}

void SV_Netchan_AddOOBProfilePacket(int32_t length)
{
    if (net_profile->integer != 0) {
        NetProf_PrepProfiling(&svs.netProfile);
        NetProf_AddPacket(&svs.netProfile->send, length, qfalse);
    }
}

void SV_Netchan_SendOOBPacket(int32_t length, const void *data,
                              netadr_t address)
{
    int32_t marker;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("SV_Netchan_SendOOBPacket: invalid packet length %i\n", length);
        return;
    }
    if (length < (int32_t)sizeof(marker)) {
        Com_Printf(
            "SV_Netchan_SendOOBPacket used to send non-OOB packet.\n");
    } else {
        memcpy(&marker, data, sizeof(marker));
        if (marker != SV_NETCHAN_CONNECTIONLESS_MARKER) {
            Com_Printf(
                "SV_Netchan_SendOOBPacket used to send non-OOB packet.\n");
        }
    }

    NetProf_PrepProfiling(&svs.netProfile);
    NET_SendPacket(NS_SERVER, length, data, address);
    /* Linux retains this call; the Windows optimizer inlines its body. */
    SV_Netchan_AddOOBProfilePacket(length);
}

void SV_Netchan_UpdateProfileStats(void)
{
    if (svs.clients == NULL)
        return;

    if (svs.netProfile != NULL) {
        NetProf_UpdateStatistics(&svs.netProfile->send);
        NetProf_UpdateStatistics(&svs.netProfile->receive);
    }

    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state != CS_FREE && client->netchan.profile != NULL) {
            NetProf_UpdateStatistics(&client->netchan.profile->send);
            NetProf_UpdateStatistics(&client->netchan.profile->receive);
        }
    }
}

/*
 * The profile report completes the common server-netchan subsystem:
 *
 *   CoDUOMP.exe       0x004638a0..0x00463ffa
 *   coduo_lnxded      0x080959e7..0x080963b2
 *
 * Both retail engines format and aggregate the same samples, clients,
 * extrema, fragment percentages, and report text.  The dedicated build only
 * emits the printHeader path.  CoDUOMP additionally draws each false-header
 * line through its cgame VM; SV_NETCHAN_PROFILE_DRAW retains that client-only
 * presentation edge without forking the report computation.
 */
void SV_Netchan_PrintProfileStats(qboolean printHeader)
{
    char line[SV_PROFILE_LINE_BYTES];
    char clientName[SV_PROFILE_CLIENT_NAME_BYTES];
    int32_t drawY = SV_PROFILE_INITIAL_Y;
    int32_t totalSendBps = 0;
    int32_t totalReceiveBps = 0;
    int32_t totalSendSamples = 0;
    int32_t totalSendFragments = 0;
    int32_t totalReceiveSamples = 0;
    int32_t totalReceiveFragments = 0;
    int32_t totalSendMax = 0;
    int32_t totalSendMin = SV_PROFILE_EMPTY_MIN_BYTES;
    int32_t totalReceiveMax = 0;
    int32_t totalReceiveMin = SV_PROFILE_EMPTY_MIN_BYTES;

    if (svs.clients == NULL) {
        return;
    }

    SV_Netchan_UpdateProfileStats();

    if (printHeader != qfalse) {
        Com_Printf("\n\n");
    }

    Com_sprintf(line, sizeof(line), "====================");
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        drawY += SV_PROFILE_LINE_HEIGHT;
        SV_NETCHAN_PROFILE_DRAW(line, drawY);
    }

    Com_sprintf(line, sizeof(line), "Server Network Profile:");
    if (printHeader != qfalse) {
        Com_Printf("%s\n\n", line);
    } else {
        drawY += SV_PROFILE_LINE_HEIGHT;
        SV_NETCHAN_PROFILE_DRAW(line, drawY);
        drawY += SV_PROFILE_LINE_HEIGHT;
    }

    Com_sprintf(
        line, sizeof(line),
        "                    | Sent To                | Recieved From          | Total Source Traffic   |");
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        drawY += SV_PROFILE_LINE_HEIGHT;
        SV_NETCHAN_PROFILE_DRAW(line, drawY);
    }

    Com_sprintf(
        line, sizeof(line),
        "              Source|   bps|  max|  min|frag%%|   bps|  max|  min|frag%%|   bps|  max|  min|frag%%|");
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        drawY += SV_PROFILE_LINE_HEIGHT;
        SV_NETCHAN_PROFILE_DRAW(line, drawY);
    }

    if (svs.netProfile != NULL) {
        const netProfileInfo_t *const profile = svs.netProfile;
        totalSendBps += profile->send.bytesPerSecond;
        totalSendSamples += profile->send.sampleCount;
        totalSendFragments += profile->send.fragmentSampleCount;
        totalReceiveBps += profile->receive.bytesPerSecond;
        totalReceiveSamples += profile->receive.sampleCount;
        totalReceiveFragments += profile->receive.fragmentSampleCount;
        if (profile->send.maxBytes > totalSendMax) {
            totalSendMax = profile->send.maxBytes;
        }
        if (profile->send.minBytes < totalSendMin) {
            totalSendMin = profile->send.minBytes;
        }
        if (profile->receive.maxBytes > totalReceiveMax) {
            totalReceiveMax = profile->receive.maxBytes;
        }
        if (profile->receive.minBytes < totalReceiveMin) {
            totalReceiveMin = profile->receive.minBytes;
        }
    }

    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        const client_t *const client = &svs.clients[clientNum];
        const netProfileInfo_t *const profile = client->netchan.profile;
        if (client->state == CS_FREE || profile == NULL) {
            continue;
        }

        totalSendBps += profile->send.bytesPerSecond;
        totalSendSamples += profile->send.sampleCount;
        totalSendFragments += profile->send.fragmentSampleCount;
        totalReceiveBps += profile->receive.bytesPerSecond;
        totalReceiveSamples += profile->receive.sampleCount;
        totalReceiveFragments += profile->receive.fragmentSampleCount;
        if (profile->send.maxBytes > totalSendMax) {
            totalSendMax = profile->send.maxBytes;
        }
        if (profile->send.minBytes < totalSendMin) {
            totalSendMin = profile->send.minBytes;
        }
        if (profile->receive.maxBytes > totalReceiveMax) {
            totalReceiveMax = profile->receive.maxBytes;
        }
        if (profile->receive.minBytes < totalReceiveMin) {
            totalReceiveMin = profile->receive.minBytes;
        }
    }

    const int32_t totalSamples =
        totalSendSamples + totalReceiveSamples;
    const int32_t totalFragments =
        totalSendFragments + totalReceiveFragments;
    const int32_t totalFragmentPercent =
        totalSamples > 0 && totalFragments > 0
            ? totalFragments * SV_PROFILE_PERCENT_SCALE / totalSamples
            : 0;
    const int32_t combinedMin =
        totalSendMin < totalReceiveMin ? totalSendMin : totalReceiveMin;
    const int32_t combinedMax =
        totalSendMax > totalReceiveMax ? totalSendMax : totalReceiveMax;
    const int32_t sendFragmentPercent =
        totalSendSamples != 0
            ? totalSendFragments * SV_PROFILE_PERCENT_SCALE /
                  totalSendSamples
            : 0;
    const int32_t receiveFragmentPercent =
        totalReceiveSamples != 0
            ? totalReceiveFragments * SV_PROFILE_PERCENT_SCALE /
                  totalReceiveSamples
            : 0;

    Com_sprintf(
        line, sizeof(line),
        "              Totals:%6i|%5i|%5i| %3i%%|%6i|%5i|%5i| %3i%%|%6i|%5i|%5i| %3i%%|",
        totalSendBps, totalSendMax, totalSendMin, sendFragmentPercent,
        totalReceiveBps, totalReceiveMax, totalReceiveMin,
        receiveFragmentPercent, totalSendBps + totalReceiveBps,
        combinedMax, combinedMin, totalFragmentPercent);
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        drawY += SV_PROFILE_LINE_HEIGHT;
        SV_NETCHAN_PROFILE_DRAW(line, drawY);
    }

    if (svs.netProfile == NULL) {
        Com_sprintf(
            line, sizeof(line),
            "  OutOfBand Messages:     0|    0|    0|   - |     0|    0|    0|   - |     0|    0|    0|   - |");
    } else {
        const netProfileInfo_t *const profile = svs.netProfile;
        const int32_t sampleCount =
            profile->send.sampleCount + profile->receive.sampleCount;
        const int32_t fragmentCount =
            profile->send.fragmentSampleCount +
            profile->receive.fragmentSampleCount;
        const int32_t fragmentPercent =
            sampleCount > 0 && fragmentCount > 0
                ? fragmentCount * SV_PROFILE_PERCENT_SCALE / sampleCount
                : 0;
        const int32_t minBytes =
            profile->send.minBytes < profile->receive.minBytes
                ? profile->send.minBytes
                : profile->receive.minBytes;
        const int32_t maxBytes =
            profile->send.maxBytes > profile->receive.maxBytes
                ? profile->send.maxBytes
                : profile->receive.maxBytes;

        Com_sprintf(
            line, sizeof(line),
            "  OutOfBand Messages: %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
            profile->send.bytesPerSecond, profile->send.maxBytes,
            profile->send.minBytes, profile->send.fragmentPercent,
            profile->receive.bytesPerSecond, profile->receive.maxBytes,
            profile->receive.minBytes, profile->receive.fragmentPercent,
            profile->send.bytesPerSecond +
                profile->receive.bytesPerSecond,
            maxBytes, minBytes, fragmentPercent);
    }
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        drawY += SV_PROFILE_LINE_HEIGHT;
        SV_NETCHAN_PROFILE_DRAW(line, drawY);
    }

    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        const client_t *const client = &svs.clients[clientNum];
        if (client->state == CS_FREE) {
            continue;
        }

        strncpy(clientName, client->name, sizeof(clientName));
        clientName[SV_PROFILE_CLIENT_NAME_LAST] = '\0';

        const netProfileInfo_t *const profile = client->netchan.profile;
        if (profile == NULL) {
            Com_sprintf(
                line, sizeof(line),
                "#%2i-%16s:     0|    0|    0|   0%%|     0|    0|    0|   0%%|     0|    0|    0|   0%%|",
                clientNum, clientName);
        } else {
            const int32_t sampleCount =
                profile->send.sampleCount + profile->receive.sampleCount;
            const int32_t fragmentCount =
                profile->send.fragmentSampleCount +
                profile->receive.fragmentSampleCount;
            const int32_t fragmentPercent =
                sampleCount > 0 && fragmentCount > 0
                    ? fragmentCount * SV_PROFILE_PERCENT_SCALE / sampleCount
                    : 0;
            const int32_t minBytes =
                profile->send.minBytes < profile->receive.minBytes
                    ? profile->send.minBytes
                    : profile->receive.minBytes;
            const int32_t maxBytes =
                profile->send.maxBytes > profile->receive.maxBytes
                    ? profile->send.maxBytes
                    : profile->receive.maxBytes;

            Com_sprintf(
                line, sizeof(line),
                "#%2i-%16s: %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                clientNum, clientName, profile->send.bytesPerSecond,
                profile->send.maxBytes, profile->send.minBytes,
                profile->send.fragmentPercent,
                profile->receive.bytesPerSecond,
                profile->receive.maxBytes, profile->receive.minBytes,
                profile->receive.fragmentPercent,
                profile->send.bytesPerSecond +
                    profile->receive.bytesPerSecond,
                maxBytes, minBytes, fragmentPercent);
        }

        if (printHeader != qfalse) {
            Com_Printf("%s\n", line);
        } else {
            drawY += SV_PROFILE_LINE_HEIGHT;
            SV_NETCHAN_PROFILE_DRAW(line, drawY);
        }
    }
}
