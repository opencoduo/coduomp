#define _POSIX_C_SOURCE 200809L

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../core_memory/core_memory_private.h"
#include "qcommon/q_command.h"
#include "../filesystem/fs_private.h"
#include "../scripting/script_runtime_private.h"
#include "../server/sv_init_shutdown_private.h"
#include "core_runtime_private.h"

#define COM_ERROR_BURST_WINDOW_MSEC 100
#define COM_ERROR_BURST_FATAL_COUNT 4
#define COM_ERROR_RECOVERABLE_LONGJMP_VALUE -1
#define COM_ERROR_CVAR_FLAGS CVAR_ROM
#define COM_ERROR_BUFFER_COPY_SIZE (COM_ERROR_MESSAGE_CAPACITY + 8)
#define COM_WEAPON_INFO_SERVER_STATE 1
#define COM_WEAPON_INFO_CLIENT_STATE 2
#define COM_ERROR_KEEP_WEAPON_MEMORY qfalse
#define COM_FS_RESTART_CLEAR_LOOKUPS qtrue

char com_errorMessage[COM_ERROR_MESSAGE_CAPACITY];
int32_t com_errorEntered;

static int32_t com_lastErrorTime;
static int32_t com_errorCount;
static errorParm_t com_recoverableErrorCode;

void Com_ClearServerFrameRunningFlag(void)
{
    sv_frameRunning = qfalse;
}

void Com_SetErrorMessage(const char *message)
{
    Cvar_Get("com_errorMessage", "", COM_ERROR_CVAR_FLAGS);
    Cvar_Set("com_errorMessage", message);
}

void Com_ErrorCleanup(void)
{
    if (com_recoverableErrorCode == ERR_SERVER_DISCONNECT) {
        Com_Shutdown("EXE_DISCONNECTEDFROMOWNLISTENSERVER");
        com_errorEntered = qfalse;
    } else if (com_recoverableErrorCode == ERR_END_GAME) {
        Com_Shutdown("EXE_ENDOFGAME");
        com_errorEntered = qfalse;
    } else if (com_recoverableErrorCode == ERR_DROP ||
               com_recoverableErrorCode == ERR_DISCONNECT) {
        Com_Printf("********************\nERROR: %s\n********************\n",
                   com_errorMessage);
        Com_Shutdown(com_errorMessage);
        com_errorEntered = qfalse;

        cvar_t *rVcCompile = Cvar_Get("r_vc_compile", "0", 0);
        if (com_recoverableErrorCode == ERR_DROP &&
            rVcCompile->integer == COM_WEAPON_INFO_CLIENT_STATE) {
            Com_Quit_f();
        }
    } else if (com_recoverableErrorCode == ERR_NEED_CD) {
        Com_Shutdown("EXE_SERVERDIDNTHAVECD");
        if (cl_running == NULL || cl_running->integer == 0) {
            Com_Printf("Server didn't have CD\n");
        } else {
            com_errorEntered = qfalse;
            CL_CDDialog();
        }
    }

    Com_InitServerRuntimePools();
}

void Com_Error(errorParm_t code, const char *format, ...)
{
    char shutdownMessage[COM_ERROR_BUFFER_COPY_SIZE];
    va_list args;

    if (com_errorEntered != qfalse) {
        Sys_Error("recursive error after: %s", com_errorMessage);
    }

    Com_ClearTempMemory();

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to the global
     * error buffer and its downstream fixed destinations. */
    (void)vsnprintf(com_errorMessage, sizeof(com_errorMessage), format, args);
    va_end(args);

    (void)snprintf(shutdownMessage, sizeof(shutdownMessage), "%s",
                   com_errorMessage);

    if (code == ERR_SCRIPT ||
        code == ERR_LOCALIZATION) {
        code = ERR_DROP;
    }

    com_errorEntered = qtrue;
    FS_PureServerSetLoadedPaks("", "");

    if (code != ERR_DISCONNECT && code != ERR_NEED_CD &&
        code != ERR_END_GAME) {
        Com_SetErrorMessage(com_errorMessage);
    }

    if (code != ERR_DISCONNECT) {
        Scr_Abort();
    }

    Com_ClearServerFrameRunningFlag();
    Com_ResetParseSessions();
    FS_ResetFiles();

    if (code == ERR_DROP) {
        Cbuf_Init();
    }

    FreeWeaponInfoMemory(COM_WEAPON_INFO_SERVER_STATE,
                         COM_ERROR_KEEP_WEAPON_MEMORY);
    FreeWeaponInfoMemory(COM_WEAPON_INFO_CLIENT_STATE,
                         COM_ERROR_KEEP_WEAPON_MEMORY);

    int32_t now = Sys_Milliseconds();
    if (now - com_lastErrorTime < COM_ERROR_BURST_WINDOW_MSEC) {
        com_errorCount++;
        if (com_errorCount >= COM_ERROR_BURST_FATAL_COUNT) {
            code = ERR_FATAL;
        }
    } else {
        com_errorCount = 0;
    }
    com_lastErrorTime = now;

    if (code != ERR_SERVER_DISCONNECT && code != ERR_END_GAME &&
        code != ERR_DROP && code != ERR_DISCONNECT &&
        code != ERR_NEED_CD) {
        CL_Shutdown();
        SV_Shutdown(va("EXE_SERVER_FATAL_CRASHED\x15 \x14%s",
                       shutdownMessage));
        Hunk_Clear();
        Com_Close();
        Sys_Error("%s", com_errorMessage);
        return;
    }

    com_recoverableErrorCode = code;
    longjmp(com_frameAbortContext, COM_ERROR_RECOVERABLE_LONGJMP_VALUE);
}
