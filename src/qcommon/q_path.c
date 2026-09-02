#include "q_path.h"

#include "com_sprintf.h"
#include "q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * The four authoritative Windows copies have the same instruction bodies
 * after normalizing image-local CRT calls, format addresses, and stack-cookie
 * checks:
 *
 *                         EXE        cgame      UI         game
 * Com_SkipPath            0044f310   3004e100   40006130   20057920
 * Com_StripExtension      0044f330   3004e120   40006150   20057940
 * Com_StripFilename       0044f350   3004e140   40006170   20057960
 * Com_DefaultExtension    0044f3a0   3004e190   400061c0   200579b0
 *
 * The Linux engine bodies at 0x080862d9, 0x08086308, 0x0808633d, and
 * 0x0808637a agree with the game-module bodies at RVAs 0x00092d0d,
 * 0x00092d3c, 0x00092d71, and 0x00092dbe.  Windows inlines Q_strncpyz in the
 * latter two functions while Linux retains the call; every copy has the same
 * copy count, terminator position, reverse scan, and output behavior.
 * Supporting Mac traceback symbols retain the exact Com_* spellings.
 */

char *Com_SkipPath(char *path)
{
    char *name = path;

    for (; *path != '\0'; ++path) {
        if (*path == '/') {
            name = path + 1;
        }
    }
    return name;
}

void Com_StripExtension(const char *input, char *output)
{
    while (*input != '\0' && *input != '.') {
        *output++ = *input++;
    }
    *output = '\0';
}

void Com_StripFilename(const char *input, char *output)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (*input == '\0') {
        *output = '\0';
        return;
    }
    Q_strncpyz(output, input, (int32_t)strlen(input));
    *Com_SkipPath(output) = '\0';
}

void Com_DefaultExtension(char *path, int32_t maximumSize, const char *extension)
{
    char oldPath[MAX_QPATH];
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    char *scan = path + strlen(path);

    if (scan != path) {
        --scan;
        while (*scan != '/' && scan != path) {
            if (*scan == '.') {
                return;
            }
            --scan;
        }
    }

    Q_strncpyz(oldPath, path, (int32_t)sizeof(oldPath));
    Com_sprintf(path, (size_t)(uint32_t)maximumSize, "%s%s", oldPath, extension);
}
