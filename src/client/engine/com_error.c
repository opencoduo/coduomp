#include "q_shared.h"

#include "com_startup.h"
#include "client/cgame.h"
#include "client/console.h"
#include "animation/dobj.h"
#include "animation/xanim_pool.h"
#include "filesystem/filesystem.h"
#include "qcommon/hunk.h"
#include "scripting/script_runtime.h"
#include "server/server.h"
#include "sound/miles_boundary.h"
#include "system_platform.h"
#include "ui/ui_module_loader.h"
#include "renderer/renderer_api.h"

#include "qcommon/q_string.h"

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

errorParm_t com_errorCode;
char com_errorMessage[COM_ERROR_MESSAGE_CAPACITY];
/* NOT_FROM_ORIGINAL_SOURCE: native setjmp/longjmp carrier corresponding to
 * the executable's MSVC CRT jump buffer; its layout is intentionally supplied
 * by each target platform's CRT rather than copied from the Win32 object. */
jmp_buf com_abortFrame;
static int32_t com_consecutiveErrorCount;
static uint32_t com_lastErrorTime;

enum {
    COM_ERROR_MESSAGE_CVAR_FLAGS = 64,
    COM_VC_COMPILE_QUIT_MODE = 2,
    COM_ERROR_BURST_INTERVAL_MSEC = 100,
    COM_ERROR_BURST_FATAL_THRESHOLD = 3,
    COM_WEAPON_MEMORY_GAME_OWNER = 1,
    COM_WEAPON_MEMORY_CGAME_OWNER = 2
};

/* Source: CoDUOMP.exe 0x00439e10..0x00439f1a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00439e10_00439f1b.mcode.
 * Name: exact same-module Mac symbol Com_ErrorCleanup. The Windows build
 * defers CL_CDDialog through cls.cdDialogRequested; the Mac implementation
 * calls its platform-specific no-op directly. */
void Com_ErrorCleanup(void)
{
    switch (com_errorCode) {
    case ERR_SERVER_DISCONNECT:
        /* Reconstruction transcription fix: CoDUOMP.exe 0x00439e1b pushes
         * 0x005989a4, the own-listen-server disconnect string. */
        Com_Shutdown("EXE_DISCONNECTEDFROMOWNLISTENSERVER");
        com_errorEntered = qfalse;
        break;

    case ERR_END_GAME:
        /* Reconstruction transcription fix: CoDUOMP.exe 0x00439e3e pushes
         * 0x00598994, the end-game string. */
        Com_Shutdown("EXE_ENDOFGAME");
        com_errorEntered = qfalse;
        break;

    case ERR_NEED_CD:
        Com_Shutdown("EXE_SERVERDIDNTHAVECD");
        if (cl_running != NULL && cl_running->integer != 0) {
            com_errorEntered = qfalse;
            CL_CDDialog();
            Com_Restart();
            return;
        }
        Com_Printf("Server didn't have CD\n");
        break;

    case ERR_DROP:
    case ERR_DISCONNECT:
        Com_Printf(
            "\n********************\nERROR: %s\n********************\n",
            com_errorMessage);
        if (com_errorCode == ERR_DROP &&
            (dedicated == NULL || dedicated->integer == 0)) {
            CL_ConsoleFixPosition();
        }
        Com_Shutdown(com_errorMessage);
        com_errorEntered = qfalse;
        if (com_errorCode == ERR_DROP &&
            Cvar_Get("r_vc_compile", "0", 0)->integer ==
                COM_VC_COMPILE_QUIT_MODE) {
            Com_Quit_f();
        }
        break;

    default:
        break;
    }

    Com_Restart();
}

/* Source: CoDUOMP.exe 0x00439f80..0x00439f8c.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00439f80_00439f8d.mcode.
 * Name: exact same-module Mac symbol Com_CleanupSkeletons. */
void Com_CleanupSkeletons(void)
{
    sv_frameRunning = qfalse;
    cl_frameRunning = qfalse;
}

/* Source: CoDUOMP.exe 0x00439f90..0x0043a011.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00439f90_0043a012.mcode.
 * Name and argument: exact same-module Mac symbol Com_SetErrorMessage. */
