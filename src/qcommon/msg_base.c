#include "msg.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87emu.h"
#include "huffman.h"
#include "q_string.h"
#include "qcommon_limits.h"

#include <stdint.h>
#include <string.h>

enum {
    MSG_WRITE_BITS_REQUIRED_SPACE = 4,
    MSG_WORD_BIT_INDEX_MASK = 31,
    MSG_BYTE_MASK = 255,
    MSG_SHORT_MASK = 65535,
    MSG_READ_OVERFLOW_VALUE = -1
};

#define MSG_ANGLE_TO_BYTE_SCALE 0.7111111283302307f
#define MSG_ANGLE_TO_SHORT_SCALE 182.04444885253906f
#define MSG_ANGLE16_FROM_SHORT_SCALE 0.0054931640625f

void Com_Printf(const char *format, ...);

/*
 * Complete base message cursor and primitive-codec subsystem.
 *
 * CoDUOMP.exe 0x004495a0..0x00449dd0 and coduo_lnxded
 * 0x0808012c..0x08080b14 agree on the msg_t state transitions, wire byte
 * order, capacity decisions, string limits and cleaning, and the three
 * independent static reader buffers. Compiler inlining and calling-convention
 * differences do not alter those results. The angle writers retain the only
 * observable arithmetic/conversion differences as complete platform bodies.
 */
static char msg_readString[MAX_STRING_CHARS];
static char msg_readBigString[BIG_INFO_STRING];
static char msg_readStringLine[MAX_STRING_CHARS];

void MSG_Init(msg_t *message, uint8_t *data, int32_t maxsize)
{
    if (msgInit == qfalse) {
        MSG_initHuffman();
    }

    memset(message, 0, sizeof(*message));
    message->data = data;
    message->maxsize = maxsize;
}

void MSG_BeginReading(msg_t *message)
{
    message->readcount = 0;
    message->bit = 0;
}

void MSG_WriteBits(msg_t *message, int32_t value, int32_t bitCount)
{
    uint32_t valueBits = (uint32_t)value;
    const int32_t remainingBytes = (int32_t)((uint32_t)message->maxsize - (uint32_t)message->cursize);

    if (remainingBytes < MSG_WRITE_BITS_REQUIRED_SPACE) {
        message->overflowed = qtrue;
        return;
    }

    while (bitCount != 0) {
        const int32_t bitInByte = message->bit & 7;

        if (bitInByte == 0) {
            message->bit = (int32_t)((uint32_t)message->cursize << 3U);
            message->data[message->cursize] = 0;
            ++message->cursize;
        }

        if ((valueBits & UINT32_C(1)) != 0) {
            message->data[message->bit >> 3] |= (uint8_t)(UINT32_C(1) << bitInByte);
        }
        ++message->bit;
        /* Both i386 bodies use SAR. Preserve its sign fill without depending
         * on implementation-defined right shift of a negative C value. */
        valueBits = (valueBits >> 1U) | (valueBits & UINT32_C(0x80000000));
        --bitCount;
    }
}

void MSG_WriteBit0(msg_t *message)
{
    if (message->cursize >= message->maxsize) {
        message->overflowed = qtrue;
        return;
    }

    if ((message->bit & 7) == 0) {
        message->bit = (int32_t)((uint32_t)message->cursize << 3U);
        message->data[message->cursize] = 0;
        ++message->cursize;
    }
    ++message->bit;
}

void MSG_WriteBit1(msg_t *message)
{
    int32_t bitInByte;

    if (message->cursize >= message->maxsize) {
        message->overflowed = qtrue;
        return;
    }

    bitInByte = message->bit & 7;
    if (bitInByte == 0) {
        message->bit = (int32_t)((uint32_t)message->cursize << 3U);
        message->data[message->cursize] = 0;
        ++message->cursize;
    }
    message->data[message->bit >> 3] |= (uint8_t)(UINT32_C(1) << bitInByte);
    ++message->bit;
}

