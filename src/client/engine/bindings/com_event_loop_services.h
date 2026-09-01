#ifndef CODUOMP_COM_EVENT_LOOP_SERVICES_H
#define CODUOMP_COM_EVENT_LOOP_SERVICES_H

#include "qcommon/msg.h"
#include "qcommon/net_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/system_event_types.h"

#include <stdint.h>

uint32_t Sys_Milliseconds(void);
void CL_KeyEvent(int32_t key, qboolean down, uint32_t time);
void CL_CharEvent(int32_t character);
void CL_MouseEvent(int32_t deltaX, int32_t deltaY);
void CL_JoystickEvent(int32_t axis, int32_t value);
void CL_PacketEvent(netadr_t from, msg_t *message, int32_t time);

#define COM_EVENT_MILLISECONDS() Sys_Milliseconds()
#define COM_EVENT_DISPATCH_KEY(event_) \
    CL_KeyEvent((event_).value, (event_).value2, (uint32_t)(event_).time)
#define COM_EVENT_DISPATCH_CHAR(event_) \
    CL_CharEvent((event_).value)
#define COM_EVENT_DISPATCH_MOUSE(event_) \
    CL_MouseEvent((event_).value, (event_).value2)
#define COM_EVENT_DISPATCH_JOYSTICK(event_) \
    CL_JoystickEvent((event_).value, (event_).value2)
#define COM_EVENT_DISPATCH_CLIENT_PACKET(from_, message_, time_) \
    CL_PacketEvent((from_), &(message_), (time_))
#define COM_EVENT_FREE_PAYLOAD(payload_) free(payload_)

#endif
