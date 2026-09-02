#include "q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    Q_STRING_WHOLE_COMPARE_COUNT = 99999
};

void Com_Error(errorParm_t code, const char *format, ...);

/* Original-machine-code sources for the common Q string/character subsystem.
 * The Windows bodies at each address are instruction-identical within a
 * function except for image-local call targets. The Linux bodies use the same
 * ASCII predicates, byte order, copy bounds, color-code grammar, and return
 * domains. Supporting Mac symbols provide the retained canonical Q_* names.
 *
 *                         Win EXE     Win cgame   Win UI      Win game
 * Q_isprint               0044f6d0    3004e4c0   400064f0    20057ce0
 * Q_islower               0044f6f0    3004e4e0   40006510    20057d00
 * Q_isupper               0044f710    3004e500   40006530    20057d20
 * Q_isalpha               0044f730    3004e520   40006550    20057d40
 * Q_isnumeric             0044f750    3004e540   40006570    20057d60
 * Q_isalphanumeric        0044f770    3004e560   40006590    20057d80
 * Q_isforfilename         0044f7a0    3004e590   400065c0    20057db0
 * Q_strrchr               0044f7e0    3004e5d0   40006600    20057df0
 * Q_strncpyz              0044f810    3004e600   40006630    20057e20
 * Q_stricmpn              0044f830    3004e620   40006650    20057e40
 * Q_strncmp               0044f890    3004e680   400066b0    20057ea0
 * Q_stricmp               0044f8d0    3004e6c0   400066f0    20057ee0
 * Q_strlwr                0044f8f0    3004e6e0   40006710    20057f00
 * Q_strupr                0044f920    3004e710   40006740    20057f30
 * Q_strcat                0044f950    3004e740   40006770    20057f60
 * Q_DrawStrlen            0044f9a0    3004e790   400067c0    20057fb0
 * Q_CleanStr              0044f9d0    3004e7c0   400067f0    20057fe0
 * Q_CleanCharacter        0044fa20    3004e810   40006840    20058030
 * Q_strncasecmp           0044fa50    3004e840   40006870    20058060
 * Q_strcasecmp            0044faa0    3004e890   400068c0    200580b0
 *
 * Linux engine addresses from Q_strncpyz onward are 0808691a, 08086946,
 * 080869eb, 08086a5a, 08086a97, 08086ad1, 08086b5e, 08086bc6, 08086c4e,
 * 08086ccc, and 08086d56. Q_strcat is at 08086b0b. Linux game RVAs for
 * the complete sequence are 0009325e, 00093285, 000932ac, 000932d3,
 * 00093308, 0009332f, 0009337a,
 * 000933c8, 00093415, 00093451, 00093506, 00093575, 000935c2, 00093609,
 * 000936b5, 0009371d, 000937a5, 00093833, and 000938cd.
 */

qboolean Q_isprint(int32_t character)
{
    return character >= ' ' && character <= '~' ? qtrue : qfalse;
}

qboolean Q_islower(int32_t character)
{
    return character >= 'a' && character <= 'z' ? qtrue : qfalse;
}

qboolean Q_isupper(int32_t character)
{
    return character >= 'A' && character <= 'Z' ? qtrue : qfalse;
}

qboolean Q_isalpha(int32_t character)
{
    return ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')) ? qtrue : qfalse;
}

qboolean Q_isnumeric(int32_t character)
{
    return character >= '0' && character <= '9' ? qtrue : qfalse;
}

qboolean Q_isalphanumeric(int32_t character)
{
    return ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))
               ? qtrue
               : qfalse;
}

qboolean Q_isforfilename(int32_t character)
{
    return (Q_isalphanumeric(character) != qfalse || character == '_' || character == '-') && character != ' ' ? qtrue : qfalse;
}

char *Q_strrchr(const char *string, int32_t character)
{
    const char target = (char)character;
    const char *cursor = string;
    const char *match = NULL;

    while (*cursor != '\0') {
        if (*cursor == target) {
            match = cursor;
        }
        ++cursor;
    }

    return (char *)(target == '\0' ? cursor : match);
}

void Q_strncpyz(char *destination, const char *source, int32_t size)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (size <= 0) {
        return;
    }

    const uint32_t copyCount = (uint32_t)size - UINT32_C(1);
    int32_t lastIndex;

    memcpy(&lastIndex, &copyCount, sizeof(lastIndex));
    strncpy(destination, source, (size_t)copyCount);
    destination[lastIndex] = '\0';
}

