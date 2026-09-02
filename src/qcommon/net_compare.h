#ifndef QCOMMON_NET_COMPARE_H
#define QCOMMON_NET_COMPARE_H

#include "net_types.h"

#include <stdint.h>

int32_t NET_CompareBaseAdrSigned(const netadr_t *left, const netadr_t *right);
qboolean NET_CompareBaseAdr(netadr_t left, netadr_t right);
int32_t NET_CompareAdrSigned(const netadr_t *left, const netadr_t *right);
qboolean NET_CompareAdr(netadr_t left, netadr_t right);
qboolean NET_IsLocalAddress(netadr_t address);

#endif
