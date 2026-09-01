#include "com_event_queue.h"

#include "com_startup_commands.h"
#include "filesystem/filesystem.h"
#include "net_types.h"
#include "q_memory.h"
#include "qcommon_limits.h"
#include "q_shared_types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    COM_JOURNAL_RECORD_BYTES = sizeof(coduo_journal_event_record_t),
    COM_JOURNAL_MODE_RECORD = 1,
    COM_JOURNAL_MODE_REPLAY = 2,
    COM_JOURNAL_MAX_PAYLOAD_BYTES = MAX_MSGLEN + sizeof(netadr_t),
    COM_PUSH_EVENT_QUEUE_MASK = SYS_EVENT_QUEUE_COUNT - 1
};

static sysEvent_t com_pushEvents[SYS_EVENT_QUEUE_COUNT];
static int32_t com_pushEventHead;
static int32_t com_pushEventTail;
static qboolean com_pushEventOverflowed;

sysEvent_t Sys_GetEvent(void);
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);

/* NOT_FROM_ORIGINAL_SOURCE: fixed-width journal/native-event adapter. */
static sysEvent_t coduo_event_from_journal_record(
    const coduo_journal_event_record_t *record)
{
    sysEvent_t event = {
        .time = record->time,
        .type = (sysEventType_t)record->type,
        .value = record->value,
        .value2 = record->value2,
        .payloadLength = record->payloadLength,
        .payload = NULL
    };

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return event;
}

/* NOT_FROM_ORIGINAL_SOURCE: native-event/fixed-width journal adapter. */
static coduo_journal_event_record_t coduo_event_to_journal_record(
    const sysEvent_t *event)
{
    const coduo_journal_event_record_t record = {
        .time = event->time,
        .type = (int32_t)event->type,
        .value = event->value,
        .value2 = event->value2,
        .payloadLength = event->payloadLength,
        .payloadAddress = (uint32_t)(uintptr_t)event->payload
    };

    return record;
}

/*
 * The authoritative event-journal bodies have the same six-dword record and
 * payload stream at CoDUOMP.exe 0x0043a7d0 and coduo_lnxded 0x08070b87.
 * Both retain the original unchecked signed payload length and serialized
 * payload-address behavior documented above.
 */
sysEvent_t Com_GetRealEvent(void)
{
    sysEvent_t event;

    if (com_journal->integer == COM_JOURNAL_MODE_REPLAY) {
        coduo_journal_event_record_t record;

        if (FS_Read(&record, COM_JOURNAL_RECORD_BYTES,
                    com_journalFile) != COM_JOURNAL_RECORD_BYTES) {
            Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_READ");
        }
        event = coduo_event_from_journal_record(&record);

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (event.type < SE_NONE || event.type > SE_PACKET ||
            event.payloadLength < 0 ||
            event.payloadLength > COM_JOURNAL_MAX_PAYLOAD_BYTES ||
            (event.type == SE_CONSOLE && event.payloadLength == 0) ||
            (event.type == SE_PACKET &&
             event.payloadLength < (int32_t)sizeof(netadr_t)) ||
            (event.type != SE_CONSOLE && event.type != SE_PACKET &&
             event.payloadLength != 0)) {
            Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_READ");
        }

        if (event.payloadLength != 0) {
            event.payload = Z_MallocInternal(
                (size_t)(uint32_t)event.payloadLength);
            if (FS_Read(event.payload, event.payloadLength,
                        com_journalFile) != event.payloadLength) {
                Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_READ");
            }
            if (event.type == SE_CONSOLE &&
                ((const char *)event.payload)[event.payloadLength - 1] != '\0') {
                Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_READ");
            }
        }
        return event;
    }

    event = Sys_GetEvent();
    if (com_journal->integer == COM_JOURNAL_MODE_RECORD) {
        const coduo_journal_event_record_t record =
            coduo_event_to_journal_record(&event);

        if (FS_Write(&record, COM_JOURNAL_RECORD_BYTES,
                     com_journalFile) != COM_JOURNAL_RECORD_BYTES) {
            Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_WRITE");
        }
        if (event.payloadLength != 0 &&
            FS_Write(event.payload, event.payloadLength,
                     com_journalFile) != event.payloadLength) {
            Com_Error(ERR_FATAL, "EXE_ERR_JOURNAL_FILE_WRITE");
        }
    }
    return event;
}

/*
 * Original complete-array clears at CoDUOMP.exe 0x0043a940 and
 * coduo_lnxded 0x08070cfe. The Mac client has no distinct symbol for this
 * startup-only operation; the role name distinguishes it from the canonical
 * Com_InitPushEvent queue-drain function below.
 */
void Com_ClearPushEventsForStartup(void)
{
    memset(com_pushEvents, 0, sizeof(com_pushEvents));
    com_pushEventHead = 0;
    com_pushEventTail = 0;
}

/* CoDUOMP.exe 0x0043a960; coduo_lnxded 0x08070d36. */
void Com_PushEvent(const sysEvent_t *event)
{
    sysEvent_t *const slot =
        &com_pushEvents[com_pushEventHead & COM_PUSH_EVENT_QUEUE_MASK];

    if (com_pushEventHead - com_pushEventTail >= SYS_EVENT_QUEUE_COUNT) {
        if (com_pushEventOverflowed == qfalse) {
            Com_Printf("WARNING: Com_PushEvent overflow\n");
            com_pushEventOverflowed = qtrue;
        }
        if (slot->payload != NULL) {
            Z_FreeInternal(slot->payload);
        }
        ++com_pushEventTail;
    } else {
        com_pushEventOverflowed = qfalse;
    }

    *slot = *event;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ++com_pushEventHead;
}

/*
 * The Mac client names this queue-drain operation Com_InitPushEvent. Its
 * Windows and Linux bodies are at 0x0043aa00 and 0x08070de6 respectively.
 */
void Com_InitPushEvent(void)
{
    while (com_pushEventTail < com_pushEventHead) {
        const sysEvent_t event =
            com_pushEvents[com_pushEventTail & COM_PUSH_EVENT_QUEUE_MASK];

        ++com_pushEventTail;
        if (event.payload != NULL) {
            Z_FreeInternal(event.payload);
        }
    }
}

/* CoDUOMP.exe 0x0043aa70; coduo_lnxded 0x08070e61. */
sysEvent_t Com_GetEvent(void)
{
    if (com_pushEventHead > com_pushEventTail) {
        const int32_t eventIndex = com_pushEventTail;

        ++com_pushEventTail;
        return com_pushEvents[eventIndex & COM_PUSH_EVENT_QUEUE_MASK];
    }

    return Com_GetRealEvent();
}

/* CoDUOMP.exe 0x0043aea0; coduo_lnxded 0x0807123e. */
int32_t Com_Milliseconds(void)
{
    for (;;) {
        const sysEvent_t event = Com_GetRealEvent();

        if (event.type == SE_NONE) {
            return event.time;
        }
        Com_PushEvent(&event);
    }
}

/* CoDUOMP.exe 0x0043aef0; coduo_lnxded 0x0807126f. */
void Com_PumpMessageLoop(void)
{
    for (;;) {
        const sysEvent_t event = Com_GetRealEvent();

        if (event.type == SE_NONE) {
            return;
        }
        Com_PushEvent(&event);
    }
}
