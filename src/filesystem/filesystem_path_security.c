#include "filesystem_path_security.h"

#include <stddef.h>

/* NOT_FROM_ORIGINAL_SOURCE: source-level helper for the shared confinement
 * policy below. */
static qboolean coduo_compat_is_path_separator(char character)
{
    return character == '/' || character == '\\' ? qtrue : qfalse;
}

qboolean coduo_compat_path_is_safe_relative(const char *path)
{
    /* NOT_FROM_ORIGINAL_SOURCE: virtual paths crossing into host filesystem
     * calls must remain relative and below their selected engine root. */
    if (path == NULL || path[0] == '\0' ||
        coduo_compat_is_path_separator(path[0]) != qfalse) {
        return qfalse;
    }

    const char *component = path;
    for (const char *cursor = path;; ++cursor) {
        const char character = *cursor;
        if (character == ':')
            return qfalse;
        if (character != '\0' &&
            coduo_compat_is_path_separator(character) == qfalse) {
            continue;
        }

        const size_t componentLength = (size_t)(cursor - component);
        if (componentLength == 0) {
            if (character == '\0' && cursor != path)
                return qtrue;
            return qfalse;
        }
        if ((componentLength == 1 && component[0] == '.') ||
            (componentLength == 2 && component[0] == '.' && component[1] == '.')) {
            return qfalse;
        }

        if (character == '\0')
            return qtrue;
        component = cursor + 1;
    }
}
