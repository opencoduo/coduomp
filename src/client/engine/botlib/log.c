#include "log.h"

#include "libvar.h"
#include "../q_shared.h"

#include <stdarg.h>
#include <string.h>

/* Original botlib log state at 0x009a73a0 and 0x009a77a0. */
static char logFilename[MAX_OSPATH];
static FILE *logFile;

/* Source: CoDUOMP.exe 0x004425b0..0x00442644.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004425b0_00442645.mcode.
 * Role name and messages: botlib Log_Open. */
void Log_Open(const char *filename)
{
    if (filename == NULL || strlen(filename) == 0) {
        Com_Printf("openlog <filename>\n");
        return;
    }
    if (logFile != NULL) {
        Com_Printf("^1Error: log file %s is already opened\n", logFilename);
        return;
    }

    logFile = fopen(filename, "wb");
    if (logFile == NULL) {
        Com_Printf("^1Error: can't open the log file %s\n", filename);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    strncpy(logFilename, filename, sizeof(logFilename) - 1u);
    logFilename[sizeof(logFilename) - 1u] = '\0';
    Com_Printf("Opened log %s\n", logFilename);
}

/* Source: CoDUOMP.exe 0x00442650..0x00442679.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442650_0044267a.mcode.
 * Role name: create the botlib log only when the LibVar "log" switch is
 * nonzero. */
void Log_Create(const char *filename)
{
    if (LibVarValue("log", "0") != 0.0f)
        Log_Open(filename);
}

/* Source: CoDUOMP.exe 0x00442680..0x004426c0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442680_004426c1.mcode. A failed fclose leaves
 * logFile intact, matching the original retryable state. */
void Log_Close(void)
{
    if (logFile == NULL)
        return;

    if (fclose(logFile) != 0) {
        Com_Printf("^1Error: can't close log file %s\n", logFilename);
        return;
    }

    logFile = NULL;
    Com_Printf("Closed log %s\n", logFilename);
}

/* Source: CoDUOMP.exe 0x004426d0..0x004426de.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004426d0_004426df.mcode. */
void Log_Shutdown(void)
{
    if (logFile != NULL)
        Log_Close();
}

/* Source: CoDUOMP.exe 0x004426e0..0x00442707.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004426e0_00442708.mcode. */
void Log_Write(const char *format, ...)
{
    if (logFile == NULL)
        return;

    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    fflush(logFile);
}

/* Source: CoDUOMP.exe 0x00442710..0x00442715.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442710_00442716.mcode. */
FILE *Log_FilePointer(void)
{
    return logFile;
}

/* Source: CoDUOMP.exe 0x00442720..0x00442730.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442720_00442731.mcode. */
void Log_Flush(void)
{
    if (logFile != NULL)
        fflush(logFile);
}
