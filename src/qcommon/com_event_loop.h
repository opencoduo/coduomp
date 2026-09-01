#ifndef QCOMMON_COM_EVENT_LOOP_H
#define QCOMMON_COM_EVENT_LOOP_H

#include "msg.h"
#include "net_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Com_RunAndTimeServerPacket(const netadr_t *from, msg_t *message);
int32_t Com_EventLoop(void);

#ifdef __cplusplus
}
#endif

#endif
