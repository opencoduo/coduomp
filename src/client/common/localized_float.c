#include "client_common.h"

#include "client_legacy_crt.h"
#include "compat/crt/format_compat.h"

/*
 * The authoritative Windows client-module bodies are instruction-identical
 * after rebasing their format-string and CRT-call operands:
 *
 *   uo_cgame_mp_x86.dll  0x3004f650..0x3004f6a5
 *   uo_ui_mp_x86.dll     0x40007680..0x400076d5
 */
void Com_FormatLocalizedFloat(char *buffer, uint32_t bufferSize,
                              int32_t precision, language_t language,
                              float value)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (bufferSize == 0) {
        return;
    }
    (void)coduo_crt_snprintf(
        buffer, bufferSize - 1u, "%.*f", precision, (double)value);
    buffer[bufferSize - 1u] = '\0';

    switch (language) {
    case LANGUAGE_FRENCH:
    case LANGUAGE_GERMAN:
    case LANGUAGE_ITALIAN:
    case LANGUAGE_SPANISH:
    case LANGUAGE_RUSSIAN:
    case LANGUAGE_POLISH:
        for (uint32_t index = 0; index < bufferSize; ++index) {
            if (buffer[index] == '.') {
                buffer[index] = ',';
                break;
            }
        }
        break;
    default:
        break;
    }
}
