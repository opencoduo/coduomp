#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>

#include "core_runtime_private.h"

enum {
    SYS_EVENT_QUEUE_INDEX_MASK = SYS_EVENT_QUEUE_COUNT - 1,
    SYS_EVENT_QUEUE_OVERFLOW_DISTANCE = SYS_EVENT_QUEUE_COUNT - 1,
    SYS_EVENT_CONSOLE = 5,
    SYS_EVENT_PACKET = 6,
    SYS_PACKET_EVENT_ADDRESS_BYTES = sizeof(netadr_t)
};

sysEvent_t sys_eventQueue[SYS_EVENT_QUEUE_COUNT];
int32_t sys_eventQueueProducer;
int32_t sys_eventQueueConsumer;

uint8_t sys_packetBuffer[MAX_MSGLEN];

/* NOT_FROM_ORIGINAL_SOURCE: structured local form of the repeated queue pop. */
static qboolean coduomp_sys_pop_queued_event(sysEvent_t *event)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (sys_eventQueueConsumer >= sys_eventQueueProducer) {
        return qfalse;
    }

    int32_t eventIndex =
        sys_eventQueueConsumer & SYS_EVENT_QUEUE_INDEX_MASK;
    ++sys_eventQueueConsumer;
    *event = sys_eventQueue[eventIndex];
    return qtrue;
}

int32_t Sys_Stat(const char *path, struct stat *statbuf)
{
    return stat(path, statbuf);
}

int32_t Sys_FileTime(const char *path)
{
    struct stat statbuf;

    if (Sys_Stat(path, &statbuf) == -1) {
        return -1;
    }

    return (int32_t)statbuf.st_mtime;
}

void Sys_FPEHandler(int32_t signalNumber)
{
    (void)signalNumber;
    signal(SIGFPE, Sys_FPEHandler);
}

/*
 * Checked 2026-06-29: FUN_080c9015 is an empty platform hook adjacent to
 * named Sys functions; no source-level name is proven.
 */
void FUN_080c9015(void)
{
}

void Sys_InitStreamThread(void)
{
}

void Sys_ShutdownStreamThread(void)
{
}

void Sys_QueEvent(int32_t time,
                  int32_t type,
                  int32_t value,
                  int32_t value2,
                  int32_t payloadLength,
                  void *payload)
{
    int32_t eventIndex =
        sys_eventQueueProducer & SYS_EVENT_QUEUE_INDEX_MASK;
    sysEvent_t *event = &sys_eventQueue[eventIndex];

    if (SYS_EVENT_QUEUE_OVERFLOW_DISTANCE <
        sys_eventQueueProducer - sys_eventQueueConsumer) {
        Com_Printf("Sys_QueEvent: overflow\n");
        if (event->payload != NULL) {
            Z_FreeInternal(event->payload);
        }
        sys_eventQueueConsumer++;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    sys_eventQueueProducer++;
    if (time == 0) {
        time = Sys_Milliseconds();
    }

    event->time = time;
    event->type = type;
    event->value = value;
    event->value2 = value2;
    event->payloadLength = payloadLength;
    event->payload = payload;
}

sysEvent_t Sys_GetEvent(void)
{
    sysEvent_t event;

    if (coduomp_sys_pop_queued_event(&event) != qfalse) {
        return event;
    }

    Sys_SendKeyEvents();
    char *consoleInput = Sys_ConsoleInput();
    if (consoleInput != NULL) {
        size_t inputLength = strlen(consoleInput);
        char *payload = Z_MallocInternal(inputLength + 1);

        strcpy(payload, consoleInput);
        Sys_QueEvent(0, SYS_EVENT_CONSOLE, 0, 0,
                     (int32_t)(inputLength + 1),
                     payload);
    }

    Sys_Input();
    msg_t msg;
    netadr_t from;

    MSG_Init(&msg, sys_packetBuffer, MAX_MSGLEN);
    if (Sys_GetPacket(&from, &msg) != qfalse) {
        int32_t payloadLength =
            SYS_PACKET_EVENT_ADDRESS_BYTES + msg.cursize;
        uint8_t *payload = Z_MallocInternal(payloadLength);

        memcpy(payload, &from, SYS_PACKET_EVENT_ADDRESS_BYTES);
        memcpy(&payload[SYS_PACKET_EVENT_ADDRESS_BYTES], msg.data,
               (size_t)msg.cursize);
        Sys_QueEvent(0, SYS_EVENT_PACKET, 0, 0, payloadLength,
                     payload);
    }

    if (coduomp_sys_pop_queued_event(&event) != qfalse) {
        return event;
    }

    memset(&event, 0, sizeof(event));
    event.time = Sys_Milliseconds();
    return event;
}
