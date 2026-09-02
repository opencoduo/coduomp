#include "com_string.h"

#include <stdint.h>

/*
 * The Windows client and Linux dedicated-server bodies implement the same
 * bounded append operation:
 *
 *   CoDUOMP.exe   0x0043d7e0
 *   coduo_lnxded  0x08072dde
 *
 * The same-module Mac client exports the canonical name Com_AddToString.  The
 * signed byte comparison is intentional: an empty argument or any argument
 * containing a byte whose signed value is at most ASCII space is quoted.
 */
int32_t Com_AddToString(const char *source, char *destination, int32_t offset, int32_t limit, qboolean addQuotes)
{
    qboolean needsQuotes = qfalse;

    if (addQuotes != qfalse) {
        const int32_t available = limit - offset;

        if (source[0] == '\0') {
            needsQuotes = qtrue;
        } else if (available > 0) {
            for (int32_t index = 0; index < available; ++index) {
                const int8_t character = (int8_t)source[index];

                if (character == '\0') {
                    break;
                }
                if (character <= (int8_t)' ') {
                    needsQuotes = qtrue;
                    break;
                }
            }
        }
    }

    if (needsQuotes != qfalse && offset < limit) {
        destination[offset++] = '"';
    }

    for (const char *cursor = source; *cursor != '\0' && offset < limit; ++cursor) {
        destination[offset++] = *cursor;
    }

    if (needsQuotes != qfalse && offset < limit) {
        destination[offset++] = '"';
    }

    return offset;
}
