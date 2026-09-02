#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include "net_address.h"
#include "../client/cgame.h"
#include "../server/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
typedef SOCKET net_socket_handle_t;
#define NET_INVALID_SOCKET_HANDLE INVALID_SOCKET
#define NET_CLOSED_SOCKET_HANDLE ((net_socket_handle_t)0)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef int net_socket_handle_t;
#define NET_INVALID_SOCKET_HANDLE (-1)
#define NET_CLOSED_SOCKET_HANDLE NET_INVALID_SOCKET_HANDLE
#endif

enum {
    NET_PLATFORM_SOCKET_COUNT = 2,
    NET_PLATFORM_IP_SOCKET = 0,
    NET_PLATFORM_IPX_SOCKET = 1,
    NET_SOCKS_UDP_HEADER_BYTES = 10,
    NET_PLATFORM_AF_IPX = 6,
    NET_PLATFORM_IPX_PROTOCOL = 1000,
    NET_LOCAL_IP_ADDRESS_COUNT = 16,
    NET_IPV4_ADDRESS_BYTES = 4,
    NET_IPV4_PRIVATE_A_PREFIX = 10,
    NET_IPV4_LOOPBACK_PREFIX = 127,
    NET_IPV4_LINK_LOCAL_PREFIX = 169,
    NET_IPV4_LINK_LOCAL_SECOND_OCTET = 254,
    NET_IPV4_PRIVATE_B_PREFIX = 172,
    NET_IPV4_PRIVATE_B_SECOND_OCTET_MASK = 240,
    NET_IPV4_PRIVATE_B_SECOND_OCTET = 16,
    NET_IPV4_PRIVATE_C_PREFIX = 192,
    NET_IPV4_PRIVATE_C_SECOND_OCTET = 168,
    NET_HOSTNAME_BUFFER_SIZE = 256,
    NET_SOCKET_OPTION_ENABLED = 1,
    NET_SOCKET_NO_PORT = -1,
    NET_PORT_RETRY_COUNT = 10,
    NET_IPV4_OCTET_MASK = 255,
    NET_SOCKS_VERSION = 5,
    NET_SOCKS_AUTH_VERSION = 1,
    NET_SOCKS_METHOD_NO_AUTH = 0,
    NET_SOCKS_METHOD_USERNAME_PASSWORD = 2,
    NET_SOCKS_COMMAND_UDP_ASSOCIATE = 3,
    NET_SOCKS_ADDRESS_IPV4 = 1,
    NET_SOCKS_REPLY_SUCCEEDED = 0,
    NET_SOCKS_CONTROL_BUFFER_BYTES = 64,
    NET_SOCKS_SEND_BUFFER_BYTES = 4096,
    NET_SOCKS_METHOD_REPLY_BYTES = 2,
    NET_SOCKS_AUTH_REPLY_BYTES = 2,
    NET_SOCKS_GREETING_NO_AUTH_BYTES = 3,
    NET_SOCKS_GREETING_AUTH_BYTES = 4,
    NET_SOCKS_AUTH_FIXED_BYTES = 3,
    NET_SOCKS_UDP_REQUEST_BYTES = 10,
    NET_SOCKS_REPLY_HEADER_BYTES = 4,
    NET_SOCKS_IPV4_REPLY_BYTES = 10,
    NET_WINSOCK_VERSION_1_1 = 257,
};

/* Original Win32 networking globals: show-packets cvar 0x0491cd88, network
 * profiler cvar 0x04927d30, loopback queues 0x0491cda0, and client/server
 * profiler pointers 0x04df9698/0x0491cc48 respectively. The client pointer is
 * the clc.netProfile field and the server pointer is the svs.netProfile
 * field. */
cvar_t *showpackets;
cvar_t *net_profile;
loopback_t net_loopbacks[NET_LOOPBACK_QUEUE_COUNT]; /* 0x0491cda0 */

/* Original Win32 socket state. SOCKET is pointer-width on Win64; retaining
 * its native type avoids narrowing the platform handle. Retail Windows uses
 * zero for a closed endpoint. Native POSIX uses -1 so descriptor zero remains
 * a valid socket. */
static net_socket_handle_t netIpSocket =
    NET_CLOSED_SOCKET_HANDLE;                 /* original 0x009d04f4 */
static net_socket_handle_t netIpxSocket =
    NET_CLOSED_SOCKET_HANDLE;                 /* original 0x009cf310 */
static qboolean netUsingSocks;                /* original 0x0389fdd4 */
static cvar_t *netSocksEnabled;               /* original 0x009cf2f4 */
static cvar_t *netSocksServer;                /* original 0x009d04f0 */
static cvar_t *netSocksPort;                  /* original 0x009cf35c */
static cvar_t *netSocksUsername;              /* original 0x009cf314 */
static cvar_t *netSocksPassword;              /* original 0x009d04f8 */
static cvar_t *netNoUdp;                      /* original 0x009d04fc */
static cvar_t *netNoIpx;                      /* original 0x009cf30c */
static net_socket_handle_t netSocksSocket =
    NET_CLOSED_SOCKET_HANDLE;                 /* original 0x009cf2f8 */
static qboolean netWinsockInitialized;        /* original 0x0389fdd0 */
static qboolean netNetworkingEnabled;         /* original 0x0389fdd8 */
#if defined(_WIN32)
static WSADATA netWinsockData;                /* original 0x009d0360 */
#endif
static struct sockaddr_in netSocksRelayAddress;
                                                /* original 0x009cf2fc */
static uint8_t netSocksSendBuffer[
    NET_SOCKS_SEND_BUFFER_BYTES];
                                                /* original 0x009cf360 */
static uint8_t sysLocalIpAddresses[
    NET_LOCAL_IP_ADDRESS_COUNT][NET_IPV4_ADDRESS_BYTES];
                                                /* original 0x009cf318 */
static int32_t sysLocalIpAddressCount;           /* original 0x009cf358 */

/* Winsock's 16-byte SOCKADDR_IPX layout. The modern Unix targets do not
 * expose AF_IPX, but retaining the typed record keeps the recovered Windows
 * conversion honest and lets unsupported IPX addresses remain inert there. */
typedef struct net_sockaddr_ipx_s {
    int16_t family;
    uint8_t network[4];
    uint8_t node[6];
    uint16_t socket;
} net_sockaddr_ipx_t;

typedef union net_socket_address_u {
    struct sockaddr generic;
    struct sockaddr_in ipv4;
    net_sockaddr_ipx_t ipx;
} net_socket_address_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(net_sockaddr_ipx_t) == 2,
               "i386 Winsock IPX address alignment changed");
_Static_assert(offsetof(net_sockaddr_ipx_t, family) == 0x00,
               "i386 Winsock IPX family moved");
_Static_assert(offsetof(net_sockaddr_ipx_t, network) == 0x02,
               "i386 Winsock IPX network moved");
