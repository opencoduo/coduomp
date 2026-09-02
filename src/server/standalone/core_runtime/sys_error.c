#if !defined(_WIN32)
#include <fcntl.h>
#endif
#include <stdarg.h>
#include <stdio.h>

#include "core_runtime_private.h"

void Sys_Error(const char *format, ...)
{
#if !defined(_WIN32)
    int flags;
#endif
    char message[MAX_STRING_CHARS];
    va_list argptr;

#if !defined(_WIN32)
    flags = fcntl(SYS_STDIN_FILE_DESCRIPTOR, SYS_F_GETFL_COMMAND, SYS_F_GETFL_UNUSED_ARGUMENT);
    fcntl(SYS_STDIN_FILE_DESCRIPTOR, SYS_F_SETFL_COMMAND, flags & ~SYS_LINUX_O_NONBLOCK);
#endif

    if (sys_ttyConsoleActive != 0) {
        Sys_TTYHideInputLine();
    }

    CL_Shutdown();

    va_start(argptr, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted fatal diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, argptr);
    va_end(argptr);

    fprintf(stderr, "Sys_Error: %s\n", message);
    Sys_Exit(SYS_ERROR_EXIT_STATUS);
}
