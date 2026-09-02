#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "platform/crt_boundary.h"
#include "platform/floating_point_boundary.h"
#include "platform/punkbuster_boundary.h"
#include "q_shared.h"
#include "server/server.h"
#include "system_console.h"
#include "system_info.h"
#include "system_input.h"
#include "system_localization.h"
#include "system_platform.h"
#include "system_process_lock.h"

#include <stdint.h>
#include <string.h>

enum {
    SYS_COMMAND_LINE_CAPACITY = 1024,
    SYS_CONSOLE_HIDDEN = 0,
    SYS_IDLE_SLEEP_MSEC = 5,
    SYS_ALLOW_DUPLICATE_PREFIX_LENGTH = 9
};

/* Original 0x009cedb0..0x009cf1af. Only WinMain and Com_Init consume this
 * retained copy of the process command line. */
static char sysCommandLine[SYS_COMMAND_LINE_CAPACITY];

/* Source: CoDUOMP.exe 0x0046c5c0..0x0046c8d4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046c5c0_0046c8d5.mcode and the
 * imported Win32/WinMM calls.
 * Name and signature: the MSVC CRT startup caller at 0x0056fb98 proves the
 * WinMain ABI. The optional PunkBuster calls retain their positions through
 * the disabled modern platform boundary; no retired backend code is carried
 * into the recovered executable. */
int WINAPI WinMain(HINSTANCE applicationInstance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand)
{
    char workingDirectory[MAX_OSPATH];

    (void)showCommand;
    coduomp_restore_retail_x87_precision();
    (void)Sys_InitLocalization();

    if (coduo_crt_strnicmp(commandLine, "allowdupe", SYS_ALLOW_DUPLICATE_PREFIX_LENGTH) != 0 ||
        (int8_t)commandLine[SYS_ALLOW_DUPLICATE_PREFIX_LENGTH] > (int8_t)' ') {
        Sys_InitProcessLockFile();
        if (Sys_CheckProcessLock() == qfalse) {
            Sys_ShutdownLocalization();
            return 0;
        }
    }

    if (previousInstance != NULL) {
        Sys_ShutdownLocalization();
        return 0;
    }

    Sys_InitHardwareInfo();

    /* The optimized retail helper receives the PE image base implicitly in
     * EDI. The maintained platform API makes that dependency explicit. */
    sysExecutableChecksum = Sys_GetExecutableChecksum(applicationInstance);
    sysApplicationInstance = applicationInstance;

    strncpy(sysCommandLine, commandLine, sizeof(sysCommandLine) - 1);
    sysCommandLine[sizeof(sysCommandLine) - 1] = '\0';

    Sys_CreateConsole();
    Sys_CreateSplashWindow();
    Sys_ShowSplashWindow();
    (void)SetErrorMode(SEM_FAILCRITICALERRORS);

    /* WinMain contains the same lazy WinMM-clock initialization as
     * Sys_Milliseconds and discards the first resulting timestamp. */
    (void)Sys_Milliseconds();

    Com_Init(sysCommandLine);
    (void)coduomp_crt_getcwd(workingDirectory, sizeof(workingDirectory));
    Com_Printf("Working directory: %s\n", workingDirectory);

    if ((dedicated == NULL || dedicated->integer == 0) && com_viewlog->integer == 0) {
        Sys_ShowConsole(SYS_CONSOLE_HIDDEN, qfalse);
    }

    if (dedicated == NULL || dedicated->integer == 0)
        PB_InitializeClient(applicationInstance);
    PB_InitializeServer();
    (void)SetFocus(win32MainWindow);

    for (;;) {
        if (sysWindowMinimized != qfalse || (dedicated != NULL && dedicated->integer != 0)) {
            Sleep(SYS_IDLE_SLEEP_MSEC);
        }

        IN_Frame();
        Com_Frame();

        if (dedicated == NULL || dedicated->integer == 0)
            PB_ProcessClientEvents();
        PB_ProcessServerEvents();
    }
}

#else

#include "platform/crt_boundary.h"
#include "platform/sdl_platform.h"
#include "q_shared.h"
#include "system_info.h"
#include "system_input.h"
#include "system_localization.h"
#include "system_platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include "platform/macos_app_bundle.h"
#endif

enum {
    SYS_NATIVE_COMMAND_LINE_CAPACITY = 8192
};

/* NOT_FROM_ORIGINAL_SOURCE: constructs the mutable command string consumed by
 * Com_ParseCommandLine from the native argc/argv process interface. Quoting is
 * retained for arguments containing whitespace so startup cvar values survive
 * the source-level conversion. */
static qboolean Sys_BuildNativeCommandLine(int argc, char **argv, char *commandLine, size_t commandLineSize)
{
    size_t cursor = 0;

    commandLine[0] = '\0';
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const char *argument = argv[argumentIndex];
        const qboolean quote = strpbrk(argument, " \t") != NULL ? qtrue : qfalse;
        const size_t argumentLength = strlen(argument);
        const size_t required = argumentLength + (quote != qfalse ? 2U : 0U) + (cursor != 0 ? 1U : 0U);

        if (required >= commandLineSize - cursor)
            return qfalse;
        if (cursor != 0)
            commandLine[cursor++] = ' ';
        if (quote != qfalse)
            commandLine[cursor++] = '"';
        memcpy(&commandLine[cursor], argument, argumentLength);
        cursor += argumentLength;
        if (quote != qfalse)
            commandLine[cursor++] = '"';
        commandLine[cursor] = '\0';
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: native process entry point corresponding to the
 * recovered WinMain. Subsystem initialization and the frame loop preserve the
 * original ordering; SDL replaces only the Win32 application shell. */
int main(int argc, char **argv)
{
    char commandLine[SYS_NATIVE_COMMAND_LINE_CAPACITY];
    char workingDirectory[MAX_OSPATH];

#if defined(__APPLE__)
    if (coduomp_macos_prepare_launch(argc, argv) == 0)
        return EXIT_SUCCESS;
#endif

    if (Sys_BuildNativeCommandLine(argc, argv, commandLine, sizeof(commandLine)) == qfalse) {
        fputs("CoDUOMP: command line is too long\n", stderr);
        return EXIT_FAILURE;
    }

    (void)Sys_InitLocalization();
    Sys_InitHardwareInfo();
    sysExecutableChecksum = Sys_GetExecutableChecksum(NULL);

    if (CoduoSDL_Init() == qfalse) {
        fputs("CoDUOMP: SDL initialization failed\n", stderr);
        Sys_ShutdownLocalization();
        return EXIT_FAILURE;
    }
    (void)atexit(CoduoSDL_Shutdown);

    (void)Sys_Milliseconds();
    Com_Init(commandLine);
    (void)coduomp_crt_getcwd(workingDirectory, sizeof(workingDirectory));
    Com_Printf("Working directory: %s\n", workingDirectory);

    for (;;) {
        IN_Frame();
        Com_Frame();
    }
}

#endif
