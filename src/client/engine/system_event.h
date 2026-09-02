#ifndef CODUOMP_SYSTEM_EVENT_H
#define CODUOMP_SYSTEM_EVENT_H

#include "q_shared.h"
#include "networking/net_address.h"
#include "qcommon/system_event_types.h"

/* Timestamp assigned by the Win32 message pump before dispatching a message. */
extern int32_t sysMsgTime; /* original 0x0489bc2c */

void Sys_QueEvent(int32_t time, sysEventType_t type, int32_t value, int32_t value2, int32_t payloadLength, void *payload);
void Sys_ClearEventQueue(void);
void Sys_PumpEvents(void);
sysEvent_t Sys_GetEvent(void);

#endif