int32_t MSG_ReadBits(msg_t *message, int32_t bitCount)
{
    uint32_t valueBits = 0;

    for (int32_t outputBit = 0; outputBit < bitCount; ++outputBit) {
        const int32_t bitInByte = message->bit & 7;

        if (bitInByte == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: crossing the declared message extent
             * publishes the established overflow cursor and no fabricated
             * bits. */
            if (message->readcount < 0 || message->readcount >= message->cursize) {
                message->readcount = message->cursize + 1;
                return 0;
            }
            message->bit = (int32_t)((uint32_t)message->readcount << 3U);
            ++message->readcount;
        }

        if (message->bit < 0 || (message->bit >> 3) >= message->cursize) {
            message->readcount = message->cursize + 1;
            return 0;
        }
        valueBits |= (uint32_t)((message->data[message->bit >> 3] >> bitInByte) & 1) << ((uint32_t)outputBit & MSG_WORD_BIT_INDEX_MASK);
        ++message->bit;
    }

    return (int32_t)valueBits;
}

int32_t MSG_ReadBit(msg_t *message)
{
    const int32_t bitInByte = message->bit & 7;
    int32_t bit;

    if (bitInByte == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: an unavailable source byte publishes the
         * established overflow cursor used by callers. */
        if (message->readcount < 0 || message->readcount >= message->cursize) {
            message->readcount = message->cursize + 1;
            return 0;
        }
        message->bit = (int32_t)((uint32_t)message->readcount << 3U);
        ++message->readcount;
    }

    if (message->bit < 0 || (message->bit >> 3) >= message->cursize) {
        message->readcount = message->cursize + 1;
        return 0;
    }
    bit = (message->data[message->bit >> 3] >> bitInByte) & 1;
    ++message->bit;
    return bit;
}

void MSG_WriteByte(msg_t *message, int32_t value)
{
    if (message->cursize < message->maxsize) {
        message->data[message->cursize++] = (uint8_t)value;
    } else {
        message->overflowed = qtrue;
    }
}

void MSG_WriteData(msg_t *message, const void *data, int32_t length)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0 || message->cursize < 0 || message->cursize > message->maxsize || length > message->maxsize - message->cursize) {
        message->overflowed = qtrue;
        return;
    }

    if (length > 0)
        memcpy(message->data + message->cursize, data, (size_t)length);
    message->cursize += length;
}

void MSG_WriteShort(msg_t *message, int32_t value)
{
    const int32_t end = (int32_t)((uint32_t)message->cursize + 2U);
    const uint16_t stored = (uint16_t)value;

    if (end > message->maxsize) {
        message->overflowed = qtrue;
        return;
    }

    message->data[message->cursize] = (uint8_t)stored;
    message->data[message->cursize + 1] = (uint8_t)(stored >> 8U);
    message->cursize = end;
}

void MSG_WriteLong(msg_t *message, int32_t value)
{
    const int32_t end = (int32_t)((uint32_t)message->cursize + 4U);
    const uint32_t stored = (uint32_t)value;

    if (end > message->maxsize) {
        message->overflowed = qtrue;
        return;
    }

    message->data[message->cursize] = (uint8_t)stored;
    message->data[message->cursize + 1] = (uint8_t)(stored >> 8U);
    message->data[message->cursize + 2] = (uint8_t)(stored >> 16U);
    message->data[message->cursize + 3] = (uint8_t)(stored >> 24U);
    message->cursize = end;
}

void MSG_WriteString(msg_t *message, const char *string)
{
    char sanitized[MAX_STRING_CHARS];
    int32_t length;

    if (string == NULL) {
        MSG_WriteData(message, "", 1);
        return;
    }

    length = (int32_t)(uint32_t)strlen(string);
    if (length >= MAX_STRING_CHARS) {
        Com_Printf("MSG_WriteString: MAX_STRING_CHARS");
        MSG_WriteData(message, "", 1);
        return;
    }

    for (int32_t index = 0; index < length; ++index) {
        sanitized[index] = (char)Q_CleanCharacter((uint8_t)string[index]);
    }
    sanitized[length] = '\0';
    MSG_WriteData(message, sanitized, length + 1);
}

