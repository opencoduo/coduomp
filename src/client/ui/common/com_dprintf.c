#include "../module/ui_functions.h"

#include <stdarg.h>
#include <stdio.h>

enum {
    COM_DEVELOPER_MESSAGE_SIZE = 4096
};

// Source: uo_ui_mp_x86.dll 0x400076e0..0x40007756
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400076e0_40007756.mcode
// Exact same-module PPC symbol: Com_DPrintf.
void Com_DPrintf(const char *format, ...)
{
    char message[COM_DEVELOPER_MESSAGE_SIZE];
    va_list arguments;

    if ((int32_t)trap_Cvar_VariableValue("com_developer") == 0) {
        return;
    }

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound developer diagnostics to the fixed
     * message buffer. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    Com_Printf("%s", message);
}