_Static_assert(sizeof(((net_sockaddr_ipx_t *)0)->network) == 0x04,
               "i386 Winsock IPX network extent changed");
_Static_assert(offsetof(net_sockaddr_ipx_t, node) == 0x06,
               "i386 Winsock IPX node moved");
_Static_assert(sizeof(((net_sockaddr_ipx_t *)0)->node) == 0x06,
               "i386 Winsock IPX node extent changed");
_Static_assert(offsetof(net_sockaddr_ipx_t, socket) == 0x0c,
               "i386 Winsock IPX socket moved");
_Static_assert(sizeof(net_sockaddr_ipx_t) == 0x0e,
               "i386 Winsock IPX address size changed");
_Static_assert(_Alignof(net_socket_address_t) == 4,
               "i386 socket-address carrier alignment changed");
_Static_assert(offsetof(net_socket_address_t, generic) == 0x00,
               "i386 generic socket-address view moved");
_Static_assert(offsetof(net_socket_address_t, ipv4) == 0x00,
               "i386 IPv4 socket-address view moved");
_Static_assert(offsetof(net_socket_address_t, ipx) == 0x00,
               "i386 IPX socket-address view moved");
_Static_assert(sizeof(net_socket_address_t) == 0x10,
               "i386 socket-address carrier size changed");
#endif

/* Source: CoDUOMP.exe 0x0046c8e0..0x0046ca62.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046c8e0_0046ca63.mcode, its original
 * dispatch tables at 0x0046ca64..0x0046cb23, and strings at
 * 0x0059fa00..0x0059fca7.
 * Name: established by the socket-error callers and the recovered Linux
 * engine counterpart. The default string really is "NO ERROR", including for
 * unlisted Winsock errors. */
const char *NET_ErrorString(void)
{
#if defined(_WIN32)
    switch (WSAGetLastError()) {
    case WSAEINTR: return "WSAEINTR";
    case WSAEBADF: return "WSAEBADF";
    case WSAEACCES: return "WSAEACCES";
    case WSAEFAULT: return "WSAEFAULT";
    case WSAEINVAL: return "WSAEINVAL";
    case WSAEMFILE: return "WSAEMFILE";
    case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
    case WSAEINPROGRESS: return "WSAEINPROGRESS";
    case WSAEALREADY: return "WSAEALREADY";
    case WSAENOTSOCK: return "WSAENOTSOCK";
    case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
    case WSAEMSGSIZE: return "WSAEMSGSIZE";
    case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
    case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
    case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
    case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
    case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
    case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
    case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
    case WSAEADDRINUSE: return "WSAEADDRINUSE";
    case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
    case WSAENETDOWN: return "WSAENETDOWN";
    case WSAENETUNREACH: return "WSAENETUNREACH";
    case WSAENETRESET: return "WSAENETRESET";
    case WSAECONNABORTED:
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        return "WSWSAECONNABORTEDAEINTR";
    case WSAECONNRESET: return "WSAECONNRESET";
    case WSAENOBUFS: return "WSAENOBUFS";
    case WSAEISCONN: return "WSAEISCONN";
    case WSAENOTCONN: return "WSAENOTCONN";
    case WSAESHUTDOWN: return "WSAESHUTDOWN";
    case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
    case WSAETIMEDOUT: return "WSAETIMEDOUT";
    case WSAECONNREFUSED: return "WSAECONNREFUSED";
    case WSAELOOP: return "WSAELOOP";
    case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
    case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
    case WSASYSNOTREADY: return "WSASYSNOTREADY";
    case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
    case WSANOTINITIALISED: return "WSANOTINITIALISED";
    case WSAEDISCON: return "WSAEDISCON";
    case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
    case WSATRY_AGAIN: return "WSATRY_AGAIN";
    case WSANO_RECOVERY: return "WSANO_RECOVERY";
    case WSANO_DATA: return "WSANO_DATA";
    default: return "NO ERROR";
    }
#else
    /* The same API in the recovered Linux engine returns strerror(errno). */
    return strerror(errno);
#endif
}

/* Source: CoDUOMP.exe 0x0046cb30..0x0046cbc9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046cb30_0046cbca.mcode.
 * Name: established id networking helper NetadrToSockadr. The original
 * accepts IPv4/IPX broadcast and unicast addresses and clears the full
 * 16-byte destination before selecting a representation. */
