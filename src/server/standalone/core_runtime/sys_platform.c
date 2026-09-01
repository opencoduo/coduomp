#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "core_runtime_private.h"

void Sys_Init(void)
{
    Cmd_AddCommand("in_restart", IN_Restart_f);
    Cvar_Set("arch", "linux i386");
    Cvar_Set("username", Sys_GetCurrentUser());
    Sys_InitInput();
}

void Sys_Warning(const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    va_list argptr;

    va_start(argptr, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted warning to its fixed
     * destination. */
    (void)vsnprintf(message, sizeof(message), format, argptr);
    va_end(argptr);

    if (sys_ttyConsoleActive != 0) {
        Sys_TTYHideInputLine();
    }

    fprintf(stderr, "Warning: %s", message);

    if (sys_ttyConsoleActive != 0) {
        Sys_TTYShowInputLine();
    }
}

void Sys_OutOfMemory(void)
{
    fprintf(stderr, "OUT OF MEMORY! ABORTING!!!\n");
    exit(SYS_OUT_OF_MEMORY_EXIT_STATUS);
}
