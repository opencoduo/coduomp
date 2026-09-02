#include "net_text.h"

#include "huffman.h"
#include "net_oob_services.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    NET_OOB_MARKER_BYTES = sizeof(int32_t),
    NET_OOB_COMPRESS_SKIP_BYTES = 12,
    NET_OOB_PRINT_BUFFER_SIZE = 32768,
    NET_OOB_DATA_BUFFER_SIZE = 65536,
    NET_OOB_DATA_PAYLOAD_CAPACITY = NET_OOB_DATA_BUFFER_SIZE - NET_OOB_MARKER_BYTES
};

/*
 * Complete connectionless packet-construction cluster shared by CoDUOMP and
 * the Linux dedicated engine. The supporting Mac client exports the same
 * canonical names. Packet geometry, unbounded formatting/copies, compression,
 * transport arguments, and post-send profile ordering agree.
 *
 * Function                    Windows       Linux
 * NET_OutOfBandPrint          0x0044e140    0x08084d76
 * NET_OutOfBandData           0x0044e240    0x08084e2c
 * NET_OutOfBandPbPacket       0x0044e340    0x08084f14
 *
 * Only profile ownership differs: CoDUOMP selects its client or server
 * profile, while the dedicated engine has only the server profile. That edge
 * remains target-owned in net_oob_services.h.
 */

void Com_Printf(const char *format, ...);

void NET_OutOfBandPrint(netsrc_t source, netadr_t address, const char *format, ...)
{
    uint8_t packet[NET_OOB_PRINT_BUFFER_SIZE];
    va_list arguments;

    packet[0] = UINT8_MAX;
    packet[1] = UINT8_MAX;
    packet[2] = UINT8_MAX;
    packet[3] = UINT8_MAX;
    va_start(arguments, format);
    const size_t payloadCapacity = sizeof(packet) - NET_OOB_MARKER_BYTES;
    const int32_t payloadLength = vsnprintf((char *)packet + NET_OOB_MARKER_BYTES, payloadCapacity, format, arguments);
    va_end(arguments);

    /* NOT_FROM_ORIGINAL_SOURCE: emit a connectionless message only when the
     * complete formatted payload fits its protocol buffer. */
    if (payloadLength < 0 || (size_t)payloadLength >= payloadCapacity) {
        Com_Printf("NET_OutOfBandPrint: formatted packet too large\n");
        return;
    }

    const int32_t packetLength = payloadLength + NET_OOB_MARKER_BYTES;
    NET_SendPacket(source, packetLength, packet, address);
    net_compat_profile_oob_packet(source, packetLength);
}

void NET_OutOfBandData(netsrc_t source, netadr_t address, const uint8_t *data, int32_t length)
{
    uint8_t packet[NET_OOB_DATA_BUFFER_SIZE];
    msg_t message;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0 || length > NET_OOB_DATA_PAYLOAD_CAPACITY) {
        Com_Printf("NET_OutOfBandData: invalid payload length %i\n", length);
        return;
    }

    packet[0] = UINT8_MAX;
    packet[1] = UINT8_MAX;
    packet[2] = UINT8_MAX;
    packet[3] = UINT8_MAX;
    if (length > 0) {
        memcpy(packet + NET_OOB_MARKER_BYTES, data, (size_t)length);
    }

    message.data = packet;
    message.cursize = length + NET_OOB_MARKER_BYTES;
    Huff_Compress(&message, NET_OOB_COMPRESS_SKIP_BYTES);
    NET_SendPacket(source, message.cursize, message.data, address);
    net_compat_profile_oob_packet(source, message.cursize);
}

void NET_OutOfBandPbPacket(netsrc_t source, netadr_t address, const void *data, int32_t length)
{
    uint8_t packet[NET_OOB_DATA_BUFFER_SIZE];

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0 || length > NET_OOB_DATA_PAYLOAD_CAPACITY) {
        Com_Printf("NET_OutOfBandPbPacket: invalid payload length %i\n", length);
        return;
    }

    packet[0] = UINT8_MAX;
    packet[1] = UINT8_MAX;
    packet[2] = UINT8_MAX;
    packet[3] = UINT8_MAX;
    if (length > 0)
        memcpy(packet + NET_OOB_MARKER_BYTES, data, (size_t)length);

    const int32_t packetLength = length + NET_OOB_MARKER_BYTES;
    NET_SendPacket(source, packetLength, packet, address);
    net_compat_profile_oob_packet(source, packetLength);
}
