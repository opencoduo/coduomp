#ifndef CODUO_SERVER_PACKET_SERVICES_H
#define CODUO_SERVER_PACKET_SERVICES_H

#include "qcommon/qcommon_runtime_types.h"
#include "qcommon/server_runtime_types.h"

#include <stdint.h>

void server_compat_handle_pb_packet(netadr_t from, msg_t *message);
void PB_InvokeEventCallback(const char *address, const uint8_t *packetData);
void PB_CallServerSaCommandDrain(void);

/* NOT_FROM_ORIGINAL_SOURCE: isolate the authoritative Linux dedicated
 * server-browser hardware value from the common query formatter. */
static inline int32_t server_compat_info_hardware(void)
{
    return 1;
}

#endif
