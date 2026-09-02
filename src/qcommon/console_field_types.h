#ifndef QCOMMON_CONSOLE_FIELD_TYPES_H
#define QCOMMON_CONSOLE_FIELD_TYPES_H

#include "q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

enum {
    CON_INPUT_BUFFER_SIZE = 256,
    CON_HISTORY_FIELD_COUNT = 32
};

/* Edited console line shared by the Windows client and Linux dedicated
 * server. Client field editing and the server tty path agree on the complete
 * 0x11c-byte i386 layout; the server preserves the extra display lanes in
 * whole-record history copies even though its tty renderer does not use them. */
typedef struct console_input_field_s {
    int32_t cursor;
    int32_t scroll;
    int32_t widthInChars;
    int32_t widthInPixels;
    float charWidth;
    float charHeight;
    qboolean fixedSize;
    char buffer[CON_INPUT_BUFFER_SIZE];
} console_input_field_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(console_input_field_t) == 0x11c, "console_input_field_t size mismatch");
_Static_assert(offsetof(console_input_field_t, widthInPixels) == 0x0c, "console_input_field_t.widthInPixels offset mismatch");
_Static_assert(offsetof(console_input_field_t, fixedSize) == 0x18, "console_input_field_t.fixedSize offset mismatch");
_Static_assert(offsetof(console_input_field_t, buffer) == 0x1c, "console_input_field_t.buffer offset mismatch");
#endif

#endif
