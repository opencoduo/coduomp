#ifndef CODUO_SV_PUNKBUSTER_BRIDGE_PRIVATE_H
#define CODUO_SV_PUNKBUSTER_BRIDGE_PRIVATE_H

#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "server/engine/server_client_message.h"
#include "server/engine/server_punkbuster_queries.h"

void SV_SetPunkBusterCvar(const char *value);
void SV_PrintPunkBusterMessage(const char *prefix, const char *text);

#endif
