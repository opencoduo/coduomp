#include "ui_runtime.h"

#include "ui_menu_globals.h"
#include "ui_parse.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * These complete UI-runtime utility leaves are instruction-identical between
 * the two authoritative Windows module binaries after rebasing:
 *
 *                               cgame          UI
 * Copy40Bytes                   0x3004cc90     0x40004ca0
 * HexDigitValue                0x3004e0c0     0x400060f0
 * LocaleDecimalSeparator       0x3004f620     0x40007650
 * Display_KeyBindPending       0x30057010     0x40018b70
 * AdjustFrom640                0x30057210     0x40018d70
 */

void Copy40Bytes(void *destination, const void *source)
{
    unsigned char *const output = destination;
    const unsigned char *const input = source;

    /* The originals perform ten ordered forward dword transfers.  Preserve
     * their overlap behavior without requiring host alignment or aliasing. */
    for (size_t wordIndex = 0; wordIndex < 10; ++wordIndex) {
        uint32_t word;
        const size_t offset = wordIndex * sizeof(word);

        memcpy(&word, input + offset, sizeof(word));
        memcpy(output + offset, &word, sizeof(word));
    }
}

int32_t HexDigitValue(char character)
{
    const uint8_t value = (uint8_t)(character - '0');

    return value < 10 ? value : 7;
}

char LocaleDecimalSeparator(language_t locale)
{
    switch (locale) {
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

qboolean Display_KeyBindPending(void)
{
    return g_waitingForKey;
}

void AdjustFrom640(float *x, float *y, float *width, float *height)
{
    displayContextDef_t *display = DC;

    /* The original store order is alias-visible.  Each multiplication loads
     * two binary32 operands into x87 and stores its result to binary32 before
     * advancing to the next pointer. */
    *x = (float)((long double)*x * (long double)display->xscale);
    *y = (float)((long double)*y * (long double)display->yscale);
    *width = (float)((long double)*width * (long double)display->xscale);
    *height = (float)((long double)*height * (long double)display->yscale);
}