void MSG_WriteBigString(msg_t *message, const char *string)
{
    char sanitized[BIG_INFO_STRING];
    int32_t length;

    if (string == NULL) {
        MSG_WriteData(message, "", 1);
        return;
    }

    length = (int32_t)(uint32_t)strlen(string);
    if (length >= BIG_INFO_STRING) {
        Com_Printf("MSG_WriteString: BIG_INFO_STRING");
        MSG_WriteData(message, "", 1);
        return;
    }

    Q_strncpyz(sanitized, string, BIG_INFO_STRING);
    for (int32_t index = 0; index < length; ++index) {
        sanitized[index] = (char)Q_CleanCharacter((uint8_t)sanitized[index]);
    }
    MSG_WriteData(message, sanitized, length + 1);
}

#if defined(WINDOWS_BEHAVIOR)
void MSG_WriteAngle(msg_t *message, float angle)
{
    int32_t packed;

    /* CoDUOMP.exe 0x00449b50 checks capacity before FLD angle, multiplies by
     * the pre-rounded binary32 reciprocal 0x3f360b61, and consumes the low
     * byte of `_ftol2`'s signed-qword result. */
    if (message->cursize >= message->maxsize) {
        message->overflowed = qtrue;
        return;
    }

#if EMULATE_X87
    packed = (int32_t)(uint32_t)x87f_store_i64_trunc(x87f_mul(x87f_load_f32(angle), x87f_load_f32(MSG_ANGLE_TO_BYTE_SCALE)));
#else
    packed = coduo_fp_to_i32_extended((long double)angle * (long double)MSG_ANGLE_TO_BYTE_SCALE);
#endif
    message->data[message->cursize++] = (uint8_t)packed;
}
#else
void MSG_WriteAngle(msg_t *message, float angle)
{
    int32_t packed;

    /* coduo_lnxded 0x080807cb evaluates angle * 256 / 360 before the normal
     * MSG_WriteByte capacity decision and truncates directly to a dword. */
#if EMULATE_X87
    packed = x87f_store_i32_trunc(x87f_div(x87f_mul(x87f_load_f32(angle), x87f_load_f32(256.0f)), x87f_load_f32(360.0f)));
#else
    packed = coduo_fp_to_i32_extended(((long double)angle * 256.0L) / 360.0L);
#endif
    MSG_WriteByte(message, packed & MSG_BYTE_MASK);
}
#endif

#if defined(WINDOWS_BEHAVIOR)
void MSG_WriteAngle16(msg_t *message, float angle)
{
    const int32_t end = (int32_t)((uint32_t)message->cursize + 2U);
    int32_t packed;

    /* CoDUOMP.exe 0x00449b80 performs the capacity decision before entering
     * the x87 domain. */
    if (end > message->maxsize) {
        message->overflowed = qtrue;
        return;
    }

#if EMULATE_X87
    packed = (int32_t)(uint32_t)x87f_store_i64_trunc(x87f_mul(x87f_load_f32(angle), x87f_load_f32(MSG_ANGLE_TO_SHORT_SCALE)));
#else
    packed = coduo_fp_to_i32_extended((long double)angle * (long double)MSG_ANGLE_TO_SHORT_SCALE);
#endif
    message->data[message->cursize] = (uint8_t)packed;
    message->data[message->cursize + 1] = (uint8_t)((uint32_t)packed >> 8U);
    message->cursize = end;
}
#else
void MSG_WriteAngle16(msg_t *message, float angle)
{
    int32_t packed;

    /* coduo_lnxded 0x08080815 computes and truncates first, then delegates
     * the capacity decision to MSG_WriteShort. */
#if EMULATE_X87
    packed = x87f_store_i32_trunc(x87f_mul(x87f_load_f32(angle), x87f_load_f32(MSG_ANGLE_TO_SHORT_SCALE)));
#else
    packed = coduo_fp_to_i32_extended((long double)angle * (long double)MSG_ANGLE_TO_SHORT_SCALE);
#endif
    MSG_WriteShort(message, packed & MSG_SHORT_MASK);
}
#endif

int32_t MSG_ReadByte(msg_t *message)
{
    if (message->readcount >= message->cursize) {
        return MSG_READ_OVERFLOW_VALUE;
    }
    return message->data[message->readcount++];
}

