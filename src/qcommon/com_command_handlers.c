#include "com_command_handlers.h"
#include "com_command_services.h"

#include "compat/coduo_int32_bits.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    COM_FREEZE_EXPECTED_ARGC = 2,
    COM_FREEZE_SECONDS_ARG = 1,
    COM_CRASH_TEST_PATTERN = 0x12345678
};

static const double com_millisecondsToSeconds = 0.001;

/*
 * Complete console command-handler cluster:
 *
 *                              Windows client       Linux dedicated
 * Com_Quit_f                   0x0043a2c0           0x0807066c
 * Com_Error_f                  0x0043af80           0x0807129b
 * Com_Freeze_f                 0x0043afb0           0x080712d7
 * Com_Crash_f                  0x0043b010           0x08071344
 *
 * Both quit bodies use the same error gate and common shutdown sequence.
 * COM_QUIT_TARGET_CLEANUP retains the client/listen-engine cleanup calls and
 * the dedicated engine's server-frame reset in their owning target.  The
 * Linux reconstruction formerly reversed the final two filesystem calls;
 * the retail body calls 0x08065429 (FS_ShutdownServerPakNames) before
 * 0x08065483 (FS_ShutdownServerReferencedPaks), matching Windows.
 */
_Noreturn void Com_Quit_f(void)
{
    if (com_errorEntered != qfalse) {
        Sys_Quit();
    }

    Com_ClearTempMemory();
    COM_QUIT_TARGET_CLEANUP();
    SV_Shutdown("EXE_SERVERQUIT");
    Hunk_Clear();
    Com_Close();
    FS_Shutdown(qtrue);
    FS_ShutdownServerPakNames();
    FS_ShutdownServerReferencedPaks();
    Sys_Quit();
}

void Com_Error_f(void)
{
    if (Cmd_Argc() > 1) {
        Com_Error(ERR_DROP, "\x15"
                            "Testing drop error");
    } else {
        Com_Error(ERR_FATAL, "\x15"
                             "Testing fatal error");
    }
}

void Com_Freeze_f(void)
{
    if (Cmd_Argc() != COM_FREEZE_EXPECTED_ARGC) {
        Com_Printf("freeze <seconds>\n");
        return;
    }

    const float seconds = (float)atof(Cmd_Argv(COM_FREEZE_SECONDS_ARG));
    const uint32_t startTime = (uint32_t)Com_Milliseconds();

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (;;) {
        const int32_t elapsed = coduo_int32_from_bits((uint32_t)Com_Milliseconds() - startTime);
        if ((double)elapsed * com_millisecondsToSeconds > (double)seconds) {
            break;
        }
    }
}

void Com_Crash_f(void)
{
    *(volatile uint32_t *)0 = COM_CRASH_TEST_PATTERN;
}
