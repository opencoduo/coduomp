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
    SYS_FATAL_EXIT_STATUS = -1,
    SYS_CONFIG_LANGUAGE_UNAVAILABLE = -1
};

typedef struct {
    int32_t line;
    const char *reason;
} coduomp_sys_config_error_t;

static qboolean coduomp_sys_improperQuitPending;
static int32_t coduomp_sys_improperQuitLanguage =
    SYS_CONFIG_LANGUAGE_UNAVAILABLE;

/* NOT_FROM_ORIGINAL_SOURCE: classify only unambiguous structural failures.
 * Ordinary unknown commands, comments, legacy bytes, empty files, and a
 * missing final newline remain valid inputs to the normal config loader. */
static qboolean coduomp_sys_config_parseable(
    const char *text, size_t length, coduomp_sys_config_error_t *error)
{
    qboolean quoted = qfalse;
    qboolean blockComment = qfalse;
    qboolean lineComment = qfalse;
    int32_t line = 1;
    int32_t openingLine = 1;

    if (length >= CBUF_TEXT_CAPACITY) {
        error->line = 1;
        error->reason = "file exceeds command queue capacity";
        return qfalse;
    }
    if (length >= 2 &&
        (((const uint8_t *)text)[0] == 0xffu &&
         ((const uint8_t *)text)[1] == 0xfeu) ||
        (((const uint8_t *)text)[0] == 0xfeu &&
         ((const uint8_t *)text)[1] == 0xffu)) {
        error->line = 1;
        error->reason = "UTF-16 text is unsupported";
        return qfalse;
    }

    for (size_t index = 0; index < length; ++index) {
        const char current = text[index];
        const char next =
            index + 1 < length ? text[index + 1] : '\0';

        if (current == '\0') {
            error->line = line;
            error->reason = "embedded NUL byte";
            return qfalse;
        }

        if (lineComment != qfalse) {
            if (current == '\n' || current == '\r')
                lineComment = qfalse;
        } else if (blockComment != qfalse) {
            if (current == '*' && next == '/') {
                blockComment = qfalse;
                ++index;
                continue;
            }
        } else if (quoted != qfalse) {
            if (current == '"') {
                quoted = qfalse;
            } else if (current == '\n' || current == '\r') {
                error->line = openingLine;
                error->reason = "quoted value crosses a line boundary";
                return qfalse;
            }
        } else if (current == '/' && next == '/') {
            lineComment = qtrue;
            ++index;
            continue;
        } else if (current == '/' && next == '*') {
            blockComment = qtrue;
            openingLine = line;
            ++index;
            continue;
        } else if (current == '"') {
            quoted = qtrue;
            openingLine = line;
        }

        if (current == '\n' ||
            (current == '\r' && next != '\n')) {
            ++line;
        }
    }

    if (quoted != qfalse) {
        error->line = openingLine;
        error->reason = "unfinished quoted value";
        return qfalse;
    }
    if (blockComment != qfalse) {
        error->line = openingLine;
        error->reason = "unfinished block comment";
        return qfalse;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: accept only a complete numeric language command;
 * malformed commands and unrelated text cannot supply a recovery value. */
static void coduomp_sys_extract_language_command(
    char *command, int32_t *language)
{
    Cmd_TokenizeString(command);

    const char *value = NULL;
    if (Cmd_Argc() == 2 &&
        Q_stricmp(Cmd_Argv(0), "cl_language") == 0) {
        value = Cmd_Argv(1);
    } else if (Cmd_Argc() == 3 &&
               Q_stricmp(Cmd_Argv(1), "cl_language") == 0 &&
               (Q_stricmp(Cmd_Argv(0), "set") == 0 ||
                Q_stricmp(Cmd_Argv(0), "seta") == 0 ||
                Q_stricmp(Cmd_Argv(0), "setu") == 0 ||
                Q_stricmp(Cmd_Argv(0), "sets") == 0)) {
        value = Cmd_Argv(2);
    }

    if (value != NULL) {
        char *end = NULL;
        const long parsed = strtol(value, &end, 10);
        if (end != value && *end == '\0' &&
            parsed >= LANGUAGE_ENGLISH && parsed <= LANGUAGE_CHINESE) {
            *language = (int32_t)parsed;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: recover the last complete cl_language assignment
 * outside comments. A malformed quoted command is discarded at its physical
 * line boundary so a later independent assignment can still be recovered. */
static int32_t coduomp_sys_extract_config_language(
    const char *text, size_t length)
{
    char command[CBUF_COMMAND_CAPACITY];
    size_t commandLength = 0;
    qboolean commandOverflow = qfalse;
    qboolean quoted = qfalse;
    qboolean blockComment = qfalse;
    qboolean lineComment = qfalse;
    int32_t language = SYS_CONFIG_LANGUAGE_UNAVAILABLE;

    for (size_t index = 0; index <= length; ++index) {
        const char current = index < length ? text[index] : '\n';
        const char next =
            index + 1 < length ? text[index + 1] : '\0';

        if (current == '\0')
            break;

        if (lineComment != qfalse) {
            if (current != '\n' && current != '\r')
                continue;
            lineComment = qfalse;
        } else if (blockComment != qfalse) {
            if (current == '*' && next == '/') {
                blockComment = qfalse;
                ++index;
                continue;
            }
            if (current != '\n' && current != '\r')
                continue;
        } else if (quoted == qfalse && current == '/' && next == '/') {
            lineComment = qtrue;
            ++index;
            continue;
        } else if (quoted == qfalse && current == '/' && next == '*') {
            blockComment = qtrue;
            ++index;
            continue;
        } else if (current == '"') {
            quoted = quoted == qfalse ? qtrue : qfalse;
        }

        const qboolean lineBoundary =
            current == '\n' || current == '\r';
        const qboolean commandBoundary =
            lineBoundary != qfalse ||
            (quoted == qfalse && blockComment == qfalse && current == ';');
        if (commandBoundary != qfalse) {
            if (quoted == qfalse && commandOverflow == qfalse) {
                command[commandLength] = '\0';
                coduomp_sys_extract_language_command(command, &language);
            }
            commandLength = 0;
            commandOverflow = qfalse;
            if (lineBoundary != qfalse)
                quoted = qfalse;
            continue;
        }

        if (blockComment == qfalse) {
            if (commandLength + 1 < sizeof(command))
                command[commandLength++] = current;
            else
                commandOverflow = qtrue;
        }
    }
    return language;
}

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
    coduomp_sys_improperQuitPending = qfalse;
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

        /* NOT_FROM_ORIGINAL_SOURCE: a complete stale marker only requests a
         * later config parse check. It cannot select safe mode by itself. */
        if (readSucceeded != FALSE && bytesRead == SYS_PROCESS_ID_BYTES) {
            if (savedProcessId != currentProcessId &&
                Sys_ProcessMatchesExecutable(savedProcessId) != qfalse) {
                return qfalse;
            }
            if (savedProcessId != currentProcessId)
                coduomp_sys_improperQuitPending = qtrue;
        }
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

/* NOT_FROM_ORIGINAL_SOURCE: the Windows improper-quit path offers recovery
 * only when the saved client config has an unambiguous structural failure.
 * A valid or absent config proceeds through the ordinary startup path. */
qboolean coduomp_sys_check_improper_quit_config(void)
{
    if (coduomp_sys_improperQuitPending == qfalse)
        return qfalse;
    coduomp_sys_improperQuitPending = qfalse;
    coduomp_sys_improperQuitLanguage =
        SYS_CONFIG_LANGUAGE_UNAVAILABLE;

    void *fileBuffer = NULL;
    const int32_t fileLength =
        FS_ReadFile("uoconfig_mp.cfg", &fileBuffer);
    if (fileLength < 0 || fileBuffer == NULL)
        return qfalse;

    coduomp_sys_config_error_t error = {1, NULL};
    const qboolean parseable = coduomp_sys_config_parseable(
        fileBuffer, (size_t)fileLength, &error);
    if (parseable != qfalse) {
        FS_FreeFile(fileBuffer);
        return qfalse;
    }

    coduomp_sys_improperQuitLanguage =
        coduomp_sys_extract_config_language(
            fileBuffer, (size_t)fileLength);
    FS_FreeFile(fileBuffer);
    Com_Printf(
        "uoconfig_mp.cfg is structurally invalid at line %i: %s\n",
        error.line, error.reason);

    const char *const title =
        Sys_LocalizeString("WIN_IMPROPER_QUIT_TITLE");
    const char *const body =
        Sys_LocalizeString("WIN_IMPROPER_QUIT_BODY");
    const int32_t response = MessageBoxA(
        NULL, body, title, SYS_PROCESS_LOCK_DIALOG_FLAGS);
    if (response == IDYES) {
        Com_SetSafeMode();
        return qtrue;
    }
    if (response == IDCANCEL) {
        Sys_DeleteProcessLockFile();
        Sys_ShutdownLocalization();
        exit(EXIT_SUCCESS);
    }
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: place the recovered language after recovery
 * defaults so the regenerated config retains the user's prior selection. */
void coduomp_sys_queue_improper_quit_language(void)
{
    if (coduomp_sys_improperQuitLanguage ==
        SYS_CONFIG_LANGUAGE_UNAVAILABLE) {
        return;
    }

    char command[64];
    Com_sprintf(command, sizeof(command),
                "seta cl_language \"%i\"\n",
                coduomp_sys_improperQuitLanguage);
    Cbuf_AddText(command);
}
#endif
