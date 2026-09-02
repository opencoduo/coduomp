#include "com_event_loop.h"

#include "com_event_loop_services.h"

#include "com_event_queue.h"
#include "net_loopback.h"
#include "q_command.h"
#include "qcommon_limits.h"
#include "qcommon_runtime_types.h"
#include "server/engine/server_packet.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern cvar_t *com_speeds;
extern cvar_t *sv_running;

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);

/*
 * The original Windows client and Linux dedicated engine implement the same
 * packet timing operation:
 *
 *   CoDUOMP.exe   0x0043ab00
 *   coduo_lnxded  0x08070eee
 *
 * Both sample the wrapping millisecond clock only while com_speeds is enabled,
 * execute SV_PacketEvent, then print the elapsed dword only for mode 3.
 */
void Com_RunAndTimeServerPacket(const netadr_t *from, msg_t *message)
{
    uint32_t startTime = 0;

    if (com_speeds->integer != 0) {
        startTime = COM_EVENT_MILLISECONDS();
    }

    SV_PacketEvent(*from, message);

    if (com_speeds->integer != 0) {
        const uint32_t elapsed = COM_EVENT_MILLISECONDS() - startTime;

        if (com_speeds->integer == 3) {
            Com_Printf("SV_PacketEvent time: %i\n", (int32_t)elapsed);
        }
    }
}

/*
 * Complete event dispatcher:
 *
 *   CoDUOMP.exe   0x0043abc0..0x0043ae81
 *   coduo_lnxded  0x08070f7c..0x0807123d
 *
 * The event cases, packet bounds test, server/client selection, loopback
 * drains, and returned timestamp agree. The target service macros preserve
 * only two build edges from the retained bodies: the dedicated null-client
 * mouse/joystick hooks receive the event time, and event payloads are released
 * through the allocator that created them on that target. They expand at the
 * call site and introduce no runtime adapter.
 */
int32_t Com_EventLoop(void)
{
    uint8_t messageBuffer[MAX_MSGLEN];
    msg_t message;
    netadr_t from;
    sysEvent_t event;

    memset(messageBuffer, 0, sizeof(messageBuffer));
    MSG_Init(&message, messageBuffer, (int32_t)sizeof(messageBuffer));

    for (;;) {
        event = Com_GetEvent();

        switch (event.type) {
        case SE_NONE:
            break;

        case SE_KEY:
            COM_EVENT_DISPATCH_KEY(event);
            continue;

        case SE_CHAR:
            COM_EVENT_DISPATCH_CHAR(event);
            continue;

        case SE_MOUSE:
            COM_EVENT_DISPATCH_MOUSE(event);
            continue;

        case SE_JOYSTICK_AXIS:
            COM_EVENT_DISPATCH_JOYSTICK(event);
            continue;

        case SE_CONSOLE:
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (event.payload == NULL || event.payloadLength <= 0 || memchr(event.payload, '\0', (size_t)event.payloadLength) == NULL) {
                if (event.payload != NULL) {
                    COM_EVENT_FREE_PAYLOAD(event.payload);
                }
                Com_Printf("Com_EventLoop: invalid console event\n");
                continue;
            }
            Cbuf_AddText((const char *)event.payload);
            COM_EVENT_FREE_PAYLOAD(event.payload);
            Cbuf_AddText("\n");
            continue;

        case SE_PACKET: {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (event.payload == NULL || event.payloadLength < (int32_t)sizeof(netadr_t)) {
                if (event.payload != NULL) {
                    COM_EVENT_FREE_PAYLOAD(event.payload);
                }
                Com_Printf("Com_EventLoop: invalid packet event\n");
                continue;
            }

            const uint32_t packetLength = (uint32_t)(event.payloadLength - (int32_t)sizeof(netadr_t));
            memcpy(&from, event.payload, sizeof(from));
            if (packetLength > (uint32_t)message.maxsize) {
                COM_EVENT_FREE_PAYLOAD(event.payload);
                Com_Printf("Com_EventLoop: oversize packet\n");
                continue;
            }

            memcpy(message.data, (const uint8_t *)event.payload + sizeof(netadr_t), (size_t)packetLength);
            message.cursize = (int32_t)packetLength;
            COM_EVENT_FREE_PAYLOAD(event.payload);

            if (sv_running->integer != 0) {
                Com_RunAndTimeServerPacket(&from, &message);
            } else {
                COM_EVENT_DISPATCH_CLIENT_PACKET(from, message, event.time);
            }
            continue;
        }

        default:
            Com_Error(ERR_FATAL,
                      "\x15"
                      "Com_EventLoop: bad event type %i",
                      event.type);
            continue;
        }

        break;
    }

    while (NET_GetLoopPacket(NS_CLIENT, &from, &message) != qfalse) {
        COM_EVENT_DISPATCH_CLIENT_PACKET(from, message, event.time);
    }

    while (NET_GetLoopPacket(NS_SERVER, &from, &message) != qfalse) {
        if (sv_running->integer != 0) {
            Com_RunAndTimeServerPacket(&from, &message);
        }
    }

    return event.time;
}
