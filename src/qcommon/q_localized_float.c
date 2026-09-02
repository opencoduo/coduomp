#include "q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "q_localized_float.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * Complete localized-float helper pair:
 *
 *                                  decimal delimiter     formatter
 * CoDUOMP.exe                      0x00450830            0x00450860
 * uo_cgame_mp_x86.dll             0x3004f620            0x3004f650
 * uo_ui_mp_x86.dll                0x40007650            0x40007680
 * uo_game_mp_x86.dll              0x20058e40            0x20058e70
 * coduo_lnxded                     0x08087d42            0x08087d4c
 * game.mp.uo.i386.so              RVA 0x000949d7        RVA 0x000949e1
 *
 * The four Windows bodies are instruction-identical apart from relocations:
 * French, German, Italian, Spanish, Russian, and Polish use a comma. Both
 * Linux bodies ignore the language argument and always use a period. This is
 * therefore a genuine build-mode distinction, not compiler shaping.
 *
 * Legacy MSVC _snprintf receives bufferSize - 1 and can fill that complete
 * count without terminating; the following explicit store terminates at
 * bufferSize - 1. C99 snprintf reserves one character of its count for NUL.
 * Passing bufferSize to C99 therefore reproduces the Windows byte result for
 * every nonzero size. Linux keeps its literal size-minus-one call.
 */

#if defined(WINDOWS_BEHAVIOR)
int32_t Q_GetDecimalDelimiter(language_t language)
{
    switch (language) {
    case LANGUAGE_FRENCH:
    case LANGUAGE_GERMAN:
    case LANGUAGE_ITALIAN:
    case LANGUAGE_SPANISH:
    case LANGUAGE_RUSSIAN:
    case LANGUAGE_POLISH:
        return ',';
    default:
        return '.';
    }
}

void Q_LocalizedFloatToString(float value, char *buffer, uint32_t bufferSize, int32_t precision, language_t language)
{
    /* NOT_FROM_ORIGINAL_SOURCE: a zero-sized output has no writable
     * terminator slot. */
    if (bufferSize == 0) {
        return;
    }
    snprintf(buffer, (size_t)bufferSize, "%.*f", precision, (double)value);
    buffer[bufferSize - 1u] = '\0';

    const int32_t delimiter = Q_GetDecimalDelimiter(language);
    if (delimiter == '.') {
        return;
    }

    for (uint32_t index = 0; index < bufferSize; ++index) {
        if (buffer[index] == '.') {
            buffer[index] = (char)delimiter;
            return;
        }
    }
}
#else
int32_t Q_GetDecimalDelimiter(language_t language)
{
    (void)language;
    return '.';
}

void Q_LocalizedFloatToString(float value, char *buffer, uint32_t bufferSize, int32_t precision, language_t language)
{
    /* NOT_FROM_ORIGINAL_SOURCE: a zero-sized output has no writable
     * terminator slot. */
    if (bufferSize == 0) {
        return;
    }
    snprintf(buffer, (size_t)(bufferSize - 1u), "%.*f", precision, (double)value);
    buffer[bufferSize - 1u] = '\0';

    const int32_t delimiter = Q_GetDecimalDelimiter(language);
    if (delimiter == '.') {
        return;
    }

    for (uint32_t index = 0; index < bufferSize; ++index) {
        if (buffer[index] == '.') {
            buffer[index] = (char)delimiter;
            return;
        }
    }
}
#endif
