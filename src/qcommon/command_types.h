#ifndef QCOMMON_COMMAND_TYPES_H
#define QCOMMON_COMMAND_TYPES_H

#include <stddef.h>
#include <stdint.h>

enum {
    CBUF_TEXT_CAPACITY = 65536,
    CBUF_COMMAND_CAPACITY = 4096,
    CMD_ARGUMENT_CAPACITY = 512,
    CMD_TOKEN_BUFFER_CAPACITY = 8704,
    /* Joined arguments are derived solely from cmd_tokenBuffer.  Keeping the
     * destination at the same capacity preserves every accepted command. */
    CMD_ARGS_CAPACITY = CMD_TOKEN_BUFFER_CAPACITY
};

/* Canonical Quake-family command-buffer record.  CoDUOMP.exe and
 * coduo_lnxded agree on data +0x00, maxsize +0x04, and cursize +0x08. */
typedef struct cbuf_s {
    char *data;
    int32_t maxsize;
    int32_t cursize;
} cbuf_t;

typedef void (*com_redirect_flush_t)(char *buffer);

#if UINTPTR_MAX == UINT32_MAX
#if defined(__cplusplus)
#define COMMAND_TYPES_ALIGNOF alignof
#define COMMAND_TYPES_STATIC_ASSERT static_assert
#else
#define COMMAND_TYPES_ALIGNOF _Alignof
#define COMMAND_TYPES_STATIC_ASSERT _Static_assert
#endif
COMMAND_TYPES_STATIC_ASSERT(COMMAND_TYPES_ALIGNOF(cbuf_t) == 4,
               "i386 command-buffer alignment changed");
COMMAND_TYPES_STATIC_ASSERT(offsetof(cbuf_t, data) == 0x00,
               "i386 command-buffer data pointer moved");
COMMAND_TYPES_STATIC_ASSERT(offsetof(cbuf_t, maxsize) == 0x04,
               "i386 command-buffer maximum size moved");
COMMAND_TYPES_STATIC_ASSERT(offsetof(cbuf_t, cursize) == 0x08,
               "i386 command-buffer current size moved");
COMMAND_TYPES_STATIC_ASSERT(sizeof(cbuf_t) == 0x0c,
               "i386 command-buffer size changed");
#undef COMMAND_TYPES_STATIC_ASSERT
#undef COMMAND_TYPES_ALIGNOF
#endif

#endif