static void NetadrToSockadr(const netadr_t *address,
                            net_socket_address_t *socketAddress)
{
    memset(socketAddress, 0, sizeof(*socketAddress));

    switch (address->type) {
    case NA_BROADCAST:
        socketAddress->ipv4.sin_family = AF_INET;
        socketAddress->ipv4.sin_port = address->port;
        socketAddress->ipv4.sin_addr.s_addr = UINT32_MAX;
        break;
    case NA_IP:
        socketAddress->ipv4.sin_family = AF_INET;
        memcpy(&socketAddress->ipv4.sin_addr, address->ip,
               sizeof(address->ip));
        socketAddress->ipv4.sin_port = address->port;
        break;
    case NA_IPX:
        socketAddress->ipx.family = NET_PLATFORM_AF_IPX;
        memcpy(socketAddress->ipx.network, address->ipx,
               sizeof(socketAddress->ipx.network));
        memcpy(socketAddress->ipx.node,
               &address->ipx[sizeof(socketAddress->ipx.network)],
               sizeof(socketAddress->ipx.node));
        socketAddress->ipx.socket = address->port;
        break;
    case NA_BROADCAST_IPX:
        socketAddress->ipx.family = NET_PLATFORM_AF_IPX;
        memset(socketAddress->ipx.node, UINT8_MAX,
               sizeof(socketAddress->ipx.node));
        socketAddress->ipx.socket = address->port;
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x0046cbd0..0x0046cc1e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046cbd0_0046cc1f.mcode.
 * Name: established id networking helper SockadrToNetadr. As in the
 * executable, fields not belonging to the selected address family are left
 * untouched. */
static void SockadrToNetadr(const net_socket_address_t *socketAddress,
                            netadr_t *address)
{
    if (socketAddress->generic.sa_family == AF_INET) {
        address->type = NA_IP;
        memcpy(address->ip, &socketAddress->ipv4.sin_addr,
               sizeof(address->ip));
        address->port = socketAddress->ipv4.sin_port;
    } else if (socketAddress->ipx.family == NET_PLATFORM_AF_IPX) {
        address->type = NA_IPX;
        memcpy(address->ipx, socketAddress->ipx.network,
               sizeof(address->ipx));
        address->port = socketAddress->ipx.socket;
    }
}

/* Source: CoDUOMP.exe 0x0046cc20..0x0046ce84.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046cc20_0046ce85.mcode, including
 * imports for sscanf, inet_addr, and gethostbyname.
 * Name: established id networking helper Sys_StringToSockaddr. A 21-byte
 * `xxxxxxxx.xxxxxxxxxxxx` spelling is decoded as four IPX network bytes and
 * six node bytes; every other spelling is resolved as dotted IPv4 or a host
 * name. */
static qboolean Sys_StringToSockaddr(
    const char *text, net_socket_address_t *socketAddress)
{
    enum {
        NET_IPX_TEXT_LENGTH = 21,
        NET_IPX_NODE_TEXT_OFFSET = 9
    };
    static const char hexadecimalFormat[] = "%x";

    memset(socketAddress, 0, sizeof(*socketAddress));

    if (strlen(text) == NET_IPX_TEXT_LENGTH && text[8] == '.') {
        socketAddress->ipx.family = NET_PLATFORM_AF_IPX;
        socketAddress->ipx.socket = 0;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        for (size_t index = 0;
             index < sizeof(socketAddress->ipx.network); ++index) {
            char pair[3] = {
                text[index * 2],
                text[index * 2 + 1],
                '\0'
            };
            unsigned int value;
            if (sscanf(pair, hexadecimalFormat, &value) != 1) {
                return qfalse;
            }
            socketAddress->ipx.network[index] = (uint8_t)value;
        }

        for (size_t index = 0;
             index < sizeof(socketAddress->ipx.node); ++index) {
            const size_t textOffset =
                NET_IPX_NODE_TEXT_OFFSET + index * 2;
            char pair[3] = {
                text[textOffset],
                text[textOffset + 1],
                '\0'
            };
            unsigned int value;
            if (sscanf(pair, hexadecimalFormat, &value) != 1) {
                return qfalse;
            }
            socketAddress->ipx.node[index] = (uint8_t)value;
        }
        return qtrue;
    }

    socketAddress->ipv4.sin_family = AF_INET;
    socketAddress->ipv4.sin_port = 0;

    if (text[0] >= '0' && text[0] <= '9') {
        socketAddress->ipv4.sin_addr.s_addr = inet_addr(text);
        return qtrue;
    }

    const struct hostent *host = gethostbyname(text);
    if (host == NULL)
        return qfalse;

    memcpy(&socketAddress->ipv4.sin_addr,
           host->h_addr_list[0],
           sizeof(socketAddress->ipv4.sin_addr));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0046ce90..0x0046ced5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046ce90_0046ced6.mcode.
 * Name and signature: established id networking boundary Sys_StringToAdr.
 * It is exactly sockaddr resolution followed by the typed address conversion. */
qboolean Sys_StringToAdr(const char *name, netadr_t *address)
{
    net_socket_address_t socketAddress;
    if (Sys_StringToSockaddr(name, &socketAddress) == qfalse)
        return qfalse;

    SockadrToNetadr(&socketAddress, address);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0046cee0..0x0046d0ec.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046cee0_0046d0ed.mcode, Winsock import
 * identities, and the exact same-module Mac symbol Sys_GetPacket.
 * The Windows routine polls the IPv4 and IPX sockets in that order, unwraps
 * the ten-byte SOCKS5 UDP relay prefix, drops packets that exactly fill the
 * receive buffer, and otherwise returns one packet per call. */
qboolean Sys_GetPacket(netadr_t *address, msg_t *message)
{
    for (int32_t socketIndex = 0;
         socketIndex < NET_PLATFORM_SOCKET_COUNT; ++socketIndex) {
        const net_socket_handle_t socketHandle =
            socketIndex == NET_PLATFORM_IP_SOCKET
                ? netIpSocket
                : netIpxSocket;
        if (socketHandle == NET_CLOSED_SOCKET_HANDLE)
            continue;

        net_socket_address_t sourceAddress;
#if defined(_WIN32)
        int32_t sourceAddressLength = sizeof(sourceAddress);
        const int32_t received = recvfrom(
            socketHandle, (char *)message->data, message->maxsize, 0,
            &sourceAddress.generic, &sourceAddressLength);
        if (received == SOCKET_ERROR) {
            const int32_t error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK &&
                error != WSAECONNRESET) {
                Com_Printf("NET_GetPacket: %s\n", NET_ErrorString());
            }
            continue;
        }
#else
        socklen_t sourceAddressLength = sizeof(sourceAddress);
        const ssize_t receivedNative = recvfrom(
            socketHandle, message->data, (size_t)message->maxsize, 0,
            &sourceAddress.generic, &sourceAddressLength);
        if (receivedNative == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK &&
                errno != ECONNREFUSED) {
                Com_Printf("NET_GetPacket: %s\n", NET_ErrorString());
            }
            continue;
        }
        const int32_t received = (int32_t)receivedNative;
#endif

        if (socketIndex == NET_PLATFORM_IP_SOCKET) {
            memset(sourceAddress.ipv4.sin_zero, 0,
                   sizeof(sourceAddress.ipv4.sin_zero));
        }

        if (netUsingSocks != qfalse &&
            socketIndex == NET_PLATFORM_IP_SOCKET &&
            memcmp(
                &sourceAddress.ipv4, &netSocksRelayAddress,
                (size_t)sourceAddressLength) == 0) {
            if (received < NET_SOCKS_UDP_HEADER_BYTES ||
                message->data[0] != 0 ||
                message->data[1] != 0 ||
                message->data[2] != 0 ||
                message->data[3] != NET_SOCKS_ADDRESS_IPV4) {
                continue;
            }
            address->type = NA_IP;
            memcpy(address->ip, &message->data[4],
                   sizeof(address->ip));
            memcpy(&address->port, &message->data[8],
                   sizeof(address->port));
            message->readcount = NET_SOCKS_UDP_HEADER_BYTES;
        } else {
            SockadrToNetadr(&sourceAddress, address);
            message->readcount = 0;
        }

        if (received != message->maxsize) {
            message->cursize = received;
            return qtrue;
        }

        Com_Printf(
            "Oversize packet from %s\n",
            NET_AdrToString(*address));
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0044e090..0x0044e13c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e090_0044e13d.mcode.
 * Name and argument roles: exact same-module Mac symbol NET_SendPacket. Local
 * addresses are copied into the opposite endpoint's 16-message ring; bad and
 * bot addresses are dropped, and other address types cross the platform
 * Sys_SendPacket boundary. */
void NET_SendPacket(netsrc_t source, int32_t length, const void *data,
                    netadr_t address)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("NET_SendPacket: invalid packet length %i\n", length);
        return;
    }

    if (showpackets->integer != 0) {
        int32_t marker;
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (length >= (int32_t)sizeof(marker)) {
            memcpy(&marker, data, sizeof(marker));
            if (marker == -1)
                Com_Printf("send packet %4i\n", length);
        }
    }

    if (address.type == NA_LOOPBACK) {
        NET_SendLoopPacket(source, length, data);
        return;
    }

    if (address.type == NA_BAD || address.type == NA_BOT)
        return;
    Sys_SendPacket(length, data, address);
}

/* Source: CoDUOMP.exe 0x0046d0f0..0x0046d22f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d0f0_0046d230.mcode.
 * Name and three-argument interface: established id networking boundary and
 * the call at 0x0044e132. The source endpoint selector is intentionally not
 * part of this platform call. SOCKS5 UDP relay traffic receives the original
 * ten-byte header before being sent to the configured relay. */
void Sys_SendPacket(int32_t length, const void *data, netadr_t address)
{
    net_socket_handle_t socketHandle;

    switch (address.type) {
    case NA_BROADCAST:
    case NA_IP:
        socketHandle = netIpSocket;
        break;
    case NA_IPX:
    case NA_BROADCAST_IPX:
        socketHandle = netIpxSocket;
        break;
    default:
        Com_Error(ERR_FATAL,
                  "\x15Sys_SendPacket: bad address type");
        return;
    }

    if (socketHandle == NET_CLOSED_SOCKET_HANDLE)
        return;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("Sys_SendPacket: invalid packet length %i\n", length);
        return;
    }

    net_socket_address_t socketAddress;
    NetadrToSockadr(&address, &socketAddress);

    const void *sendData = data;
    int32_t sendLength = length;
    const struct sockaddr *destination = &socketAddress.generic;

    if (netUsingSocks != qfalse && address.type == NA_IP) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (length > NET_SOCKS_SEND_BUFFER_BYTES - NET_SOCKS_UDP_HEADER_BYTES) {
            Com_Printf("Sys_SendPacket: SOCKS packet length %i exceeds staging capacity\n", length);
            return;
        }
        netSocksSendBuffer[0] = 0;
        netSocksSendBuffer[1] = 0;
        netSocksSendBuffer[2] = 0;
        netSocksSendBuffer[3] = 1;
        memcpy(&netSocksSendBuffer[4], address.ip, sizeof(address.ip));
        memcpy(&netSocksSendBuffer[8], &address.port,
               sizeof(address.port));
        memcpy(&netSocksSendBuffer[NET_SOCKS_UDP_HEADER_BYTES],
               data, (size_t)length);

        sendData = netSocksSendBuffer;
        sendLength += NET_SOCKS_UDP_HEADER_BYTES;
        destination =
            (const struct sockaddr *)&netSocksRelayAddress;
    }

#if defined(_WIN32)
    const int32_t sent = sendto(
        socketHandle, (const char *)sendData, sendLength, 0,
        destination, sizeof(net_socket_address_t));
    if (sent != SOCKET_ERROR)
        return;

    const int32_t error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK)
        return;
    if (error == WSAEADDRNOTAVAIL &&
        (address.type == NA_BROADCAST ||
         address.type == NA_BROADCAST_IPX)) {
        return;
    }
