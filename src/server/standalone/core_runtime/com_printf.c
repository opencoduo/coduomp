#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core_runtime_private.h"

int32_t Com_VPrintf(const char *format, va_list args)
{
    char text[CODUO_COM_PRINT_FORMAT_BUFFER_SIZE];
    size_t textLength;
    size_t redirectLength;
    size_t redirectTextLength;
    size_t redirectCapacityLimit;
    time_t rawTime;
    struct tm *localTime;

    vsnprintf(text, sizeof(text), format, args);
    PB_Print(text, CODUO_COM_PB_PRINT_LIMIT);

    if (com_redirectBuffer != NULL) {
        redirectTextLength = strlen(text);
        redirectLength = strlen(com_redirectBuffer);
        redirectCapacityLimit = (size_t)(com_redirectBufferSize - CODUO_COM_REDIRECT_NUL_BYTE);
        if (redirectCapacityLimit < redirectLength + redirectTextLength) {
            com_redirectFlush(com_redirectBuffer);
            com_redirectBuffer[0] = '\0';
        }
        Q_strcat(com_redirectBuffer, com_redirectBufferSize, text);
        return (int32_t)strlen(text);
    }

    if (dedicated != NULL && dedicated->integer == CODUO_COM_DEDICATED_DISABLED) {
        CL_ConsolePrint(CODUO_COM_PRINT_CHANNEL_DEFAULT, text, CODUO_COM_CONSOLE_STUB_ARG2, CODUO_COM_CONSOLE_STUB_ARG3);
    }
    Sys_Print(text);

    if (com_logfile != NULL && com_logfile->integer != CODUO_COM_LOGFILE_DISABLED) {
        if (com_consoleLogFile == CODUO_COM_LOG_FILE_CLOSED_HANDLE && FS_Initialized() != qfalse && com_vprintfOpeningLog == qfalse) {
            com_vprintfOpeningLog = qtrue;
            time(&rawTime);
            localTime = localtime(&rawTime);
            com_consoleLogFile = FS_FOpenFileWrite("etconsole.log");
            Com_Printf("logfile opened on %s\n", asctime(localTime));
            if (com_logfile->integer > CODUO_COM_LOGFILE_SYNC_THRESHOLD) {
                FS_ForceFlush(com_consoleLogFile);
            }
            com_vprintfOpeningLog = qfalse;
        }

        if (com_consoleLogFile != CODUO_COM_LOG_FILE_CLOSED_HANDLE && FS_Initialized() != qfalse) {
            textLength = strlen(text);
            FS_Write(text, (int32_t)textLength, com_consoleLogFile);
        }
    }

    return (int32_t)strlen(text);
}

void Com_Printf(const char *format, ...)
{
    char text[CODUO_COM_PRINT_FORMAT_BUFFER_SIZE];
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    text[sizeof(text) - 1] = '\0';
    Com_PrintMessage(CODUO_COM_PRINT_CHANNEL_DEFAULT, text);
}

void Com_DPrintf(const char *format, ...)
{
    char text[CODUO_COM_PRINT_FORMAT_BUFFER_SIZE];
    va_list args;

    if (com_developer == NULL || com_developer->integer == 0) {
        return;
    }

    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    text[sizeof(text) - 1] = '\0';
    Com_Printf("%s", text);
}
