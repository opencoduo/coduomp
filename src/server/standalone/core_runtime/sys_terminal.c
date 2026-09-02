#include <string.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "core_runtime_private.h"

#define SYS_ISATTY_TRUE 1
#define SYS_LINUX_TERMIOS_VERASE_INDEX 2
#define SYS_LINUX_TERMIOS_VEOF_INDEX 4
#define SYS_LINUX_TERMIOS_VTIME_INDEX 5
#define SYS_LINUX_TERMIOS_VMIN_INDEX 6
#define SYS_LINUX_TERMIOS_RAW_INPUT_MASK 48
#define SYS_LINUX_TERMIOS_NONCANONICAL_LOCAL_MASK 10
#define SYS_LINUX_TERMIOS_MIN_READ_BYTES 1
#define SYS_LINUX_TERMIOS_READ_TIMEOUT 0

void Sys_ShutdownTerminalConsole(void)
{
    if (sys_ttyConsoleActive != 0) {
#if defined(_WIN32)
        Com_Printf("Shutdown Windows console\n");
        sys_ttyConsoleActive = 0;
#else
        Com_Printf("Shutdown tty console\n");
        tcsetattr(SYS_STDIN_FILE_DESCRIPTOR, TCSADRAIN, &sys_ttyOriginalTermios);
#endif
    }
}

void Sys_InitTerminalConsole(void)
{
#if !defined(_WIN32)
    struct termios terminalSettings;

    memset(&terminalSettings, 0, sizeof(terminalSettings));
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
#endif

    ttycon = Cvar_Get("ttycon", "1", CVAR_NONE);
    if (ttycon == NULL || ttycon->value == 0.0f) {
        sys_ttyConsoleActive = 0;
        return;
    }

#if defined(_WIN32)
    if (_isatty(SYS_STDIN_FILE_DESCRIPTOR) != SYS_ISATTY_TRUE) {
#else
    if (isatty(SYS_STDIN_FILE_DESCRIPTOR) != SYS_ISATTY_TRUE) {
#endif
        Com_Printf("stdin is not a tty, tty console mode failed\n");
        Cvar_Set("ttycon", "0");
        sys_ttyConsoleActive = 0;
        return;
    }

#if defined(_WIN32)
    Com_Printf("Started Windows console (use +set ttycon 0 to disable)\n");
    Sys_TTYResetLine(&sys_ttyCurrentLine);
    sys_ttyEraseChar = '\b';
    sys_ttyEofChar = 0x1a;
    sys_ttyConsoleActive = 1;
#else
    Com_Printf("Started tty console (use +set ttycon 0 to disable)\n");
    Sys_TTYResetLine(&sys_ttyCurrentLine);
    tcgetattr(SYS_STDIN_FILE_DESCRIPTOR, &sys_ttyOriginalTermios);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    sys_ttyEraseChar =
        (unsigned char)terminalSettings.c_cc[SYS_LINUX_TERMIOS_VERASE_INDEX];
    sys_ttyEofChar =
        (unsigned char)terminalSettings.c_cc[SYS_LINUX_TERMIOS_VEOF_INDEX];

    terminalSettings = sys_ttyOriginalTermios;
    terminalSettings.c_lflag &= ~SYS_LINUX_TERMIOS_NONCANONICAL_LOCAL_MASK;
    terminalSettings.c_iflag &= ~SYS_LINUX_TERMIOS_RAW_INPUT_MASK;
    terminalSettings.c_cc[SYS_LINUX_TERMIOS_VMIN_INDEX] =
        SYS_LINUX_TERMIOS_MIN_READ_BYTES;
    terminalSettings.c_cc[SYS_LINUX_TERMIOS_VTIME_INDEX] =
        SYS_LINUX_TERMIOS_READ_TIMEOUT;
    tcsetattr(SYS_STDIN_FILE_DESCRIPTOR, TCSADRAIN, &terminalSettings);
    sys_ttyConsoleActive = 1;
#endif
}