int32_t Q_stricmpn(const char *left, const char *right, int32_t count)
{
    uint32_t remaining = (uint32_t)count;

    for (;;) {
        int32_t leftCharacter;
        int32_t rightCharacter;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (remaining == 0) {
            return 0;
        }
        remaining -= UINT32_C(1);
        leftCharacter = (int32_t)(signed char)*left++;
        rightCharacter = (int32_t)(signed char)*right++;

        if (leftCharacter != rightCharacter) {
            if (leftCharacter >= 'a' && leftCharacter <= 'z') {
                leftCharacter -= 'a' - 'A';
            }
            if (rightCharacter >= 'a' && rightCharacter <= 'z') {
                rightCharacter -= 'a' - 'A';
            }
            if (leftCharacter != rightCharacter) {
                return leftCharacter < rightCharacter ? -1 : 1;
            }
        }

        if (leftCharacter == '\0') {
            return 0;
        }
    }
}

int32_t Q_strncmp(const char *left, const char *right, int32_t count)
{
    uint32_t remaining = (uint32_t)count;

    for (;;) {
        int32_t leftCharacter;
        int32_t rightCharacter;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (remaining == 0) {
            return 0;
        }
        remaining -= UINT32_C(1);
        leftCharacter = (int32_t)(signed char)*left++;
        rightCharacter = (int32_t)(signed char)*right++;

        if (leftCharacter != rightCharacter) {
            return leftCharacter < rightCharacter ? -1 : 1;
        }
        if (leftCharacter == '\0') {
            return 0;
        }
    }
}

int32_t Q_stricmp(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return -1;
    }

    return Q_stricmpn(left, right, Q_STRING_WHOLE_COMPARE_COUNT);
}

char *Q_strlwr(char *string)
{
    char *cursor;

    /* No original image calls setlocale. Its CRT calls therefore use the C
     * locale: ASCII letters fold and every other stored byte is unchanged. */
    for (cursor = string; *cursor != '\0'; ++cursor) {
        if (*cursor >= 'A' && *cursor <= 'Z') {
            *cursor = (char)(*cursor + ('a' - 'A'));
        }
    }
    return string;
}

char *Q_strupr(char *string)
{
    char *cursor;

    for (cursor = string; *cursor != '\0'; ++cursor) {
        if (*cursor >= 'a' && *cursor <= 'z') {
            *cursor = (char)(*cursor - ('a' - 'A'));
        }
    }
    return string;
}

void Q_strcat(char *destination, int32_t size, const char *source)
{
    const int32_t destinationLength = (int32_t)strlen(destination);

    /* All original bodies use a signed comparison here. */
    if (destinationLength >= size) {
        Com_Error(ERR_FATAL, "\x15"
                             "Q_strcat: already overflowed");
    }

    Q_strncpyz(destination + destinationLength, source, size - destinationLength);
}

int32_t Q_DrawStrlen(const char *string)
{
    uint32_t length = 0;
    int32_t result;

    while (*string != '\0') {
        if (string[0] == '^' && string[1] != '\0' && string[1] != '^' && string[1] >= '0' && string[1] <= '9') {
            string += 2;
        } else {
            ++length;
            ++string;
        }
    }

    memcpy(&result, &length, sizeof(result));
    return result;
}

char *Q_CleanStr(char *string)
{
    char *source = string;
    char *destination = string;

    while (*source != '\0') {
        if (source[0] == '^' && source[1] != '\0' && source[1] != '^' && source[1] >= '0' && source[1] <= '9') {
            ++source;
        } else {
            const int32_t character = (int32_t)(signed char)*source;
            if (character >= ' ' && character <= '~') {
                *destination++ = *source;
            }
        }
        ++source;
    }
    *destination = '\0';
    return string;
}

uint8_t Q_CleanCharacter(uint8_t character)
{
    if (character == UINT8_C(0x92)) {
        return (uint8_t)'\'';
    }
    if (character > UINT8_C(0x7f)) {
        return (uint8_t)'.';
    }
    return character;
}

int32_t Q_strncasecmp(const char *left, const char *right, int32_t count)
{
    uint32_t remaining = (uint32_t)count;

    for (;;) {
        int32_t leftCharacter;
        int32_t rightCharacter;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (remaining == 0) {
            return 0;
        }
        remaining -= UINT32_C(1);
        leftCharacter = (int32_t)(signed char)*left++;
        rightCharacter = (int32_t)(signed char)*right++;

        if (leftCharacter != rightCharacter) {
            if (leftCharacter >= 'a' && leftCharacter <= 'z') {
                leftCharacter -= 'a' - 'A';
            }
            if (rightCharacter >= 'a' && rightCharacter <= 'z') {
                rightCharacter -= 'a' - 'A';
            }
            if (leftCharacter != rightCharacter) {
                return -1;
            }
        }
        if (leftCharacter == '\0') {
            return 0;
        }
    }
}

int32_t Q_strcasecmp(const char *left, const char *right)
{
    return Q_strncasecmp(left, right, Q_STRING_WHOLE_COMPARE_COUNT);
}