int32_t MSG_ReadShort(msg_t *message)
{
    const int32_t end = (int32_t)((uint32_t)message->readcount + 2U);
    uint16_t stored;

    if (end > message->cursize) {
        return MSG_READ_OVERFLOW_VALUE;
    }

    stored = (uint16_t)message->data[message->readcount] | (uint16_t)((uint16_t)message->data[message->readcount + 1] << 8U);
    message->readcount = end;
    return (int16_t)stored;
}

int32_t MSG_ReadLong(msg_t *message)
{
    const int32_t end = (int32_t)((uint32_t)message->readcount + 4U);
    uint32_t stored;

    if (end > message->cursize) {
        return MSG_READ_OVERFLOW_VALUE;
    }

    stored = (uint32_t)message->data[message->readcount] | ((uint32_t)message->data[message->readcount + 1] << 8U) |
             ((uint32_t)message->data[message->readcount + 2] << 16U) | ((uint32_t)message->data[message->readcount + 3] << 24U);
    message->readcount = end;
    return (int32_t)stored;
}

char *MSG_ReadString(msg_t *message)
{
    int32_t length = 0;

    while (message->readcount < message->cursize) {
        const int32_t character = MSG_ReadByte(message);

        if (character == MSG_READ_OVERFLOW_VALUE || character == '\0') {
            break;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: retain the bounded result prefix while
         * consuming the complete wire string through its terminator. */
        if (length < MAX_STRING_CHARS - 1) {
            msg_readString[length++] = (char)Q_CleanCharacter((uint8_t)character);
        }
    }

    msg_readString[length] = '\0';
    return msg_readString;
}

char *MSG_ReadBigString(msg_t *message)
{
    int32_t length = 0;

    while (message->readcount < message->cursize) {
        int32_t character = MSG_ReadByte(message);

        if (character == MSG_READ_OVERFLOW_VALUE || character == '\0') {
            break;
        }
        if (character == '%') {
            character = '.';
        }
        /* NOT_FROM_ORIGINAL_SOURCE: preserve the bounded sanitized prefix
         * while consuming through the wire NUL. */
        if (length < BIG_INFO_STRING - 1) {
            msg_readBigString[length++] = (char)Q_CleanCharacter((uint8_t)character);
        }
    }

    msg_readBigString[length] = '\0';
    return msg_readBigString;
}

char *MSG_ReadStringLine(msg_t *message)
{
    int32_t length = 0;

    while (message->readcount < message->cursize) {
        int32_t character = MSG_ReadByte(message);

        if (character == MSG_READ_OVERFLOW_VALUE || character == '\0' || character == '\n') {
            break;
        }
        if (character == '%') {
            character = '.';
        }
        /* NOT_FROM_ORIGINAL_SOURCE: retain the bounded line prefix while
         * consuming through newline or NUL. */
        if (length < MAX_STRING_CHARS - 1) {
            msg_readStringLine[length++] = (char)Q_CleanCharacter((uint8_t)character);
        }
    }

    msg_readStringLine[length] = '\0';
    return msg_readStringLine;
}

float MSG_ReadAngle16(msg_t *message)
{
    /* The scale is exactly 9/16384. Every int16 product is exactly
     * representable in binary32, so the Windows/Linux x87 precision policy
     * cannot change this result. */
    return (float)MSG_ReadShort(message) * MSG_ANGLE16_FROM_SHORT_SCALE;
}

void MSG_ReadData(msg_t *message, void *data, int32_t length)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        Com_Printf("MSG_ReadData: negative length %i\n", length);
        return;
    }

    const int32_t end = (int32_t)((uint32_t)message->readcount + (uint32_t)length);
    const size_t originalLength = (size_t)(uint32_t)length;
    if (end > message->cursize) {
        memset(data, 0xff, originalLength);
        return;
    }

    memcpy(data, message->data + message->readcount, originalLength);
    message->readcount = end;
}

#undef MSG_ANGLE16_FROM_SHORT_SCALE
#undef MSG_ANGLE_TO_SHORT_SCALE
#undef MSG_ANGLE_TO_BYTE_SCALE
