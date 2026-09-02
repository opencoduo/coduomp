#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif
#include <sys/time.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "core_runtime_private.h"
#include "compat/coduo_native_x87.h"
#include "../core_cvar/cvar_private.h"
#include "../core_math/core_math_private.h"
#include "../networking/netchan_private.h"

enum {
    SYS_NET_IP_SOCKET_INDEX = 0,
    SYS_NET_IPX_SOCKET_INDEX = 1,
    SYS_NET_SOCKET_COUNT = 2,
    SYS_NET_SOCKADDR_BYTES = 16,
    SYS_NET_LOCAL_IP_LIMIT = 16,
    SYS_NET_HOSTNAME_BUFFER_SIZE = 256,
    SYS_NET_GETHOSTNAME_LIMIT = 256,
    SYS_NET_DEFAULT_PORT = 28960,
    SYS_NET_PORT_RETRY_COUNT = 10,
    SYS_NET_SOCKET_NO_PORT = -1,
    SYS_NET_SOCKET_OPTION_ENABLED = 1,
    SYS_NET_SELECT_USECS_PER_MSEC = 1000,
};

#if defined(_WIN32)
typedef SOCKET coduo_socket_handle_t;
typedef int coduo_socket_length_t;
typedef int coduo_socket_receive_count_t;
#define CODUO_SOCKET_ERROR_RESULT SOCKET_ERROR
#define CODUO_SOCKET_LAST_ERROR() WSAGetLastError()
#define CODUO_SOCKET_WOULD_BLOCK WSAEWOULDBLOCK
#define CODUO_SOCKET_CONNECTION_REFUSED WSAECONNREFUSED
#define CODUO_CLOSE_SOCKET(socketHandle) closesocket((SOCKET)(socketHandle))
#else
typedef int32_t coduo_socket_handle_t;
typedef socklen_t coduo_socket_length_t;
typedef ssize_t coduo_socket_receive_count_t;
#define CODUO_SOCKET_ERROR_RESULT (-1)
#define CODUO_SOCKET_LAST_ERROR() errno
#define CODUO_SOCKET_WOULD_BLOCK EAGAIN
#define CODUO_SOCKET_CONNECTION_REFUSED ECONNREFUSED
#define CODUO_CLOSE_SOCKET(socketHandle) close(socketHandle)
#endif

coduo_socket_handle_t sys_ipSocket;
coduo_socket_handle_t sys_ipxSocket;
static cvar_t *noudp;
static int32_t sys_localIPCount;
static uint8_t sys_localIP[SYS_NET_LOCAL_IP_LIMIT][sizeof(((netadr_t *)0)->ip)];

static int32_t sys_timeBaseSeconds;
static int32_t sys_lastMilliseconds;
#if defined(_WIN32)
static qboolean sys_winsockInitialized;
#endif

coduo_socket_handle_t Sys_OpenIPSocket(const char *netInterface, int32_t port);

void Sys_NetadrToSockaddr(const netadr_t *adr, struct sockaddr_in *sockadr)
{
    memset(sockadr, 0, SYS_NET_SOCKADDR_BYTES);

    if (adr->type == NA_BROADCAST) {
        sockadr->sin_family = AF_INET;
        sockadr->sin_port = adr->port;
        sockadr->sin_addr.s_addr = UINT32_MAX;
    } else if (adr->type == NA_IP) {
        sockadr->sin_family = AF_INET;
        memcpy(&sockadr->sin_addr.s_addr, adr->ip, sizeof(sockadr->sin_addr.s_addr));
        sockadr->sin_port = adr->port;
    }
}

void Sys_SockaddrToNetadr(const struct sockaddr_in *sockadr, netadr_t *adr)
{
    adr->type = NA_IP;
    memcpy(adr->ip, &sockadr->sin_addr.s_addr, sizeof(adr->ip));
    adr->port = sockadr->sin_port;
}

const char *NET_BaseAdrToString(netadr_t adr)
{
    Com_sprintf(com_netadrString, sizeof(com_netadrString), "%i.%i.%i.%i", adr.ip[0], adr.ip[1], adr.ip[2], adr.ip[3]);
    return com_netadrString;
}

