#if !defined(_WIN32)
#include <fcntl.h>
#endif
#include <unistd.h>

#include "core_runtime_private.h"

#define SYS_NORMAL_EXIT_STATUS 0
#define SYS_DELAYED_PROCESS_SLEEP_SECONDS 1

void Sys_StartProcessNow(const char *command);

void Sys_Exit(int status)
{
    Sys_ShutdownTerminalConsole();
    if (sys_delayedProcessCommand[0] != '\0') {
        sleep(SYS_DELAYED_PROCESS_SLEEP_SECONDS);
        Sys_StartProcessNow(sys_delayedProcessCommand);
        sleep(SYS_DELAYED_PROCESS_SLEEP_SECONDS);
    }

    _exit(status);
}

void Sys_Quit(void)
{
#if !defined(_WIN32)
    int flags;
#endif

    CL_Shutdown();
    Cvar_Shutdown();
    Cmd_Shutdown();
    Hunk_Shutdown();
#if !defined(_WIN32)
    flags = fcntl(SYS_STDIN_FILE_DESCRIPTOR, SYS_F_GETFL_COMMAND,
                  SYS_F_GETFL_UNUSED_ARGUMENT);
    fcntl(SYS_STDIN_FILE_DESCRIPTOR, SYS_F_SETFL_COMMAND,
          flags & ~SYS_LINUX_O_NONBLOCK);
#endif
    Sys_Exit(SYS_NORMAL_EXIT_STATUS);
}