#else
    const ssize_t sent = sendto(
        socketHandle, sendData, (size_t)sendLength, 0,
        destination, sizeof(net_socket_address_t));
    if (sent != -1)
        return;

    const int32_t error = errno;
    if (error == EAGAIN || error == EWOULDBLOCK)
        return;
    if (error == EADDRNOTAVAIL &&
        (address.type == NA_BROADCAST ||
         address.type == NA_BROADCAST_IPX)) {
        return;
    }
#endif

    Com_Printf("Sys_SendPacket: %s\n", NET_ErrorString());
}

/* Source: CoDUOMP.exe 0x0046d230..0x0046d2b0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d230_0046d2b1.mcode.
 * Role and signature: same platform helper as the recovered Linux engine's
 * Sys_SendPacketByName. It resolves the textual host, installs the caller's
 * network-order port, then crosses the ordinary three-argument send boundary. */
void Sys_SendPacketByName(const char *name, uint16_t port,
                          const void *data, int32_t length)
{
    netadr_t address = {0};
    address.type = NA_IP;

    if (Sys_StringToAdr(name, &address) == qfalse)
        return;

    address.port = htons(port);
    Sys_SendPacket(length, data, address);
}

/* Source: CoDUOMP.exe 0x0046d2c0..0x0046d360.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d2c0_0046d361.mcode.
 * Name: exact same-module Mac symbol Sys_IsLANAddress. Besides loopback and
 * IPX, the Windows executable treats NA_BAD as local, recognizes RFC1918,
 * loopback, and IPv4 link-local prefixes, then compares the first three
 * octets against the discovered local-interface table. */