qboolean Sys_StringToSockaddr(const char *name, struct sockaddr_in *sockadr)
{
    memset(sockadr, 0, SYS_NET_SOCKADDR_BYTES);
    sockadr->sin_family = AF_INET;
    sockadr->sin_port = 0;

    if (name[0] < '0' || name[0] > '9') {
        struct hostent *host = gethostbyname(name);
        if (host == NULL) {
            return qfalse;
        }

        memcpy(&sockadr->sin_addr, host->h_addr_list[0], sizeof(sockadr->sin_addr));
    } else {
        const uint32_t numericAddress = inet_addr(name);

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (numericAddress == UINT32_MAX) {
            struct addrinfo hints;
            struct addrinfo *numericResult = NULL;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_flags = AI_NUMERICHOST;
            if (getaddrinfo(name, NULL, &hints, &numericResult) != 0 || numericResult == NULL || numericResult->ai_addr == NULL ||
                numericResult->ai_addrlen < sizeof(struct sockaddr_in)) {
                if (numericResult != NULL) {
                    freeaddrinfo(numericResult);
                }
                return qfalse;
            }
            sockadr->sin_addr.s_addr = ((const struct sockaddr_in *)numericResult->ai_addr)->sin_addr.s_addr;
            freeaddrinfo(numericResult);
        } else {
            sockadr->sin_addr.s_addr = numericAddress;
        }
    }

    return qtrue;
}

qboolean Sys_StringToAdr(const char *name, netadr_t *adr)
{
    struct sockaddr_in sockadr;

    if (Sys_StringToSockaddr(name, &sockadr) == qfalse) {
        return qfalse;
    }

    Sys_SockaddrToNetadr(&sockadr, adr);
    return qtrue;
}

qboolean Sys_GetPacket(netadr_t *from, msg_t *msg)
{
    for (int32_t socketIndex = 0; socketIndex < SYS_NET_SOCKET_COUNT; ++socketIndex) {
        coduo_socket_handle_t socket = socketIndex == SYS_NET_IP_SOCKET_INDEX ? sys_ipSocket : sys_ipxSocket;
        if (socket == 0) {
            continue;
        }

        struct sockaddr_in sockadr;
        coduo_socket_length_t sockadrLength = SYS_NET_SOCKADDR_BYTES;
        coduo_socket_receive_count_t received =
            recvfrom(socket, (char *)msg->data, msg->maxsize, 0, (struct sockaddr *)&sockadr, &sockadrLength);
        msg->readcount = 0;

        if (received == CODUO_SOCKET_ERROR_RESULT) {
            int socketError = CODUO_SOCKET_LAST_ERROR();

            if (socketError != CODUO_SOCKET_WOULD_BLOCK && socketError != CODUO_SOCKET_CONNECTION_REFUSED) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                Com_Printf("NET_GetPacket: %s\n", NET_ErrorString());
            }
            continue;
        }

        Sys_SockaddrToNetadr(&sockadr, from);

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (received != msg->maxsize) {
            msg->cursize = (int32_t)received;
            return qtrue;
        }

        Com_Printf("Oversize packet from %s\n", NET_AdrToString(*from));
    }

    return qfalse;
}

void Sys_SendPacket(int32_t length, const void *data, netadr_t to)
{
    coduo_socket_handle_t socket;
    struct sockaddr_in sockadr;

    if (to.type == NA_BROADCAST || to.type == NA_IP) {
        socket = sys_ipSocket;
    } else if (to.type == NA_IPX || to.type == NA_BROADCAST_IPX) {
        socket = sys_ipxSocket;
    } else {
        Com_Error(ERR_FATAL, "NET_SendPacket: bad address type");
        return;
    }

    if (socket == 0) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("Sys_SendPacket: invalid packet length %i\n", length);
        return;
    }

    Sys_NetadrToSockaddr(&to, &sockadr);
    if (sendto(socket, (const char *)data, (size_t)length, 0, (struct sockaddr *)&sockadr, SYS_NET_SOCKADDR_BYTES) ==
        CODUO_SOCKET_ERROR_RESULT) {
        /* Stock formats the address first, then snapshots errno text. */
        const char *addressText = NET_AdrToString(to);
        const char *errorText = NET_ErrorString();

        Com_Printf("NET_SendPacket ERROR: %s to %s\n", errorText, addressText);
    }
}

void Sys_SendPacketByName(const char *address, uint16_t port, const void *data, int32_t length)
{
    netadr_t adr;

    adr.type = NA_IP;
    if (Sys_StringToAdr(address, &adr) != qfalse) {
        adr.port = htons(port);
        Sys_SendPacket(length, data, adr);
    }
}

