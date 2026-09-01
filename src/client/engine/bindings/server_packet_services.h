#ifndef CODUOMP_SERVER_PACKET_SERVICES_H
#define CODUOMP_SERVER_PACKET_SERVICES_H

#include "qcommon/qcommon_runtime_types.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>

extern cvar_t *dedicated;

void server_compat_handle_pb_packet(netadr_t from, msg_t *message);
void PB_InvokeEventCallback(const char *address,
                            const uint8_t *packetData);
void PB_CallServerSaCommandDrain(void);

/* NOT_FROM_ORIGINAL_SOURCE: isolate the authoritative Windows server-browser
 * hardware value from the common query formatter. */
static inline int32_t server_compat_info_hardware(void)
{
    return dedicated != NULL && dedicated->integer != 0 ? 2 : 4;
}

#endif