void Com_SetErrorMessage(const char *message)
{
    (void)Cvar_Get(
        "com_errorMessage", "", COM_ERROR_MESSAGE_CVAR_FLAGS);

    if (message == NULL || message[0] == '\0') {
        Cvar_Set("com_errorMessage", "");
        return;
    }

    const char *const localized = SEH_LocalizeTextMessage(
        message, "error message", LOCMSG_NOERR);
    if (localized == NULL) {
        Cvar_Set("com_errorMessage", message);
        return;
    }

    Cvar_Set("com_errorMessage", localized);
    Q_strncpyz(com_errorMessage, localized,
               COM_ERROR_MESSAGE_CAPACITY);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
_Noreturn void Com_Error(errorParm_t errorCode, const char *format, ...)
{
    char originalMessage[COM_ERROR_MESSAGE_CAPACITY];
    va_list args;

    if (com_errorEntered != qfalse)
        Sys_Error("recursive error after: %s", com_errorMessage);

    Com_ClearTempMemory();

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    va_start(args, format);
    (void)vsnprintf(com_errorMessage, sizeof(com_errorMessage), format, args);
    va_end(args);
    Q_strncpyz(originalMessage, com_errorMessage, sizeof(originalMessage));

    if (errorCode == ERR_SCRIPT ||
        errorCode == ERR_LOCALIZATION) {
        errorCode = ERR_DROP;
    }

    com_errorEntered = qtrue;
    FS_PureServerSetLoadedPaks("", "");
    SEH_UpdateLanguageInfo();

    if (errorCode == ERR_DISCONNECT ||
        errorCode == ERR_NEED_CD ||
        errorCode == ERR_END_GAME) {
        if (com_errorMessage[0] != '\0') {
            const char *const localized = SEH_LocalizeTextMessage(
                com_errorMessage, "error message", LOCMSG_NOERR);
            if (localized != NULL) {
                Q_strncpyz(com_errorMessage, localized,
                           COM_ERROR_MESSAGE_CAPACITY);
            }
        }
    } else {
        if (coduo_uiVm != NULL) {
            (void)VM_Call(coduo_uiVm, UIVM_SET_ACTIVE_MENU,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
        Com_SetErrorMessage(com_errorMessage);
    }

    if (errorCode != ERR_DISCONNECT)
        Scr_Abort();

    MSS_ErrorCleanup();
    Com_CleanupSkeletons();
    Com_ResetParseSessions();
    if (rendererExports.ResetImageAllocations != NULL)
        rendererExports.ResetImageAllocations();
    FS_ResetFiles();

    if (errorCode == ERR_DROP)
        Cbuf_Init();

    Com_FreeWeaponInfoMemory(COM_WEAPON_MEMORY_GAME_OWNER, qfalse);
    Com_FreeWeaponInfoMemory(COM_WEAPON_MEMORY_CGAME_OWNER, qfalse);

    const uint32_t now = Sys_Milliseconds();
    if ((int32_t)(now - com_lastErrorTime) <
        COM_ERROR_BURST_INTERVAL_MSEC) {
        ++com_consecutiveErrorCount;
        if (com_consecutiveErrorCount >
            COM_ERROR_BURST_FATAL_THRESHOLD) {
            errorCode = ERR_FATAL;
        }
    } else {
        com_consecutiveErrorCount = 0;
    }
    com_lastErrorTime = now;

    switch (errorCode) {
    case ERR_DROP:
    case ERR_SERVER_DISCONNECT:
    case ERR_DISCONNECT:
    case ERR_NEED_CD:
    case ERR_END_GAME:
        scr_updateScreenRecursionGuard = qfalse;
        com_errorCode = errorCode;
        longjmp(com_abortFrame, -1);

    default:
        break;
    }

    CL_Shutdown();
    SV_Shutdown(va("EXE_SERVER_FATAL_CRASHED\x15 %s",
                   originalMessage));
    Hunk_Clear();
    Com_Close();
    Sys_Error("%s", com_errorMessage);
}

/* Source: CoDUOMP.exe 0x0043d2a0..0x0043d346.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d2a0_0043d347.mcode.
 * Name and source-level call sequence: exact same-module Mac symbol
 * Com_Restart. MSVC inlines DObjShutdown, DObjInit, Com_InitDObj, and parts
 * of the script shutdown/initialization path into the Windows body. */
void Com_Restart(void)
{
    Com_ShutdownDObj();
    DObjShutdown();
    XAnimShutdown();
    Scr_Shutdown();
    Script_Init();
    XAnimInit();
    DObjInit();
    Com_InitDObj();
}
