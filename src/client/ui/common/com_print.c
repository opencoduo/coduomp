#include "../module/ui_functions.h"

#include <stdarg.h>
#include <stdio.h>

// Source: uo_ui_mp_x86.dll 0x40007760..0x400077b7
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007760_400077b7.mcode
// Exact same-module PPC symbol: Com_Error.
void Com_Error(errorParm_t level, const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    va_list arguments;

    (void)level;
    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound formatted diagnostics to the fixed
     * message buffer. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    trap_Error(va("%s", message));
}

// Source: uo_ui_mp_x86.dll 0x400077c0..0x40007817
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400077c0_40007817.mcode
// Exact same-module PPC symbol: Com_Printf.
void Com_Printf(const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    va_list arguments;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound formatted errors to the fixed message
     * buffer. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    trap_Print(va("%s", message));
}