qboolean Sys_IsLANAddress(netadr_t adr)
{
    if (adr.type == NA_LOOPBACK || adr.type == NA_IPX) {
        return qtrue;
    }

    if (adr.type != NA_IP) {
        return qfalse;
    }

    if (adr.ip[0] == 10 || adr.ip[0] == 127 || (adr.ip[0] == 172 && (adr.ip[1] & 0xf0) == 16) || (adr.ip[0] == 192 && adr.ip[1] == 168)) {
        return qtrue;
    }

    for (int32_t index = 0; index < sys_localIPCount; ++index) {
        if (adr.ip[0] == sys_localIP[index][0] && adr.ip[1] == sys_localIP[index][1] && adr.ip[2] == sys_localIP[index][2]) {
            return qtrue;
        }
    }

    return qfalse;
}

void Sys_ShowIP(void)
{
    for (int32_t index = 0; index < sys_localIPCount; ++index) {
        Com_Printf("IP: %i.%i.%i.%i\n", sys_localIP[index][0], sys_localIP[index][1], sys_localIP[index][2], sys_localIP[index][3]);
    }
}

void Sys_GetLocalIP(void)
{
    char hostname[SYS_NET_HOSTNAME_BUFFER_SIZE];

    if (gethostname(hostname, SYS_NET_GETHOSTNAME_LIMIT) == -1) {
        return;
    }

    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        return;
    }

    Com_Printf("Hostname: %s\n", host->h_name);
    for (int32_t aliasIndex = 0; host->h_aliases[aliasIndex] != NULL; ++aliasIndex) {
        Com_Printf("Alias: %s\n", host->h_aliases[aliasIndex]);
    }

    if (host->h_addrtype != AF_INET) {
        return;
    }

    sys_localIPCount = 0;
    while (1) {
        uint32_t ip;
        char *address;

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        address = host->h_addr_list[sys_localIPCount];
        ++sys_localIPCount;
        if (address == NULL || sys_localIPCount > SYS_NET_LOCAL_IP_LIMIT - 1) {
            break;
        }

        memcpy(&ip, address, sizeof(ip));
        memcpy(sys_localIP[sys_localIPCount], address, sizeof(sys_localIP[sys_localIPCount]));

        uint32_t hostOrderIP = ntohl(ip);
        Com_Printf("IP: %i.%i.%i.%i\n", (int32_t)(hostOrderIP >> 24) & 0xff, (int32_t)(hostOrderIP >> 16) & 0xff,
                   (int32_t)(hostOrderIP >> 8) & 0xff, (int32_t)hostOrderIP & 0xff);
    }
}

void NET_OpenIP(void)
{
    cvar_t *netIP = Cvar_Get("net_ip", "localhost", 0);
    cvar_t *netPort = Cvar_Get("net_port", va("%i", SYS_NET_DEFAULT_PORT), 0);
#if defined(__x86_64__)
    int32_t basePort = CODUO_X87_TRUNCATE_I32((long double)netPort->value);
#else
    int32_t basePort = (int32_t)netPort->value;
#endif

    for (int32_t portOffset = 0; portOffset < SYS_NET_PORT_RETRY_COUNT; ++portOffset) {
        int32_t port = basePort + portOffset;

        sys_ipSocket = Sys_OpenIPSocket(netIP->string, port);
        if (sys_ipSocket != 0) {
            Cvar_SetValue("net_port", (float)port);
            Sys_GetLocalIP();
            return;
        }
    }

    Com_Error(ERR_FATAL, "Couldn't allocate IP port");
}

void NET_Init(void)
{
#if defined(_WIN32)
    if (sys_winsockInitialized == qfalse) {
        WSADATA winsockData;
        int winsockResult = WSAStartup(MAKEWORD(2, 2), &winsockData);

        if (winsockResult != 0) {
            Com_Error(0, "Winsock initialization failed: %d", winsockResult);
        }
        sys_winsockInitialized = qtrue;
    }
#endif
    noudp = Cvar_Get("net_noudp", "0", 0);
    if (noudp->value == 0.0f && sys_ipSocket == 0) {
        NET_OpenIP();
    }
}

coduo_socket_handle_t Sys_OpenIPSocket(const char *netInterface, int32_t port)
{
    struct sockaddr_in address;
    int32_t enabled = SYS_NET_SOCKET_OPTION_ENABLED;
#if defined(_WIN32)
    u_long nonblocking = SYS_NET_SOCKET_OPTION_ENABLED;
#else
    int32_t nonblocking = SYS_NET_SOCKET_OPTION_ENABLED;
#endif

    if (netInterface == NULL) {
        Com_Printf("Opening IP socket: localhost:%i\n", port);
    } else {
        Com_Printf("Opening IP socket: %s:%i\n", netInterface, port);
    }

#if defined(_WIN32)
    SOCKET socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketFd == INVALID_SOCKET) {
#else
    int32_t socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketFd == -1) {
#endif
        Com_Printf("ERROR: UDP_OpenSocket: socket: %s", NET_ErrorString());
        return 0;
    }

