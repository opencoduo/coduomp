#include "bg_animation.h"
#include "bg_animation_services.h"
#include "compat/coduo_int32_bits.h"
#include "qcommon/com_parse.h"
#include "com_parse_error_binding.h"

#include <stdarg.h>
#include <stdio.h>

/*
 * Complete animation-parser diagnostic shared by the cgame and game modules:
 *
 *   uo_cgame_mp_x86.dll        0x30001200
 *   uo_game_mp_x86.dll         0x20001200
 *   game.mp.uo.i386.so         RVA 0x00019a9c
 *
 * All three original bodies format through unbounded vsprintf into a
 * 1024-byte local,
 * test the same bgPlayerAnimScriptPath pointer, and append the current parse
 * line plus one when a source is active.  Windows cgame reads that line field
 * directly while the Linux game calls Com_GetCurrentParseLine; the accessor
 * returns that exact field.  The Windows game routes the final message through
 * G_Error while cgame and Linux game use Com_Error.  COM_PARSE_ERROR preserves
 * that already-proven module/platform error boundary without splitting this
 * common body.
 */
void BG_AnimParseError(const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    va_list arguments;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted animation-parser
     * diagnostic to its fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (bgPlayerAnimScriptPath == NULL) {
        COM_PARSE_ERROR(ERR_DROP, "\x15" "%s", message);
    } else {
        const int32_t displayLine = coduo_int32_from_bits(
            (uint32_t)Com_GetCurrentParseLine() + 1u);

        COM_PARSE_ERROR(ERR_DROP, "\x15" "%s: (%s, line %i)", message,
                        bgPlayerAnimScriptPath, displayLine);
    }
}
