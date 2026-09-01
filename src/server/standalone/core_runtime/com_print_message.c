#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "core_runtime_private.h"

void Com_PrintMessage(int32_t channel, const char *message)
{
    size_t messageLength;
    size_t redirectMessageLength;
    size_t redirectLength;
    size_t redirectCapacityLimit;
    time_t rawTime;
    struct tm *localTime;

    PB_Print(message, CODUO_COM_PB_PRINT_LIMIT);

    if (com_redirectBuffer != NULL) {
        if (channel != CODUO_COM_PRINT_CHANNEL_DEVELOPER) {
            redirectMessageLength = strlen(message);
            redirectLength = strlen(com_redirectBuffer);
            redirectCapacityLimit =
                (size_t)(com_redirectBufferSize -
                         CODUO_COM_REDIRECT_NUL_BYTE);
            if (redirectCapacityLimit <
                redirectLength + redirectMessageLength) {
                com_redirectFlush(com_redirectBuffer);
                com_redirectBuffer[0] = '\0';
            }
            Q_strcat(com_redirectBuffer, com_redirectBufferSize, message);
        }
        return;
    }

    if (channel != CODUO_COM_PRINT_CHANNEL_DEVELOPER) {
        if (dedicated != NULL &&
            dedicated->integer == CODUO_COM_DEDICATED_DISABLED) {
            CL_ConsolePrint(channel, message, CODUO_COM_CONSOLE_STUB_ARG2,
                            CODUO_COM_CONSOLE_STUB_ARG3);
        }
        Sys_Print(message);
    }

    if (com_logfile == NULL ||
        com_logfile->integer == CODUO_COM_LOGFILE_DISABLED) {
        return;
    }

    if (com_consoleLogFile == CODUO_COM_LOG_FILE_CLOSED_HANDLE &&
        FS_Initialized() != qfalse &&
        com_printMessageOpeningLog == qfalse) {
        com_printMessageOpeningLog = qtrue;
        time(&rawTime);
        localTime = localtime(&rawTime);
        com_consoleLogFile = FS_FOpenTextFileWrite("console_mp_server.log");
        Com_Printf("logfile opened on %s\n", asctime(localTime));
        if (com_logfile->integer > CODUO_COM_LOGFILE_SYNC_THRESHOLD) {
            FS_ForceFlush(com_consoleLogFile);
        }
        com_printMessageOpeningLog = qfalse;
    }

    if (com_consoleLogFile != CODUO_COM_LOG_FILE_CLOSED_HANDLE &&
        FS_Initialized() != qfalse) {
        messageLength = strlen(message);
        FS_Write(message, (int32_t)messageLength,
                 com_consoleLogFile);
    }
}