#if !defined(_WIN32)
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (socketFd >= FD_SETSIZE) {
        Com_Printf("ERROR: UDP_OpenSocket: descriptor %i exceeds select limit %i\n", socketFd, FD_SETSIZE - 1);
        CODUO_CLOSE_SOCKET(socketFd);
        return 0;
    }
#endif

#if defined(_WIN32)
    if (ioctlsocket(socketFd, FIONBIO, &nonblocking) == SOCKET_ERROR) {
#else
    if (ioctl(socketFd, FIONBIO, &nonblocking) == -1) {
#endif
        Com_Printf("ERROR: UDP_OpenSocket: ioctl FIONBIO:%s\n", NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        CODUO_CLOSE_SOCKET(socketFd);
        return 0;
    }

#if defined(_WIN32)
    if (setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, (const char *)&enabled, sizeof(enabled)) == SOCKET_ERROR) {
#else
    if (setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) == -1) {
#endif
        Com_Printf("ERROR: UDP_OpenSocket: setsockopt SO_BROADCAST:%s\n", NET_ErrorString());
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        CODUO_CLOSE_SOCKET(socketFd);
        return 0;
    }

    if (netInterface == NULL || netInterface[0] == '\0' || Q_stricmp(netInterface, "localhost") == 0) {
        address.sin_addr.s_addr = 0;
        address.sin_family = AF_INET;
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: a named bind interface must resolve
         * successfully; failure closes the owned socket and publishes no bind. */
        if (Sys_StringToSockaddr(netInterface, &address) == qfalse) {
            Com_Printf("ERROR: UDP_OpenSocket: couldn't resolve interface %s\n", netInterface);
            CODUO_CLOSE_SOCKET(socketFd);
            return 0;
        }
    }

    if (port == SYS_NET_SOCKET_NO_PORT) {
        address.sin_port = 0;
    } else {
        address.sin_port = htons((uint16_t)port);
    }
    address.sin_family = AF_INET;

    if (bind(socketFd, (struct sockaddr *)&address, SYS_NET_SOCKADDR_BYTES) == CODUO_SOCKET_ERROR_RESULT) {
        Com_Printf("ERROR: UDP_OpenSocket: bind: %s\n", NET_ErrorString());
        CODUO_CLOSE_SOCKET(socketFd);
        return 0;
    }

    return socketFd;
}

void NET_CloseIP(void)
{
    if (sys_ipSocket != 0) {
        CODUO_CLOSE_SOCKET(sys_ipSocket);
        sys_ipSocket = 0;
    }
}

void NET_Sleep(int32_t msec)
{
    if (sys_ipSocket == 0 || dedicated->integer == 0) {
        return;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
#if !defined(_WIN32)
    if (sys_stdinActive != 0) {
        FD_SET(STDIN_FILENO, &readSet);
    }
#endif
    /* Sys_OpenIPSocket rejects POSIX descriptors outside fd_set's domain. */
    FD_SET(sys_ipSocket, &readSet);

    struct timeval timeout;
    timeout.tv_sec = msec / SYS_NET_SELECT_USECS_PER_MSEC;
    timeout.tv_usec = (msec % SYS_NET_SELECT_USECS_PER_MSEC) * SYS_NET_SELECT_USECS_PER_MSEC;
    select((int)(sys_ipSocket + 1), &readSet, NULL, NULL, &timeout);
}

const char *NET_ErrorString(void)
{
#if defined(_WIN32)
    static char errorText[64];

    snprintf(errorText, sizeof(errorText), "Winsock error %d", WSAGetLastError());
    return errorText;
#else
    return strerror(errno);
#endif
}

int32_t Sys_Milliseconds(void)
{
    struct timeval tv;
    uint8_t tz[8];

    gettimeofday(&tv, tz);
    if (sys_timeBaseSeconds == 0) {
        sys_timeBaseSeconds = (int32_t)tv.tv_sec;
        return (int32_t)(tv.tv_usec / 1000);
    }

    sys_lastMilliseconds = (int32_t)(tv.tv_usec / 1000) + ((int32_t)tv.tv_sec - sys_timeBaseSeconds) * 1000;
    return sys_lastMilliseconds;
}
