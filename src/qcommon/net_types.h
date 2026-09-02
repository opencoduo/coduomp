#ifndef QCOMMON_NET_TYPES_H
#define QCOMMON_NET_TYPES_H

#include "qcommon_limits.h"
#include "q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

typedef enum netadrtype_e {
    NA_BAD = 0,
    NA_BOT = 1,
    NA_LOOPBACK = 2,
    NA_BROADCAST = 3,
    NA_IP = 4,
    NA_IPX = 5,
    NA_BROADCAST_IPX = 6
} netadrtype_t;

typedef enum netsrc_e {
    NS_CLIENT = 0,
    NS_SERVER = 1
} netsrc_t;

/* Native engine address record. It is passed by value across internal engine
 * calls, but contains no pointers and retains this 20-byte layout on every
 * supported target. The Windows client and Linux server bodies agree on type
 * +0x00, IPv4 bytes +0x04, IPX bytes +0x08, and port +0x12. */
typedef struct netadr_s {
    netadrtype_t type;
    uint8_t ip[4];
    uint8_t ipx[10];
    uint16_t port;
} netadr_t;

/* The Windows client and Linux server retain the same loopback geometry. The
 * server's NET_GetLoopPacket at 0x08084b49 and NET_SendLoopPacket at
 * 0x08084c4c prove the 16-record ring, 0x57c record stride, 1400-byte payload,
 * and two-socket table used by the client implementation as well. */
enum {
    NET_LOOPBACK_QUEUE_COUNT = 2,
    NET_LOOPBACK_SOCKET_PAIR_MASK = 1,
    NET_LOOPBACK_MESSAGE_COUNT = 16,
    NET_LOOPBACK_MESSAGE_BYTES = 1400,
    NETCHAN_FRAGMENT_SIZE = 1300,
    NETCHAN_PACKET_BUFFER_SIZE = 1400
};

typedef struct loopmsg_s {
    uint8_t data[NET_LOOPBACK_MESSAGE_BYTES];
    int32_t datalen;
} loopmsg_t;

typedef struct loopback_s {
    loopmsg_t msgs[NET_LOOPBACK_MESSAGE_COUNT];
    int32_t get;
    int32_t send;
} loopback_t;

/* The Windows client and Linux server profiling bodies agree on a 60-sample
 * ring, 0x0c-byte sample, 0x2f0-byte stream, and paired 0x5e0-byte profile.
 * The Linux allocation and update bodies are at 0x08083884..0x08083b00. */
enum {
    NET_PROFILE_SAMPLE_COUNT = 60,
    NET_PROFILE_SOCKET_NAME_COUNT = 2
};

typedef enum netProfileMode_e {
    NET_PROFILE_OFF = 0,
    NET_PROFILE_CLIENT = 1,
    NET_PROFILE_SERVER = 2
} netProfileMode_t;

typedef struct netProfileSample_s {
    int32_t time;
    int32_t bytes;
    qboolean fragmented;
} netProfileSample_t;

typedef struct netProfileStream_s {
    netProfileSample_t samples[NET_PROFILE_SAMPLE_COUNT];
    int32_t ringIndex;
    int32_t bytesPerSecond;
    int32_t lastRateCalcTime;
    int32_t sampleCount;
    int32_t fragmentSampleCount;
    int32_t fragmentPercent;
    int32_t maxBytes;
    int32_t minBytes;
} netProfileStream_t;

typedef struct netProfileInfo_s {
    netProfileStream_t send;
    netProfileStream_t receive;
} netProfileInfo_t;

/* Native reliable-packet channel. CoDUOMP.exe and coduo_lnxded agree on the
 * complete i386 layout, including both 32-KiB fragment buffers and the final
 * native profile pointer. */
