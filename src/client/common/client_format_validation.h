#ifndef CLIENT_COMMON_FORMAT_VALIDATION_H
#define CLIENT_COMMON_FORMAT_VALIDATION_H

#include "qcommon/q_shared_types.h"

#include <stdint.h>

/* NOT_FROM_ORIGINAL_SOURCE: validates the deliberately narrow conversion
 * contract used before mounted menu/localization text crosses a formatter.
 * Each signature byte is a promoted argument category: 's' is const char *,
 * 'i' is int32_t, and 'f' is double. Templates may omit trailing arguments,
 * but cannot consume more arguments or change their types. Fixed flags,
 * widths, and precision are safe; '*' and length modifiers are not. */
static inline qboolean client_compat_validate_format_signature(const char *format, const char *signature)
{
    int32_t argumentIndex = 0;

    for (const char *cursor = format; *cursor != '\0'; ++cursor) {
        if (*cursor != '%') {
            continue;
        }
        ++cursor;
        if (*cursor == '%') {
            continue;
        }

        while (*cursor == '-' || *cursor == '+' || *cursor == ' ' || *cursor == '#' || *cursor == '0') {
            ++cursor;
        }
        if (*cursor == '*') {
            return qfalse;
        }
        while (*cursor >= '0' && *cursor <= '9') {
            ++cursor;
        }
        if (*cursor == '.') {
            ++cursor;
            if (*cursor == '*') {
                return qfalse;
            }
            while (*cursor >= '0' && *cursor <= '9') {
                ++cursor;
            }
        }

        const char expected = signature[argumentIndex];
        if (expected == '\0') {
            return qfalse;
        }
        if ((expected == 's' && *cursor != 's') || (expected == 'i' && *cursor != 'd' && *cursor != 'i') ||
            (expected == 'f' && *cursor != 'f' && *cursor != 'F' && *cursor != 'e' && *cursor != 'E' && *cursor != 'g' && *cursor != 'G')) {
            return qfalse;
        }
        ++argumentIndex;
    }
    return qtrue;
}

#endif
