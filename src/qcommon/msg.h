#ifndef QCOMMON_MSG_H
#define QCOMMON_MSG_H

#include <stddef.h>
#include <stdint.h>

#include "q_shared_types.h"

/*
 * Native message cursor. The data pointer widens with the host; the remaining
 * scalar fields preserve the original message-reader semantics.
 *
 * Linux evidence:
 * - MSG_Init at 0x0808012c clears 0x18 bytes, writes data at +0x04 and maxsize
 *   at +0x08.
 * - MSG_BeginReading at 0x0808016f clears readcount +0x10 and bit +0x14.
 * Windows MSG_Init at 0x004495a0 and its readers establish the same fields,
 * widths, and i386 layout.
 */
typedef struct msg_s {
    qboolean overflowed;
    uint8_t *data;
    int32_t maxsize;
    int32_t cursize;
    int32_t readcount;
    int32_t bit;
} msg_t;

#ifdef __cplusplus
extern "C" {
#endif

void MSG_Init(msg_t *message, uint8_t *data, int32_t maxsize);
void MSG_BeginReading(msg_t *message);
void MSG_WriteBits(msg_t *message, int32_t value, int32_t bitCount);
void MSG_WriteBit0(msg_t *message);
void MSG_WriteBit1(msg_t *message);
int32_t MSG_ReadBits(msg_t *message, int32_t bitCount);
int32_t MSG_ReadBit(msg_t *message);
void MSG_WriteByte(msg_t *message, int32_t value);
void MSG_WriteData(msg_t *message, const void *data, int32_t length);
void MSG_WriteShort(msg_t *message, int32_t value);
void MSG_WriteLong(msg_t *message, int32_t value);
void MSG_WriteString(msg_t *message, const char *string);
void MSG_WriteBigString(msg_t *message, const char *string);
void MSG_WriteAngle(msg_t *message, float angle);
void MSG_WriteAngle16(msg_t *message, float angle);
int32_t MSG_ReadByte(msg_t *message);
int32_t MSG_ReadShort(msg_t *message);
int32_t MSG_ReadLong(msg_t *message);
char *MSG_ReadString(msg_t *message);
char *MSG_ReadBigString(msg_t *message);
char *MSG_ReadStringLine(msg_t *message);
float MSG_ReadAngle16(msg_t *message);
void MSG_ReadData(msg_t *message, void *data, int32_t length);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
#define MSG_STATIC_ASSERT static_assert
#else
#define MSG_STATIC_ASSERT _Static_assert
#endif

#if UINTPTR_MAX == UINT32_MAX
MSG_STATIC_ASSERT(sizeof(msg_t) == 0x18, "i386 msg_t size changed");
MSG_STATIC_ASSERT(offsetof(msg_t, overflowed) == 0x00,
                  "i386 msg_t overflow flag moved");
MSG_STATIC_ASSERT(offsetof(msg_t, data) == 0x04,
                  "i386 msg_t data pointer moved");
MSG_STATIC_ASSERT(offsetof(msg_t, maxsize) == 0x08,
                  "i386 msg_t maximum size moved");
MSG_STATIC_ASSERT(offsetof(msg_t, cursize) == 0x0c,
                  "i386 msg_t current size moved");
MSG_STATIC_ASSERT(offsetof(msg_t, readcount) == 0x10,
                  "i386 msg_t read cursor moved");
MSG_STATIC_ASSERT(offsetof(msg_t, bit) == 0x14,
                  "i386 msg_t bit cursor moved");
#endif

#undef MSG_STATIC_ASSERT

#endif