typedef struct netchan_s {
    netsrc_t sock;
    int32_t dropped;
    netadr_t remoteAddress;
    int32_t qport;
    int32_t incomingSequence;
    int32_t outgoingSequence;
    int32_t fragmentSequence;
    int32_t fragmentLength;
    uint8_t fragmentBuffer[MAX_MSGLEN];
    qboolean unsentFragments;
    int32_t unsentFragmentStart;
    int32_t unsentLength;
    uint8_t unsentBuffer[MAX_MSGLEN];
    netProfileInfo_t *profile;
} netchan_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(netadrtype_t) == sizeof(int32_t), "network-address type width changed");
_Static_assert(sizeof(netsrc_t) == sizeof(int32_t), "network source width changed");
_Static_assert(_Alignof(netadr_t) == 4, "network-address alignment changed");
_Static_assert(offsetof(netadr_t, type) == 0x00, "network-address type moved");
_Static_assert(offsetof(netadr_t, ip) == 0x04, "network-address IPv4 bytes moved");
_Static_assert(sizeof(((netadr_t *)0)->ip) == 0x04, "network-address IPv4 extent changed");
_Static_assert(offsetof(netadr_t, ipx) == 0x08, "network-address IPX bytes moved");
_Static_assert(sizeof(((netadr_t *)0)->ipx) == 0x0a, "network-address IPX extent changed");
_Static_assert(offsetof(netadr_t, port) == 0x12, "network-address port moved");
_Static_assert(sizeof(netadr_t) == 0x14, "network-address size changed");
_Static_assert(_Alignof(loopmsg_t) == 4, "loopback-message alignment changed");
_Static_assert(offsetof(loopmsg_t, data) == 0x000, "loopback-message data moved");
_Static_assert(sizeof(((loopmsg_t *)0)->data) == 0x578, "loopback-message data extent changed");
_Static_assert(offsetof(loopmsg_t, datalen) == 0x578, "loopback-message data length moved");
_Static_assert(sizeof(loopmsg_t) == 0x57c, "loopback-message size changed");
_Static_assert(_Alignof(loopback_t) == 4, "loopback-queue alignment changed");
_Static_assert(offsetof(loopback_t, msgs) == 0x0000, "loopback-queue messages moved");
_Static_assert(sizeof(((loopback_t *)0)->msgs) == 0x57c0, "loopback-queue message extent changed");
_Static_assert(offsetof(loopback_t, get) == 0x57c0, "loopback-queue read index moved");
_Static_assert(offsetof(loopback_t, send) == 0x57c4, "loopback-queue write index moved");
_Static_assert(sizeof(loopback_t) == 0x57c8, "loopback-queue size changed");
_Static_assert(_Alignof(netProfileSample_t) == 4, "network-profile sample alignment changed");
_Static_assert(offsetof(netProfileSample_t, time) == 0x00, "network-profile sample time moved");
_Static_assert(offsetof(netProfileSample_t, bytes) == 0x04, "network-profile sample byte count moved");
_Static_assert(offsetof(netProfileSample_t, fragmented) == 0x08, "network-profile sample fragment flag moved");
_Static_assert(sizeof(netProfileSample_t) == 0x0c, "network-profile sample size changed");
_Static_assert(_Alignof(netProfileStream_t) == 4, "network-profile stream alignment changed");
_Static_assert(offsetof(netProfileStream_t, samples) == 0x000, "network-profile samples moved");
_Static_assert(sizeof(((netProfileStream_t *)0)->samples) == 0x2d0, "network-profile sample extent changed");
_Static_assert(offsetof(netProfileStream_t, ringIndex) == 0x2d0, "network-profile ring index moved");
_Static_assert(offsetof(netProfileStream_t, bytesPerSecond) == 0x2d4, "network-profile byte rate moved");
_Static_assert(offsetof(netProfileStream_t, lastRateCalcTime) == 0x2d8, "network-profile rate timestamp moved");
_Static_assert(offsetof(netProfileStream_t, sampleCount) == 0x2dc, "network-profile sample count moved");
_Static_assert(offsetof(netProfileStream_t, fragmentSampleCount) == 0x2e0, "network-profile fragment count moved");
_Static_assert(offsetof(netProfileStream_t, fragmentPercent) == 0x2e4, "network-profile fragment percentage moved");
_Static_assert(offsetof(netProfileStream_t, maxBytes) == 0x2e8, "network-profile maximum bytes moved");
_Static_assert(offsetof(netProfileStream_t, minBytes) == 0x2ec, "network-profile minimum bytes moved");
_Static_assert(sizeof(netProfileStream_t) == 0x2f0, "network-profile stream size changed");
_Static_assert(_Alignof(netProfileInfo_t) == 4, "network-profile object alignment changed");
_Static_assert(offsetof(netProfileInfo_t, send) == 0x000, "network-profile send stream moved");
_Static_assert(offsetof(netProfileInfo_t, receive) == 0x2f0, "network-profile receive stream moved");
_Static_assert(sizeof(netProfileInfo_t) == 0x5e0, "network-profile object size changed");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(netchan_t) == 4, "i386 netchan alignment changed");
_Static_assert(offsetof(netchan_t, sock) == 0x00000, "i386 netchan socket source moved");
_Static_assert(offsetof(netchan_t, dropped) == 0x00004, "i386 netchan dropped-packet count moved");
_Static_assert(offsetof(netchan_t, remoteAddress) == 0x00008, "i386 netchan remote address moved");
_Static_assert(offsetof(netchan_t, qport) == 0x0001c, "i386 netchan qport moved");
_Static_assert(offsetof(netchan_t, incomingSequence) == 0x00020, "i386 netchan incoming sequence moved");
_Static_assert(offsetof(netchan_t, outgoingSequence) == 0x00024, "i386 netchan outgoing sequence moved");
_Static_assert(offsetof(netchan_t, fragmentSequence) == 0x00028, "i386 netchan fragment sequence moved");
_Static_assert(offsetof(netchan_t, fragmentLength) == 0x0002c, "i386 netchan fragment length moved");
_Static_assert(offsetof(netchan_t, fragmentBuffer) == 0x00030, "i386 netchan fragment buffer moved");
_Static_assert(sizeof(((netchan_t *)0)->fragmentBuffer) == 0x08000, "i386 netchan fragment-buffer extent changed");
_Static_assert(offsetof(netchan_t, unsentFragments) == 0x08030, "i386 netchan unsent-fragment flag moved");
_Static_assert(offsetof(netchan_t, unsentFragmentStart) == 0x08034, "i386 netchan unsent-fragment cursor moved");
_Static_assert(offsetof(netchan_t, unsentLength) == 0x08038, "i386 netchan unsent length moved");
_Static_assert(offsetof(netchan_t, unsentBuffer) == 0x0803c, "i386 netchan unsent buffer moved");
_Static_assert(sizeof(((netchan_t *)0)->unsentBuffer) == 0x08000, "i386 netchan unsent-buffer extent changed");
_Static_assert(offsetof(netchan_t, profile) == 0x1003c, "i386 netchan profile pointer moved");
_Static_assert(sizeof(netchan_t) == 0x10040, "i386 netchan size changed");
#endif
#endif

#endif
