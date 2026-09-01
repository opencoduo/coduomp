#ifndef CODUO_NETCHAN_PRIVATE_H
#define CODUO_NETCHAN_PRIVATE_H

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "qcommon/net_compare.h"
#include "qcommon/net_loopback.h"
#include "qcommon/net_text.h"
#include "qcommon/netchan.h"
#include "server/engine/server_netchan.h"

extern serverStatic_t svs;
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
qboolean NET_CompareAdr(netadr_t a, netadr_t b);
qboolean NET_IsLocalAddress(netadr_t adr);
#endif
