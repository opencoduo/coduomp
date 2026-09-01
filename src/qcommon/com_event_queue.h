#ifndef QCOMMON_COM_EVENT_QUEUE_H
#define QCOMMON_COM_EVENT_QUEUE_H

#include "system_event_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

sysEvent_t Com_GetRealEvent(void);
sysEvent_t Com_GetEvent(void);
void Com_PushEvent(const sysEvent_t *event);
void Com_InitPushEvent(void);
void Com_ClearPushEventsForStartup(void);
int32_t Com_Milliseconds(void);
void Com_PumpMessageLoop(void);

#ifdef __cplusplus
}
#endif

#endif