qboolean Sys_IsLANAddress(netadr_t address)
{
    if (address.type == NA_LOOPBACK ||
        address.type == NA_BAD ||
        address.type == NA_IPX) {
        return qtrue;
    }
    if (address.type != NA_IP)
        return qfalse;

    if (address.ip[0] == NET_IPV4_PRIVATE_A_PREFIX ||
        address.ip[0] == NET_IPV4_LOOPBACK_PREFIX ||
        (address.ip[0] == NET_IPV4_LINK_LOCAL_PREFIX &&
         address.ip[1] == NET_IPV4_LINK_LOCAL_SECOND_OCTET) ||
        (address.ip[0] == NET_IPV4_PRIVATE_B_PREFIX &&
         (address.ip[1] &
          NET_IPV4_PRIVATE_B_SECOND_OCTET_MASK) ==
             NET_IPV4_PRIVATE_B_SECOND_OCTET) ||
        (address.ip[0] == NET_IPV4_PRIVATE_C_PREFIX &&
         address.ip[1] == NET_IPV4_PRIVATE_C_SECOND_OCTET)) {
        return qtrue;
    }

    for (int32_t index = 0;
         index < sysLocalIpAddressCount; ++index) {
        if (address.ip[0] == sysLocalIpAddresses[index][0] &&
            address.ip[1] == sysLocalIpAddresses[index][1] &&
            address.ip[2] == sysLocalIpAddresses[index][2]) {
            return qtrue;
        }
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0046d370..0x0046d3bc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d370_0046d3bd.mcode.
 * Name: exact same-module Mac symbol Sys_ShowIP. */
void Sys_ShowIP(void)
{
    for (int32_t index = 0;
         index < sysLocalIpAddressCount; ++index) {
        Com_Printf(
            "IP: %i.%i.%i.%i\n",
            sysLocalIpAddresses[index][0],
            sysLocalIpAddresses[index][1],
            sysLocalIpAddresses[index][2],
            sysLocalIpAddresses[index][3]);
    }
}

/* Source: CoDUOMP.exe 0x0046d3c0..0x0046d551.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d3c0_0046d552.mcode and its
 * Winsock imports for socket, ioctlsocket, setsockopt, bind, and closesocket.
 * Role name: established id networking helper Sys_OpenIPSocket. As in the
 * executable, failures before bind return without closing the newly created
 * socket; bind failure closes it. */
static net_socket_handle_t Sys_OpenIPSocket(
    const char *netInterface, int32_t port)
{
    if (netInterface != NULL) {
        Com_Printf("Opening IP socket: %s:%i\n",
                   netInterface, port);
    } else {
        Com_Printf("Opening IP socket: localhost:%i\n", port);
    }

    const net_socket_handle_t socketHandle =
        socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
    if (socketHandle == INVALID_SOCKET) {
        if (WSAGetLastError() != WSAEAFNOSUPPORT) {
            Com_Printf(
                "WARNING: UDP_OpenSocket: socket: %s\n",
                NET_ErrorString());
        }
        return NET_CLOSED_SOCKET_HANDLE;
    }

    u_long nonblocking = NET_SOCKET_OPTION_ENABLED;
    if (ioctlsocket(socketHandle, FIONBIO, &nonblocking) ==
        SOCKET_ERROR) {
        Com_Printf(
            "WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n",
            NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        closesocket(socketHandle);
        return NET_CLOSED_SOCKET_HANDLE;
    }

    const int32_t broadcast = NET_SOCKET_OPTION_ENABLED;
    if (setsockopt(
            socketHandle, SOL_SOCKET, SO_BROADCAST,
            (const char *)&broadcast, sizeof(broadcast)) ==
        SOCKET_ERROR) {
        Com_Printf(
            "WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n",
            NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        closesocket(socketHandle);
        return NET_CLOSED_SOCKET_HANDLE;
    }
#else
    if (socketHandle == -1) {
        if (errno != EAFNOSUPPORT) {
            Com_Printf(
                "WARNING: UDP_OpenSocket: socket: %s\n",
                NET_ErrorString());
        }
        return NET_CLOSED_SOCKET_HANDLE;
    }

#if defined(__APPLE__)
    /*
     * NOT_FROM_ORIGINAL_SOURCE: the original PPC Mac client uses
     * OpenTransport's OTSendUData, whose failure is returned to
     * Sys_SendPacket.  A native Darwin sendto can instead terminate the
     * process with SIGPIPE after an unreachable authorization endpoint.
     * SO_NOSIGPIPE restores the original error-return boundary without
     * changing the shipped Windows/i386 socket path.
     */
    const int32_t noSigPipe = NET_SOCKET_OPTION_ENABLED;
    if (setsockopt(
            socketHandle, SOL_SOCKET, SO_NOSIGPIPE,
            &noSigPipe, sizeof(noSigPipe)) == -1) {
        Com_Printf(
            "WARNING: UDP_OpenSocket: setsockopt SO_NOSIGPIPE: %s\n",
            NET_ErrorString());
        close(socketHandle);
        return NET_CLOSED_SOCKET_HANDLE;
    }
#endif

    int32_t nonblocking = NET_SOCKET_OPTION_ENABLED;
    if (ioctl(socketHandle, FIONBIO, &nonblocking) == -1) {
        Com_Printf(
            "WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n",
            NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        close(socketHandle);
        return NET_CLOSED_SOCKET_HANDLE;
    }

    const int32_t broadcast = NET_SOCKET_OPTION_ENABLED;
    if (setsockopt(
            socketHandle, SOL_SOCKET, SO_BROADCAST,
            &broadcast, sizeof(broadcast)) == -1) {
        Com_Printf(
            "WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n",
            NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        close(socketHandle);
        return NET_CLOSED_SOCKET_HANDLE;
    }
#endif

    net_socket_address_t address;
    if (netInterface != NULL &&
        netInterface[0] != '\0' &&
        Q_stricmp(netInterface, "localhost") != 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (Sys_StringToSockaddr(netInterface, &address) == qfalse) {
            Com_Printf("WARNING: UDP_OpenSocket: couldn't resolve interface %s\n", netInterface);
#if defined(_WIN32)
            closesocket(socketHandle);
#else
            close(socketHandle);
#endif
            return NET_CLOSED_SOCKET_HANDLE;
        }
    } else {
        memset(&address, 0, sizeof(address));
        address.ipv4.sin_addr.s_addr = 0;
    }

    address.ipv4.sin_port =
        port == NET_SOCKET_NO_PORT ? 0 : htons((uint16_t)port);
    address.ipv4.sin_family = AF_INET;

    if (bind(
            socketHandle, &address.generic,
            sizeof(address)) != 0) {
        Com_Printf(
            "WARNING: UDP_OpenSocket: bind: %s\n",
            NET_ErrorString());
#if defined(_WIN32)
        closesocket(socketHandle);
#else
        close(socketHandle);
#endif
        return NET_CLOSED_SOCKET_HANDLE;
    }

    return socketHandle;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
static qboolean coduomp_net_socks_send_all(const uint8_t *data, size_t length)
{
    size_t transferred = 0;

    while (transferred < length) {
#if defined(_WIN32)
        const int32_t result = send(netSocksSocket,
                                    (const char *)&data[transferred],
                                    (int32_t)(length - transferred), 0);
#else
        const ssize_t result = send(netSocksSocket, &data[transferred],
                                    length - transferred, 0);
#endif
        if (result < 0) {
            Com_Printf("NET_OpenSocks: send: %s\n", NET_ErrorString());
            return qfalse;
        }
        if (result == 0) {
            Com_Printf("NET_OpenSocks: connection closed during send\n");
            return qfalse;
        }
        transferred += (size_t)result;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
static qboolean coduomp_net_socks_receive_exact(uint8_t *data, size_t length)
{
    size_t transferred = 0;

    while (transferred < length) {
#if defined(_WIN32)
        const int32_t result = recv(netSocksSocket,
                                    (char *)&data[transferred],
                                    (int32_t)(length - transferred), 0);
#else
        const ssize_t result = recv(netSocksSocket, &data[transferred],
                                    length - transferred, 0);
#endif
        if (result < 0) {
            Com_Printf("NET_OpenSocks: recv: %s\n", NET_ErrorString());
            return qfalse;
        }
        if (result == 0) {
            Com_Printf("NET_OpenSocks: connection closed during recv\n");
            return qfalse;
        }
        transferred += (size_t)result;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0046d560..0x0046d986.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d560_0046d987.mcode, Winsock
 * socket/connect/send/recv imports, and the exact embedded SOCKS diagnostics.
 * Role name: established id networking helper NET_OpenSocks. This is a
 * SOCKS5 TCP negotiation for a UDP ASSOCIATE relay, with optional RFC 1929
 * username/password authentication. */
static void NET_OpenSocks(int32_t port)
{
    uint8_t message[NET_SOCKS_CONTROL_BUFFER_BYTES];

    netUsingSocks = qfalse;
    Com_Printf("Opening connection to SOCKS server.\n");

    netSocksSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(_WIN32)
    if (netSocksSocket == INVALID_SOCKET) {
#else
    if (netSocksSocket == -1) {
#endif
        Com_Printf(
            "WARNING: NET_OpenSocks: socket: %s\n",
            NET_ErrorString());
        return;
    }

#if defined(__APPLE__)
    /*
     * NOT_FROM_ORIGINAL_SOURCE: match the original Mac OpenTransport
     * error-return behavior for every send on the native SOCKS control
     * socket as well as the ordinary UDP socket.
     */
    const int32_t noSigPipe = NET_SOCKET_OPTION_ENABLED;
    if (setsockopt(
            netSocksSocket, SOL_SOCKET, SO_NOSIGPIPE,
            &noSigPipe, sizeof(noSigPipe)) == -1) {
        Com_Printf(
            "WARNING: NET_OpenSocks: setsockopt SO_NOSIGPIPE: %s\n",
            NET_ErrorString());
        goto failed;
    }
#endif

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const struct hostent *host =
        gethostbyname(netSocksServer->string);
    if (host == NULL) {
        Com_Printf(
            "WARNING: NET_OpenSocks: gethostbyname: %s\n",
            NET_ErrorString());
        goto failed;
    }
    if (host->h_addrtype != AF_INET) {
        Com_Printf(
            "WARNING: NET_OpenSocks: gethostbyname: "
            "address type was not AF_INET\n");
        goto failed;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    memcpy(&serverAddress.sin_addr, host->h_addr_list[0],
           sizeof(serverAddress.sin_addr));
    serverAddress.sin_port =
        htons((uint16_t)netSocksPort->integer);

    if (connect(
            netSocksSocket,
            (const struct sockaddr *)&serverAddress,
            sizeof(serverAddress)) != 0) {
        Com_Printf(
            "NET_OpenSocks: connect: %s\n",
            NET_ErrorString());
        goto failed;
    }

    const qboolean useAuthentication =
        netSocksUsername->string[0] != '\0' ||
        netSocksPassword->string[0] != '\0';
    message[0] = NET_SOCKS_VERSION;
    int32_t greetingLength;
    if (useAuthentication != qfalse) {
        message[1] = 2;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        message[2] = NET_SOCKS_METHOD_USERNAME_PASSWORD;
        message[3] = NET_SOCKS_METHOD_NO_AUTH;
        greetingLength = NET_SOCKS_GREETING_AUTH_BYTES;
    } else {
        message[1] = 1;
        message[2] = NET_SOCKS_METHOD_NO_AUTH;
        greetingLength = NET_SOCKS_GREETING_NO_AUTH_BYTES;
    }

    if (coduomp_net_socks_send_all(message, (size_t)greetingLength) == qfalse)
        goto failed;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (coduomp_net_socks_receive_exact(message,
                                        NET_SOCKS_METHOD_REPLY_BYTES) == qfalse)
        goto failed;
    if (message[0] != NET_SOCKS_VERSION) {
        Com_Printf("NET_OpenSocks: bad response\n");
        goto failed;
    }
    if (message[1] != NET_SOCKS_METHOD_NO_AUTH &&
        message[1] != NET_SOCKS_METHOD_USERNAME_PASSWORD) {
        Com_Printf("NET_OpenSocks: request denied\n");
        goto failed;
    }

    if (message[1] == NET_SOCKS_METHOD_USERNAME_PASSWORD) {
        const size_t usernameLength =
            strlen(netSocksUsername->string);
        const size_t passwordLength =
            strlen(netSocksPassword->string);
        const size_t credentialCapacity =
            sizeof(message) - NET_SOCKS_AUTH_FIXED_BYTES;

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (usernameLength > UINT8_MAX || passwordLength > UINT8_MAX ||
            usernameLength > credentialCapacity ||
            passwordLength > credentialCapacity - usernameLength) {
            Com_Printf("NET_OpenSocks: authentication credentials are too long\n");
            goto failed;
        }

        message[0] = NET_SOCKS_AUTH_VERSION;
        message[1] = (uint8_t)usernameLength;
        memcpy(&message[2], netSocksUsername->string,
               usernameLength);
        message[2 + usernameLength] =
            (uint8_t)passwordLength;
        memcpy(&message[3 + usernameLength],
               netSocksPassword->string, passwordLength);

        const size_t authRequestLength =
            usernameLength + passwordLength +
            NET_SOCKS_AUTH_FIXED_BYTES;
        if (coduomp_net_socks_send_all(message, authRequestLength) == qfalse)
            goto failed;
        if (coduomp_net_socks_receive_exact(message,
                                            NET_SOCKS_AUTH_REPLY_BYTES) == qfalse)
            goto failed;
        if (message[0] != NET_SOCKS_AUTH_VERSION) {
            Com_Printf("NET_OpenSocks: bad response\n");
            goto failed;
        }
        if (message[1] != NET_SOCKS_REPLY_SUCCEEDED) {
            Com_Printf(
                "NET_OpenSocks: authentication failed\n");
            goto failed;
        }
    }

    message[0] = NET_SOCKS_VERSION;
    message[1] = NET_SOCKS_COMMAND_UDP_ASSOCIATE;
    message[2] = 0;
    message[3] = NET_SOCKS_ADDRESS_IPV4;
    memset(&message[4], 0, sizeof(uint32_t));
    const uint16_t networkPort = htons((uint16_t)port);
    memcpy(&message[8], &networkPort, sizeof(networkPort));

    if (coduomp_net_socks_send_all(message,
                                   NET_SOCKS_UDP_REQUEST_BYTES) == qfalse)
        goto failed;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (coduomp_net_socks_receive_exact(message,
                                        NET_SOCKS_REPLY_HEADER_BYTES) == qfalse)
        goto failed;
    if (message[0] != NET_SOCKS_VERSION) {
        Com_Printf("NET_OpenSocks: bad response\n");
        goto failed;
    }
    if (message[1] != NET_SOCKS_REPLY_SUCCEEDED) {
        Com_Printf(
            "NET_OpenSocks: request denied: %i\n",
            message[1]);
        goto failed;
    }
    if (message[3] != NET_SOCKS_ADDRESS_IPV4) {
        Com_Printf(
            "NET_OpenSocks: relay address is not IPV4: %i\n",
            message[3]);
        goto failed;
    }
    if (coduomp_net_socks_receive_exact(
            &message[NET_SOCKS_REPLY_HEADER_BYTES],
            NET_SOCKS_IPV4_REPLY_BYTES - NET_SOCKS_REPLY_HEADER_BYTES) == qfalse)
        goto failed;

    memset(&netSocksRelayAddress, 0,
           sizeof(netSocksRelayAddress));
    netSocksRelayAddress.sin_family = AF_INET;
    memcpy(&netSocksRelayAddress.sin_addr,
           &message[4], sizeof(netSocksRelayAddress.sin_addr));
    memcpy(&netSocksRelayAddress.sin_port,
           &message[8], sizeof(netSocksRelayAddress.sin_port));
    netUsingSocks = qtrue;
    return;

failed:
#if defined(_WIN32)
    closesocket(netSocksSocket);
#else
    close(netSocksSocket);
#endif
    netSocksSocket = NET_CLOSED_SOCKET_HANDLE;
}

/* Source: CoDUOMP.exe 0x0046d990..0x0046dae1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046d990_0046dae2.mcode and its
 * gethostname/gethostbyname/ntohl imports.
 * Role name: established id networking helper Sys_GetLocalIP. It prints the
 * canonical host and every alias, then retains at most sixteen IPv4
 * addresses for the first-three-octet LAN comparison. */
static void Sys_GetLocalIP(void)
{
    char hostname[NET_HOSTNAME_BUFFER_SIZE];
    if (gethostname(hostname, sizeof(hostname)) == -1)
        return;

    const struct hostent *host = gethostbyname(hostname);
    if (host == NULL)
        return;

    Com_Printf("Hostname: %s\n", host->h_name);
    for (int32_t aliasIndex = 0;
         host->h_aliases[aliasIndex] != NULL; ++aliasIndex) {
        Com_Printf("Alias: %s\n", host->h_aliases[aliasIndex]);
    }

    if (host->h_addrtype != AF_INET)
        return;

    sysLocalIpAddressCount = 0;
    while (sysLocalIpAddressCount <
               NET_LOCAL_IP_ADDRESS_COUNT &&
           host->h_addr_list[sysLocalIpAddressCount] != NULL) {
        const uint8_t *address = (const uint8_t *)
            host->h_addr_list[sysLocalIpAddressCount];
        memcpy(
            sysLocalIpAddresses[sysLocalIpAddressCount],
            address, NET_IPV4_ADDRESS_BYTES);

        uint32_t networkAddress;
        memcpy(&networkAddress, address, sizeof(networkAddress));
        const uint32_t hostAddress = ntohl(networkAddress);
        Com_Printf(
            "IP: %i.%i.%i.%i\n",
            (int32_t)((hostAddress >> 24) &
                      NET_IPV4_OCTET_MASK),
            (int32_t)((hostAddress >> 16) &
                      NET_IPV4_OCTET_MASK),
            (int32_t)((hostAddress >> 8) &
                      NET_IPV4_OCTET_MASK),
            (int32_t)(hostAddress & NET_IPV4_OCTET_MASK));

        ++sysLocalIpAddressCount;
    }
}

/* Source: CoDUOMP.exe 0x0046daf0..0x0046db9a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046daf0_0046db9b.mcode.
 * Role name: established id networking helper NET_OpenIP. The Windows client
 * tries ten consecutive ports, updates net_port after the first success,
 * optionally opens the configured SOCKS relay, then discovers local IPv4
 * addresses. Unlike the recovered Linux server, exhausting the port range
 * only prints a warning. */
static void NET_OpenIP(void)
{
    cvar_t *const netIp =
        Cvar_Get("net_ip", "localhost", CVAR_LATCH);
    cvar_t *const netPort = Cvar_Get(
        "net_port", va("%i", NET_DEFAULT_PORT), CVAR_LATCH);
    const int32_t basePort = netPort->integer;

    for (int32_t portOffset = 0;
         portOffset < NET_PORT_RETRY_COUNT; ++portOffset) {
        const int32_t port = basePort + portOffset;
        netIpSocket = Sys_OpenIPSocket(netIp->string, port);
        if (netIpSocket == NET_CLOSED_SOCKET_HANDLE)
            continue;

        Cvar_SetValue("net_port", (float)port);
        if (netSocksEnabled->integer != 0)
            NET_OpenSocks(port);
        Sys_GetLocalIP();
        return;
    }

    Com_Printf("WARNING: Couldn't allocate IP port\n");
}

/* Source: CoDUOMP.exe 0x0046dba0..0x0046dcdf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046dba0_0046dce0.mcode and the
 * socket/ioctl/setsockopt/bind imports.
 * Role name: established id networking helper Sys_OpenIPXSocket. The
 * original bind length proves that SOCKADDR_IPX is fourteen bytes even
 * though generic socket-address scratch storage is sixteen bytes. */
static net_socket_handle_t Sys_OpenIPXSocket(int32_t port)
{
    const net_socket_handle_t socketHandle = socket(
        NET_PLATFORM_AF_IPX, SOCK_DGRAM,
        NET_PLATFORM_IPX_PROTOCOL);
    if (socketHandle == NET_INVALID_SOCKET_HANDLE) {
#if defined(_WIN32)
        if (WSAGetLastError() != WSAEAFNOSUPPORT) {
#else
        if (errno != EAFNOSUPPORT) {
#endif
            Com_Printf(
                "WARNING: IPX_Socket: socket: %s\n",
                NET_ErrorString());
        }
        return NET_CLOSED_SOCKET_HANDLE;
    }

#if defined(_WIN32)
    u_long nonblocking = NET_SOCKET_OPTION_ENABLED;
    if (ioctlsocket(socketHandle, FIONBIO, &nonblocking) ==
        SOCKET_ERROR) {
#else
    int32_t nonblocking = NET_SOCKET_OPTION_ENABLED;
    if (ioctl(socketHandle, FIONBIO, &nonblocking) == -1) {
#endif
        Com_Printf(
            "WARNING: IPX_Socket: ioctl FIONBIO: %s\n",
            NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
#if defined(_WIN32)
        closesocket(socketHandle);
#else
        close(socketHandle);
#endif
        return NET_CLOSED_SOCKET_HANDLE;
    }

    const int32_t broadcast = NET_SOCKET_OPTION_ENABLED;
#if defined(_WIN32)
    if (setsockopt(
            socketHandle, SOL_SOCKET, SO_BROADCAST,
            (const char *)&broadcast, sizeof(broadcast)) ==
        SOCKET_ERROR) {
#else
    if (setsockopt(
            socketHandle, SOL_SOCKET, SO_BROADCAST,
            &broadcast, sizeof(broadcast)) == -1) {
#endif
        Com_Printf(
            "WARNING: IPX_Socket: setsockopt SO_BROADCAST: %s\n",
            NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
#if defined(_WIN32)
        closesocket(socketHandle);
#else
        close(socketHandle);
#endif
        return NET_CLOSED_SOCKET_HANDLE;
    }

    net_sockaddr_ipx_t address;
    memset(&address, 0, sizeof(address));
    address.family = NET_PLATFORM_AF_IPX;
    address.socket =
        port == NET_SOCKET_NO_PORT ? 0 : htons((uint16_t)port);

    if (bind(
            socketHandle, (const struct sockaddr *)&address,
            sizeof(address)) != 0) {
        Com_Printf(
            "WARNING: IPX_Socket: bind: %s\n",
            NET_ErrorString());
#if defined(_WIN32)
        closesocket(socketHandle);
#else
        close(socketHandle);
#endif
        return NET_CLOSED_SOCKET_HANDLE;
    }

    return socketHandle;
}

/* Source: CoDUOMP.exe 0x0046dce0..0x0046dd11.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046dce0_0046dd12.mcode.
 * Role name: established id networking helper NET_OpenIPX. */
static void NET_OpenIPX(void)
{
    cvar_t *const netPort = Cvar_Get(
        "net_port", va("%i", NET_DEFAULT_PORT), CVAR_LATCH);
    netIpxSocket = Sys_OpenIPXSocket(netPort->integer);
}

/* Source: CoDUOMP.exe 0x0046dd20..0x0046de68.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046dd20_0046de69.mcode and the seven
 * exact cvar strings/defaults in .rdata.
 * Role name: established id networking helper NET_GetCvars. Its return value
 * reports whether any previously registered network cvar was modified before
 * the pointers are refreshed. */
static qboolean NET_GetCvars(void)
{
    qboolean modified = qfalse;

    if (netNoUdp != NULL && netNoUdp->modified != qfalse)
        modified = qtrue;
    netNoUdp = Cvar_Get(
        "net_noudp", "0", CVAR_ARCHIVE | CVAR_LATCH);

    if (netNoIpx != NULL && netNoIpx->modified != qfalse)
        modified = qtrue;
    netNoIpx = Cvar_Get(
        "net_noipx", "0", CVAR_ARCHIVE | CVAR_LATCH);

    if (netSocksEnabled != NULL &&
        netSocksEnabled->modified != qfalse) {
        modified = qtrue;
    }
    netSocksEnabled = Cvar_Get(
        "net_socksEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH);

    if (netSocksServer != NULL &&
        netSocksServer->modified != qfalse) {
        modified = qtrue;
    }
    netSocksServer = Cvar_Get(
        "net_socksServer", "", CVAR_ARCHIVE | CVAR_LATCH);

    if (netSocksPort != NULL &&
        netSocksPort->modified != qfalse) {
        modified = qtrue;
    }
    netSocksPort = Cvar_Get(
        "net_socksPort", "1080", CVAR_ARCHIVE | CVAR_LATCH);

    if (netSocksUsername != NULL &&
        netSocksUsername->modified != qfalse) {
        modified = qtrue;
    }
    netSocksUsername = Cvar_Get(
        "net_socksUsername", "", CVAR_ARCHIVE | CVAR_LATCH);

    if (netSocksPassword != NULL &&
        netSocksPassword->modified != qfalse) {
        modified = qtrue;
    }
    netSocksPassword = Cvar_Get(
        "net_socksPassword", "", CVAR_ARCHIVE | CVAR_LATCH);

    return modified;
}

/* Source: CoDUOMP.exe 0x0046de70..0x0046df57.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046de70_0046df58.mcode.
 * Role name: established id networking helper NET_Config. Enabling opens
 * only protocols not disabled by their cvars; modified cvars restart an
 * already-enabled network stack, while disabling closes every live socket. */
void NET_Config(qboolean enableNetworking)
{
    const qboolean cvarsModified = NET_GetCvars();
    if (netNoUdp->integer != 0 && netNoIpx->integer != 0)
        enableNetworking = qfalse;

    qboolean closeSockets;
    qboolean openSockets;
    if (enableNetworking == netNetworkingEnabled) {
        if (cvarsModified == qfalse ||
            enableNetworking == qfalse) {
            return;
        }
        closeSockets = qtrue;
        openSockets = qtrue;
    } else {
        netNetworkingEnabled = enableNetworking;
        closeSockets =
            enableNetworking == qfalse ? qtrue : qfalse;
        openSockets =
            enableNetworking != qfalse ? qtrue : qfalse;
    }

    if (closeSockets != qfalse) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        netUsingSocks = qfalse;
        if (netIpSocket != NET_CLOSED_SOCKET_HANDLE &&
            netIpSocket != NET_INVALID_SOCKET_HANDLE) {
#if defined(_WIN32)
            closesocket(netIpSocket);
#else
            close(netIpSocket);
#endif
            netIpSocket = NET_CLOSED_SOCKET_HANDLE;
        }
        if (netSocksSocket != NET_CLOSED_SOCKET_HANDLE &&
            netSocksSocket != NET_INVALID_SOCKET_HANDLE) {
#if defined(_WIN32)
            closesocket(netSocksSocket);
#else
            close(netSocksSocket);
#endif
            netSocksSocket = NET_CLOSED_SOCKET_HANDLE;
        }
        if (netIpxSocket != NET_CLOSED_SOCKET_HANDLE &&
            netIpxSocket != NET_INVALID_SOCKET_HANDLE) {
#if defined(_WIN32)
            closesocket(netIpxSocket);
#else
            close(netIpxSocket);
#endif
            netIpxSocket = NET_CLOSED_SOCKET_HANDLE;
        }
    }

    if (openSockets == qfalse)
        return;
    if (netNoUdp->integer == 0)
        NET_OpenIP();
    if (netNoIpx->integer == 0)
        NET_OpenIPX();
}

/* Source: CoDUOMP.exe 0x0046df60..0x0046dfa7.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046df60_0046dfa8.mcode.
 * Name: exact same-module Mac symbol Sys_InitNetworking. */
void Sys_InitNetworking(void)
{
#if defined(_WIN32)
    const int32_t result = WSAStartup(
        NET_WINSOCK_VERSION_1_1, &netWinsockData);
    if (result != 0) {
        Com_Printf(
            "WARNING: Winsock initialization failed, returned %d\n",
            result);
        return;
    }
#endif

    netWinsockInitialized = qtrue;
    Com_Printf("Winsock Initialized\n");
    (void)NET_GetCvars();
    NET_Config(qtrue);
}

/* Source: CoDUOMP.exe 0x0046dfb0..0x0046dfcf.
 * Evidence: repaired executable-gap function record
 * coduomp/mcode/CoDUOMP/FUN_0046dfb0_0046dfd0.mcode.
 * Name: exact same-module Mac symbol Sys_ShutdownNetworking. */
void Sys_ShutdownNetworking(void)
{
    if (netWinsockInitialized == qfalse)
        return;

    NET_Config(qfalse);
#if defined(_WIN32)
    (void)WSACleanup();
#endif
    netWinsockInitialized = qfalse;
}

/* Source: CoDUOMP.exe 0x0046dfe0..0x0046dfe9; an identical retained body at
 * 0x0046be00..0x0046be09 is the address registered by Sys_Init.
 * Evidence: both executable bodies.
 * Role name: established id networking command NET_Restart_f. */
void NET_Restart_f(void)
{
    NET_Config(netNetworkingEnabled);
}

/* Source: CoDUOMP.exe 0x0046dfd0..0x0046dfd7.
 * Evidence: executable-gap function record
 * coduomp/mcode/CoDUOMP/FUN_0046dfd0_0046dfd8.mcode and the Win32 Sleep
 * import. Name: exact same-module Mac symbol NET_Sleep. MSVC's whole-program
 * register allocation carries the sole argument in EAX in this unused retail
 * wrapper. */
void NET_Sleep(int32_t milliseconds)
{
#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native POSIX replacement for Win32 Sleep. */
    const struct timespec duration = {
        milliseconds / 1000,
        (milliseconds % 1000) * 1000000L
    };
    (void)nanosleep(&duration, NULL);
#endif
}
