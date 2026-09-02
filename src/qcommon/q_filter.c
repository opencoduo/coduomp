#include "q_string.h"

#include "compat/coduo_ctype_compat.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "q_filter.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    COM_FILTER_CHUNK_SIZE = 1024
};

/* NOT_FROM_ORIGINAL_SOURCE: both retail bodies pass a sign-extended byte to
 * their target CRT toupper.  MSVC returns -128..-2 unchanged, whereas glibc's
 * signed-char compatibility entries return the corresponding unsigned byte.
 * Keep that CRT boundary distinction out of the original matcher bodies. */
static int coduo_filter_toupper(int value)
{
    const int signedValue = (int)(int8_t)(uint8_t)value;

#if defined(WINDOWS_BEHAVIOR)
    if (signedValue < EOF) {
        return signedValue;
    }
#endif
    return toupper(coduo_ctype_signed_byte_arg(signedValue));
}

/*
 * Complete common wildcard-filter subsystem.  The Windows client and Linux
 * dedicated server retain the same control flow and byte-width operations:
 *
 *                            CoDUOMP.exe          coduo_lnxded
 * Com_StringContains         0x004350f0           0x0806b538
 * Com_Filter                 0x004351b0           0x0806b606
 * Com_FilterPath             0x004353d0           0x0806b926
 *
 * Their only target-library distinction is the signed-byte toupper result
 * isolated above.  Com_FilterPath uses two 64-byte buffers, normalizes '\\'
 * and ':' to '/', and truncates both inputs at MAX_QPATH - 1.
 */

const char *Com_StringContains(const char *haystack, const char *needle,
                               qboolean caseSensitive)
{
    const int32_t maximumOffset =
        (int32_t)strlen(haystack) - (int32_t)strlen(needle);

    for (int32_t offset = 0; offset <= maximumOffset;
         ++offset, ++haystack) {
        int32_t index;

        for (index = 0; needle[index] != '\0'; ++index) {
            if (caseSensitive != qfalse) {
                if (haystack[index] != needle[index]) {
                    break;
                }
            } else if (coduo_filter_toupper(haystack[index]) !=
                       coduo_filter_toupper(needle[index])) {
                break;
            }
        }
        if (needle[index] == '\0') {
            return haystack;
        }
    }

    return NULL;
}

qboolean Com_Filter(const char *filter, const char *name,
                    qboolean caseSensitive)
{
    char chunk[COM_FILTER_CHUNK_SIZE];

    for (;;) {
        if (*filter == '\0') {
            return qtrue;
        }

        if (*filter == '*') {
            int32_t chunkLength = 0;

            while (*++filter != '\0' && *filter != '*' && *filter != '?') {
                /* NOT_FROM_ORIGINAL_SOURCE: a literal filter chunk and its
                 * terminator must fit the fixed matching buffer. */
                if (chunkLength >= COM_FILTER_CHUNK_SIZE - 1) {
                    return qfalse;
                }
                chunk[chunkLength++] = *filter;
            }
            chunk[chunkLength] = '\0';

            if (chunk[0] != '\0') {
                const char *match =
                    Com_StringContains(name, chunk, caseSensitive);

                if (match == NULL) {
                    return qfalse;
                }
                name = match + strlen(chunk);
            }
            continue;
        }

        if (*filter == '?') {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (*name == '\0') {
                return qfalse;
            }
            ++filter;
            ++name;
            continue;
        }

        if (*filter == '[' && filter[1] == '[') {
            ++filter;
            continue;
        }

        if (*filter == '[') {
            qboolean matched = qfalse;

            ++filter;
            while (*filter != '\0' && matched == qfalse &&
                   (*filter != ']' || filter[1] == ']')) {
                if (filter[1] == '-' && filter[2] != '\0' &&
                    (filter[2] != ']' || filter[3] == ']')) {
                    if (caseSensitive != qfalse) {
                        const int32_t nameCharacter =
                            (int32_t)(int8_t)(uint8_t)*name;
                        if ((int32_t)(int8_t)(uint8_t)*filter <=
                                nameCharacter &&
                            nameCharacter <=
                                (int32_t)(int8_t)(uint8_t)filter[2]) {
                            matched = qtrue;
                        }
                    } else {
                        const int32_t nameCharacter =
                            coduo_filter_toupper(*name);

                        if (coduo_filter_toupper(*filter) <= nameCharacter &&
                            nameCharacter <=
                                coduo_filter_toupper(filter[2])) {
                            matched = qtrue;
                        }
                    }
                    filter += 3;
                } else {
                    if (caseSensitive != qfalse) {
                        if (*filter == *name) {
                            matched = qtrue;
                        }
                    } else if (coduo_filter_toupper(*filter) ==
                               coduo_filter_toupper(*name)) {
                        matched = qtrue;
                    }
                    ++filter;
                }
            }

            if (matched == qfalse) {
                return qfalse;
            }
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            while (*filter != '\0' &&
                   (*filter != ']' || filter[1] == ']')) {
                ++filter;
            }
            if (*filter == '\0') {
                return qfalse;
            }
            ++filter;
            ++name;
            continue;
        }

        if (caseSensitive != qfalse) {
            if (*filter != *name) {
                return qfalse;
            }
        } else if (coduo_filter_toupper(*filter) !=
                   coduo_filter_toupper(*name)) {
            return qfalse;
        }

        ++filter;
        ++name;
    }
}

qboolean Com_FilterPath(const char *filter, const char *name,
                        qboolean caseSensitive)
{
    char normalizedFilter[MAX_QPATH];
    char normalizedName[MAX_QPATH];
    int32_t index;

    for (index = 0;
         index < MAX_QPATH - 1 && filter[index] != '\0';
         ++index) {
        normalizedFilter[index] =
            (filter[index] == '\\' || filter[index] == ':')
                ? '/'
                : filter[index];
    }
    normalizedFilter[index] = '\0';

    for (index = 0;
         index < MAX_QPATH - 1 && name[index] != '\0';
         ++index) {
        normalizedName[index] =
            (name[index] == '\\' || name[index] == ':') ? '/' : name[index];
    }
    normalizedName[index] = '\0';

    return Com_Filter(normalizedFilter, normalizedName, caseSensitive);
}
