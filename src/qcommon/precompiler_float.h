#ifndef QCOMMON_PRECOMPILER_FLOAT_H
#define QCOMMON_PRECOMPILER_FLOAT_H

#include "compat/coduo_x87emu.h"

#include <stdint.h>
#include <string.h>

enum {
    PC_X87_EXTENDED_TBYTE_SIZE = 10
};

#if EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE: a Linux .pc number token stores the parsed value
 * as a raw x87 80-bit TBYTE in its 12-byte floatValue slot.  Off x87,
 * extFloat80_t has the same little-endian ten-byte payload, so this adapter
 * preserves the original token representation. */
static inline void coduo_pc_store_token_float80(uint8_t *destination,
                                                 x87f value)
{
    memcpy(destination, &value, PC_X87_EXTENDED_TBYTE_SIZE);
}

/* NOT_FROM_ORIGINAL_SOURCE: inverse of the TBYTE storage adapter above. */
static inline x87f coduo_pc_load_token_float80(const uint8_t *source)
{
    x87f value;
    memset(&value, 0, sizeof(value));
    memcpy(&value, source, PC_X87_EXTENDED_TBYTE_SIZE);
    return value;
}
#endif

#endif
