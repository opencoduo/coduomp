// Source: uo_cgame_mp_x86.dll 0x3002d850..0x3002da84
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002d850_3002da84.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <string.h>

/*
 * CG_TranslateMessage — localize text, then replace the first
 * "[{command}]" marker with "[bound keys]" in a two-slot result buffer.
 */
const char *CG_TranslateMessage(const char *src, const char *keyOrFormat)
{
    const char *text = (const char *)(intptr_t)cgame_syscall(
        CG_SE_LOCALIZE_MESSAGE, (intptr_t)src,
        (intptr_t)keyOrFormat);
    const char *open = NULL;
    const char *close = NULL;

    for (const char *p = text; *p != '\0'; ++p) {
        if (p[1] == '\0' || p[2] == '\0' || p[3] == '\0' ||
            p[4] == '\0') {
            break;
        }
        if (p[0] == '[' && p[1] == '{') {
            open = p;
            break;
        }
    }
    if (open == NULL) {
        return text;
    }

    for (const char *p = open; *p != '\0'; ++p) {
        if (p[1] == '\0') {
            break;
        }
        if (p[0] == '}' && p[1] == ']') {
            close = p;
            break;
        }
    }
    if (close == NULL) {
        return text;
    }

    {
        char command[MAX_STRING_CHARS];
        ptrdiff_t commandLength = close - (open + 2);
        char *keys = NULL;
        size_t keyLength;
        size_t prefixLength;
        size_t tailLength;
        int32_t resultLength;

        /* 0x3002d955 passes the raw command length to CRT strncpy, then the
         * following store writes the terminator at command[commandLength]. */
        strncpy(command, open + 2, (size_t)commandLength);
        command[commandLength] = '\0';

        Controls_GetConfig();
        if (UI_KeysStringForBinding(command, &keys) == 0) {
            return text;
        }

        keyLength = strlen(keys);

        /* 0x3002d998..0x3002d9a8 toggles and selects the output buffer before
         * the eventual length rejection. The changed selector is observable
         * even when the function returns the original overlong text. */
        cg_translateMessageBufferIndex ^= 1;
        char *buffer = cg_translateMessageBuffers[cg_translateMessageBufferIndex];

        prefixLength = (size_t)(open - text);
        tailLength = strlen(close + 2);

        /* 0x3002d9c9..0x3002d9db forms this size in one target dword and uses a
         * signed JL against 1024. The rebuilt text keeps '[' and ']' while
         * dropping only "{command}". */
        resultLength = coduo_int32_from_bits(
            (uint32_t)prefixLength + 1u + (uint32_t)keyLength + 1u +
            (uint32_t)tailLength);
        if (resultLength >= (int32_t)sizeof(cg_translateMessageBuffers[0])) {
            Com_Printf("String too long to add key binding: %s\n", text);
            return text;
        }

        char *out;
        size_t remaining;

        /* Preserve the '[' immediately before the extracted command. */
        strncpy(buffer, text, prefixLength + 1);
        buffer[prefixLength + 1] = '\0';

        out = buffer + prefixLength + 1;
        strncpy(out, keys, keyLength);
        out[keyLength] = '\0';
        out += keyLength;

        /* Start at close+1 so the original closing ']' is retained. The raw
         * CRT strncpy is bounded to the remaining half-buffer minus one byte,
         * followed by the machine's forced final terminator. */
        remaining = sizeof(cg_translateMessageBuffers[0]) - (size_t)(out - buffer);
        strncpy(out, close + 1, remaining - 1);
        buffer[sizeof(cg_translateMessageBuffers[0]) - 1] = '\0';
        return buffer;
    }
}
