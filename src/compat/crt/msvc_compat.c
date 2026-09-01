#include "compat/crt/msvc_compat.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: C-locale MSVC atoi compatibility boundary. */
int32_t coduo_crt_atoi(const char *string)
{
    const unsigned char *cursor = (const unsigned char *)string;
    uint32_t value = 0;
    int32_t negative = 0;
    int32_t result;

    while (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\r')) {
        ++cursor;
    }
    if (*cursor == '-' || *cursor == '+') {
        negative = *cursor == '-';
        ++cursor;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10u + (uint32_t)(*cursor - '0');
        ++cursor;
    }
    if (negative != 0) {
        value = 0u - value;
    }

    memcpy(&result, &value, sizeof(result));
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: deterministic MSVC C-locale comparison. */
int32_t coduo_crt_stricmp(const char *left, const char *right)
{
    for (;;) {
        const uint8_t leftCharacter =
            (uint8_t)coduo_crt_tolower((uint8_t)*left++);
        const uint8_t rightCharacter =
            (uint8_t)coduo_crt_tolower((uint8_t)*right++);

        if (leftCharacter != rightCharacter || leftCharacter == 0) {
            return (int32_t)leftCharacter - (int32_t)rightCharacter;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: deterministic bounded MSVC comparison. */
int32_t coduo_crt_strnicmp(const char *left, const char *right,
                           size_t count)
{
    while (count != 0) {
        const uint8_t leftCharacter =
            (uint8_t)coduo_crt_tolower((uint8_t)*left++);
        const uint8_t rightCharacter =
            (uint8_t)coduo_crt_tolower((uint8_t)*right++);

        --count;
        if (leftCharacter != rightCharacter || leftCharacter == 0) {
            return (int32_t)leftCharacter - (int32_t)rightCharacter;
        }
    }
    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: deterministic in-place MSVC case folding. */
char *coduo_crt_strlwr(char *text)
{
    for (char *cursor = text; *cursor != '\0'; ++cursor) {
        *cursor = (char)coduo_crt_tolower((uint8_t)*cursor);
    }
    return text;
}

/* NOT_FROM_ORIGINAL_SOURCE: deterministic in-place MSVC case folding. */
char *coduo_crt_strupr(char *text)
{
    for (char *cursor = text; *cursor != '\0'; ++cursor) {
        *cursor = (char)coduo_crt_toupper((uint8_t)*cursor);
    }
    return text;
}

/* NOT_FROM_ORIGINAL_SOURCE: MSVC C-locale byte classification boundary. */
int32_t coduo_crt_isalpha(int32_t character)
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z');
}

/* NOT_FROM_ORIGINAL_SOURCE: MSVC C-locale byte classification boundary. */
int32_t coduo_crt_isalnum(int32_t character)
{
    return coduo_crt_isalpha(character) != 0 ||
           (character >= '0' && character <= '9');
}

/* NOT_FROM_ORIGINAL_SOURCE: MSVC C-locale byte classification boundary. */
int32_t coduo_crt_isspace(int32_t character)
{
    return character == ' ' ||
           (character >= '\t' && character <= '\r');
}

/* NOT_FROM_ORIGINAL_SOURCE: MSVC C-locale byte-folding boundary. */
int32_t coduo_crt_tolower(int32_t character)
{
    if (character >= 'A' && character <= 'Z') {
        return character + ('a' - 'A');
    }
    return character;
}

/* NOT_FROM_ORIGINAL_SOURCE: MSVC C-locale byte-folding boundary. */
int32_t coduo_crt_toupper(int32_t character)
{
    if (character >= 'a' && character <= 'z') {
        return character - ('a' - 'A');
    }
    return character;
}
