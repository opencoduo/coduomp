#ifndef QCOMMON_SYSTEM_EVENT_TYPES_H
#define QCOMMON_SYSTEM_EVENT_TYPES_H

#include <stddef.h>
#include <stdint.h>

enum {
    SYS_EVENT_QUEUE_COUNT = 256
};

#if defined(__cplusplus)
#define SYS_EVENT_TYPES_STATIC_ASSERT static_assert
#else
#define SYS_EVENT_TYPES_STATIC_ASSERT _Static_assert
#endif

typedef enum sysEventType_e {
    SE_NONE = 0,
    SE_KEY = 1,
    SE_CHAR = 2,
    SE_MOUSE = 3,
    SE_JOYSTICK_AXIS = 4,
    SE_CONSOLE = 5,
    SE_PACKET = 6
} sysEventType_t;

/* Native event-queue record. The payload is an owned host pointer and widens
 * with the target; only the original i386 processes use the 24-byte layout. */
typedef struct sysEvent_s {
    int32_t time;
    sysEventType_t type;
    int32_t value;
    int32_t value2;
    int32_t payloadLength;
    void *payload;
} sysEvent_t;

/* NOT_FROM_ORIGINAL_SOURCE: fixed-width compatibility view of the original
 * i386 sysEvent_t journal bytes.  Native pointers widen, but journal.dat must
 * retain the original six-word stream; payload bytes follow separately. */
typedef struct coduo_journal_event_record_s {
    int32_t time;
    int32_t type;
    int32_t value;
    int32_t value2;
    int32_t payloadLength;
    uint32_t payloadAddress;
} coduo_journal_event_record_t;

SYS_EVENT_TYPES_STATIC_ASSERT(sizeof(coduo_journal_event_record_t) == 0x18, "event journal record size changed");

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && UINTPTR_MAX == UINT32_MAX
SYS_EVENT_TYPES_STATIC_ASSERT(sizeof(sysEventType_t) == 4, "sysEventType_t width mismatch");
SYS_EVENT_TYPES_STATIC_ASSERT(sizeof(sysEvent_t) == 0x18, "sysEvent_t size mismatch");
SYS_EVENT_TYPES_STATIC_ASSERT(offsetof(sysEvent_t, type) == 0x04, "sysEvent_t.type offset mismatch");
SYS_EVENT_TYPES_STATIC_ASSERT(offsetof(sysEvent_t, payloadLength) == 0x10, "sysEvent_t.payloadLength offset mismatch");
SYS_EVENT_TYPES_STATIC_ASSERT(offsetof(sysEvent_t, payload) == 0x14, "sysEvent_t.payload offset mismatch");
#endif

#undef SYS_EVENT_TYPES_STATIC_ASSERT

#endif
