#include "q_shared.h"

#include "client/console.h"
#include "filesystem/filesystem.h"
#include "platform/crt_boundary.h"
#include "server/server.h"
#include "system_console.h"

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* Original common console-log handle and the two independently compiled
 * recursion guards at CoDUOMP.exe 0x00981e88, 0x0389fd30
 * (Com_PrintMessage), and 0x0389fd34 (Com_VPrintf's inlined copy). */
int32_t com_consoleLogFile;
static qboolean com_printMessageOpeningConsoleLog;
static qboolean com_vprintfOpeningConsoleLog;

/* NOT_FROM_ORIGINAL_SOURCE: source factoring for the otherwise duplicated
 * Com_PrintMessage body in Com_VPrintf. The executable's inlined VPrintf copy
 * opens console_mp.log through FS_FOpenFileWrite while the callable original
 * uses FS_FOpenTextFileWrite. */
static void coduomp_com_print_message_internal(
    int32_t channel, const char *message, qboolean binaryConsoleLog,
    qboolean *openingConsoleLog)
{
    static const char consoleLogName[] = "console_mp.log";

    if (com_redirectBuffer != NULL) {
        if (channel == CON_DEST_NONE)
            return;

        const size_t combinedLength =
            strlen(com_redirectBuffer) + strlen(message);
        if (combinedLength >
            (size_t)(com_redirectBufferSize - 1)) {
            com_redirectFlush(com_redirectBuffer);
            com_redirectBuffer[0] = '\0';
        }
        Q_strcat(com_redirectBuffer, com_redirectBufferSize, message);
        return;
    }

    if (channel != CON_DEST_NONE) {
        if (dedicated != NULL && dedicated->integer == 0) {
            CL_ConsolePrint(
                message, (console_message_destination_t)channel,
                0, 0);
        }
        Sys_Print(message);
    }

    if (com_logfile == NULL || com_logfile->integer == 0)
        return;

    if (com_consoleLogFile == 0 &&
        fs_searchpaths != NULL &&
        *openingConsoleLog == qfalse) {
        *openingConsoleLog = qtrue;

        const time_t now = time(NULL);
        struct tm *const localTime = localtime(&now);
        com_consoleLogFile = binaryConsoleLog != qfalse
            ? FS_FOpenFileWrite(consoleLogName)
            : FS_FOpenTextFileWrite(consoleLogName);
        Com_Printf("logfile opened on %s\n", asctime(localTime));
        if (com_logfile->integer > 1)
            FS_ForceFlush(com_consoleLogFile);

        *openingConsoleLog = qfalse;
    }

    if (com_consoleLogFile != 0 && fs_searchpaths != NULL) {
        (void)FS_Write(
            message, (int32_t)strlen(message),
            com_consoleLogFile);
    }
}

/* Source: CoDUOMP.exe 0x004398f0..0x00439a7d.
 * Name and signature: exact same-module Mac symbol Com_PrintMessage. Channel
 * CON_DEST_NONE bypasses redirection and visible console output but remains
 * eligible for the disk log, exactly as in the original branch layout. */
void Com_PrintMessage(int32_t channel, const char *message)
{
    coduomp_com_print_message_internal(
        channel, message, qfalse,
        &com_printMessageOpeningConsoleLog);
}

/* Source: CoDUOMP.exe 0x00439a80..0x00439cb2.
 * Name and signature: exact same-module Mac symbol Com_VPrintf. MSVC inlines
 * Com_PrintMessage for the fixed miniconsole destination into this body, but
 * its copy uses the binary-mode filesystem opener. */
int32_t Com_VPrintf(const char *format, va_list args)
{
    enum { COM_PRINT_BUFFER_CAPACITY = 4096 };
    char message[COM_PRINT_BUFFER_CAPACITY];

    (void)coduo_crt_vsnprintf(
        message, sizeof(message), format, args);
    message[COM_PRINT_BUFFER_CAPACITY - 1] = '\0';
    coduomp_com_print_message_internal(
        CON_DEST_MINICONSOLE, message, qtrue,
        &com_vprintfOpeningConsoleLog);
    return (int32_t)strlen(message);
}

/* Source: CoDUOMP.exe 0x00439cc0..0x00439d1b.
 * Name and signature: exact same-module Mac symbol Com_Printf. The explicit
 * final-byte terminator preserves the original legacy _vsnprintf boundary. */
void Com_Printf(const char *format, ...)
{
    enum { COM_PRINT_BUFFER_CAPACITY = 4096 };
    char message[COM_PRINT_BUFFER_CAPACITY];
    va_list args;

    va_start(args, format);
    (void)coduo_crt_vsnprintf(
        message, sizeof(message), format, args);
    va_end(args);
    message[COM_PRINT_BUFFER_CAPACITY - 1] = '\0';

    Com_PrintMessage(CON_DEST_MINICONSOLE, message);
}

/* Source: CoDUOMP.exe 0x00439d20..0x00439d91.
 * Name and signature: exact same-module Mac symbol Com_DPrintf. */
void Com_DPrintf(const char *format, ...)
{
    enum { COM_PRINT_BUFFER_CAPACITY = 4096 };
    char message[COM_PRINT_BUFFER_CAPACITY];
    va_list args;

    if (com_developer == NULL || com_developer->integer == 0)
        return;

    va_start(args, format);
    (void)coduo_crt_vsnprintf(
        message, sizeof(message), format, args);
    va_end(args);
    message[COM_PRINT_BUFFER_CAPACITY - 1] = '\0';

    Com_Printf("%s", message);
}
