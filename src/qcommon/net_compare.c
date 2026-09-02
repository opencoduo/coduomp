#include "net_compare.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

void Com_Printf(const char *format, ...);

/*
 * Complete network-address comparison subsystem.
 *
 * Windows CoDUOMP.exe 0x0044dd50 and 0x0044dec0 use the MSVC intrinsic
 * REPZ-CMPS realization for the address bytes and reduce the first mismatch
 * to exactly -1 or +1.  Linux coduo_lnxded 0x08084802 and 0x080849fe call
 * memcmp for the same four- and ten-byte spans, retaining its signed result.
 * The type, port, unsupported-type, boolean-wrapper, and local-address paths
 * otherwise agree.
 */
int32_t NET_CompareBaseAdrSigned(const netadr_t *left, const netadr_t *right)
{
    if (left->type != right->type) {
        return left->type - right->type;
    }
    if (left->type == NA_LOOPBACK) {
        return 0;
    }
    if (left->type == NA_BAD) {
        return (int32_t)left->port - (int32_t)right->port;
    }
    if (left->type == NA_IP) {
#if defined(WINDOWS_BEHAVIOR)
        size_t index;
        for (index = 0; index < sizeof(left->ip); ++index) {
            if (left->ip[index] < right->ip[index]) {
                return -1;
            }
            if (left->ip[index] > right->ip[index]) {
                return 1;
            }
        }
        return 0;
#else
        return memcmp(left->ip, right->ip, sizeof(left->ip));
#endif
    }
    if (left->type == NA_IPX) {
#if defined(WINDOWS_BEHAVIOR)
        size_t index;
        for (index = 0; index < sizeof(left->ipx); ++index) {
            if (left->ipx[index] < right->ipx[index]) {
                return -1;
            }
            if (left->ipx[index] > right->ipx[index]) {
                return 1;
            }
        }
        return 0;
#else
        return memcmp(left->ipx, right->ipx, sizeof(left->ipx));
#endif
    }

    Com_Printf("NET_CompareBaseAdrSigned: bad address type\n");
    return 0;
}

qboolean NET_CompareBaseAdr(netadr_t left, netadr_t right)
{
    return NET_CompareBaseAdrSigned(&left, &right) == 0 ? qtrue : qfalse;
}

int32_t NET_CompareAdrSigned(const netadr_t *left, const netadr_t *right)
{
    if (left->type != right->type) {
        return left->type - right->type;
    }
    if (left->type == NA_LOOPBACK) {
        return 0;
    }
    if (left->type == NA_IP) {
        if (left->port != right->port) {
            return (int32_t)left->port - (int32_t)right->port;
        }
#if defined(WINDOWS_BEHAVIOR)
        size_t index;
        for (index = 0; index < sizeof(left->ip); ++index) {
            if (left->ip[index] < right->ip[index]) {
                return -1;
            }
            if (left->ip[index] > right->ip[index]) {
                return 1;
            }
        }
        return 0;
#else
        return memcmp(left->ip, right->ip, sizeof(left->ip));
#endif
    }
    if (left->type == NA_IPX) {
        if (left->port != right->port) {
            return (int32_t)left->port - (int32_t)right->port;
        }
#if defined(WINDOWS_BEHAVIOR)
        size_t index;
        for (index = 0; index < sizeof(left->ipx); ++index) {
            if (left->ipx[index] < right->ipx[index]) {
                return -1;
            }
            if (left->ipx[index] > right->ipx[index]) {
                return 1;
            }
        }
        return 0;
#else
        return memcmp(left->ipx, right->ipx, sizeof(left->ipx));
#endif
    }

    Com_Printf("NET_CompareAdrSigned: bad address type\n");
    return 0;
}

qboolean NET_CompareAdr(netadr_t left, netadr_t right)
{
    return NET_CompareAdrSigned(&left, &right) == 0 ? qtrue : qfalse;
}

qboolean NET_IsLocalAddress(netadr_t address)
{
    return address.type == NA_LOOPBACK || address.type == NA_BAD ? qtrue : qfalse;
}
