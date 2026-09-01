#include "system_process_lock.h"

#include "system_localization.h"
#include "qcommon/com_sprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char sysProcessLockFile[SYS_PROCESS_LOCK_NAME_CAPACITY];
                                        /* original 0x009cf1b0..0x009cf1cf */

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>

enum {
    SYS_PROCESS_ID_BYTES = 4,
    SYS_PROCESS_LOCK_DIALOG_FLAGS =
        MB_YESNOCANCEL | MB_ICONWARNING,
    SYS_PROCESS_LOCK_ERROR_FLAGS = MB_ICONERROR,
    SYS_FATAL_EXIT_STATUS = -1
};

/* Source: CoDUOMP.exe 0x0046c2f0..0x0046c409.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046c2f0_0046c40a.mcode and the
 * OpenProcess/Toolhelp import calls. Exact source name is unavailable; the
 * role name states the proven check: the process must exist and one of its
 * modules must have the same basename as the current executable. */
qboolean Sys_ProcessMatchesExecutable(uint32_t processId)
{
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (process == NULL)
        return qfalse;
    CloseHandle(process);

    HANDLE snapshot =
        CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    if (snapshot == INVALID_HANDLE_VALUE)
        return qfalse;

    MODULEENTRY32 module;
    module.dwSize = sizeof(module);
    if (Module32First(snapshot, &module) == FALSE) {
        CloseHandle(snapshot);
        return qfalse;
    }

    char currentPath[MAX_OSPATH];
    (void)GetModuleFileNameA(NULL, currentPath, sizeof(currentPath));
    currentPath[sizeof(currentPath) - 1] = '\0';

    const char *currentName = currentPath;
    for (const char *cursor = currentPath;
         *cursor != '\0';
         ++cursor) {
        if (*cursor == '\\' || *cursor == ':')
            currentName = cursor + 1;
    }

    qboolean matched = qfalse;
    do {
        if (Q_stricmp(module.szModule, currentName) == 0) {
            matched = qtrue;
            break;
        }
    } while (Module32Next(snapshot, &module) != FALSE);

    CloseHandle(snapshot);
    return matched;
}

/* Source: CoDUOMP.exe 0x0046c410..0x0046c488.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046c410_0046c489.mcode and the exact
 * "__%s" format string at 0x0059efa0. Exact source name is unavailable; the
 * role name describes the derived hidden per-executable lock filename. */
void Sys_InitProcessLockFile(void)
{
    char executablePath[MAX_OSPATH];
    (void)GetModuleFileNameA(
        NULL, executablePath, sizeof(executablePath));
    executablePath[sizeof(executablePath) - 1] = '\0';

    char *executableName = executablePath;
    for (char *cursor = executablePath;
         *cursor != '\0';
         ++cursor) {
        if (*cursor == '\\' || *cursor == ':')
            executableName = cursor + 1;
        else if (*cursor == '.')
            *cursor = '\0';
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    Com_sprintf(sysProcessLockFile, sizeof(sysProcessLockFile), "__%s",
                executableName);
}

/* Source: CoDUOMP.exe 0x0046c5b0..0x0046c5bb.
 * Role name: the retained Windows-only boundary deletes the hidden
 * per-executable PID lock. MSVC also expands it inline at shutdown and mode
 * transition call sites. */
void Sys_DeleteProcessLockFile(void)
{
    (void)DeleteFileA(sysProcessLockFile);
}

/* Source: CoDUOMP.exe 0x0046c490..0x0046c5a8 with the shared cold error tail
 * at 0x0046c2c0..0x0046c2e9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046c2c0_0046c5a9.mcode, the exact
 * early-localization references at 0x0059ef70..0x0059efbc, and the Win32
 * file/process imports. Exact source name is unavailable; the role name
 * describes the read-check-prompt-rewrite lifecycle of the PID lock. */
qboolean Sys_CheckProcessLock(void)
{
    const uint32_t currentProcessId = GetCurrentProcessId();
    HANDLE lockFile = CreateFileA(
        sysProcessLockFile, GENERIC_READ, 0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_HIDDEN, NULL);

    if (lockFile != INVALID_HANDLE_VALUE) {
        uint32_t savedProcessId;
        DWORD bytesRead;
        const BOOL readSucceeded = ReadFile(
            lockFile, &savedProcessId, sizeof(savedProcessId),
            &bytesRead, NULL);
        CloseHandle(lockFile);

        if (readSucceeded != FALSE &&
            bytesRead == SYS_PROCESS_ID_BYTES &&
            savedProcessId != currentProcessId &&
            Sys_ProcessMatchesExecutable(savedProcessId) != qfalse) {
            return qfalse;
        }

        const char *const title =
            Sys_LocalizeString("WIN_IMPROPER_QUIT_TITLE");
        const char *const body =
            Sys_LocalizeString("WIN_IMPROPER_QUIT_BODY");
        const int32_t response = MessageBoxA(
            NULL, body, title, SYS_PROCESS_LOCK_DIALOG_FLAGS);
        if (response == IDYES)
            Com_SetSafeMode();
        else if (response == IDCANCEL)
            return qfalse;
    }

    lockFile = CreateFileA(
        sysProcessLockFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN, NULL);
    if (lockFile == INVALID_HANDLE_VALUE)
        goto disk_full;

    DWORD bytesWritten;
    const BOOL writeSucceeded = WriteFile(
        lockFile, &currentProcessId, sizeof(currentProcessId),
        &bytesWritten, NULL);
    CloseHandle(lockFile);
    if (writeSucceeded != FALSE &&
        bytesWritten == SYS_PROCESS_ID_BYTES) {
        return qtrue;
    }

disk_full:
    {
        const char *const title =
            Sys_LocalizeString("WIN_DISK_FULL_TITLE");
        const char *const body =
            Sys_LocalizeString("WIN_DISK_FULL_BODY");
        (void)MessageBoxA(
            NULL, body, title, SYS_PROCESS_LOCK_ERROR_FLAGS);
        exit(SYS_FATAL_EXIT_STATUS);
    }
}
#endif
