#include "script_error_reporting.h"
#include "script_runtime_host.h"
#include "script_runtime_state.h"
#include "script_source_positions.h"
#include "script_value.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    SCRIPT_ERROR_PRINT_CHANNEL_DEFAULT = 0,
    SCRIPT_ERROR_PRINT_CHANNEL_WARNING = 4,
};

/* Sources: CoDUOMP.exe 0x00481860..0x0048196b and coduo_lnxded
 * 0x080a3aaa..0x080a3c32. */
void PrintSourcePos(int32_t channel, const char *filename, const char *sourceText, uint32_t sourcePos)
{
    const char *lineStart = sourceText;
    const char *sourceCursor = sourceText;
    int32_t line = 1;

    for (uint32_t remaining = sourcePos; remaining != 0; --remaining) {
        if (*sourceCursor == '\0') {
            lineStart = sourceCursor + 1;
            line++;
        }
        sourceCursor++;
    }

    size_t lineLength = strlen(lineStart);
    size_t displayedLineLength = lineLength;
    if (displayedLineLength >= MAX_STRING_CHARS) {
        displayedLineLength = MAX_STRING_CHARS - 1;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: source diagnostics use the fixed
     * MAX_STRING_CHARS display domain. */
    char lineText[MAX_STRING_CHARS];
    for (size_t index = 0; index < displayedLineLength; ++index) {
        char ch = lineStart[index];
        lineText[index] = ch == '\t' ? ' ' : ch;
    }
    lineText[displayedLineLength] = '\0';

    const char *savegameSuffix = script_savedSourceFiles == NULL ? "" : " (savegame)";
    Com_PrintMessage(channel, va("(file '%s'%s, line %d)\n", filename, savegameSuffix, line));
    Com_PrintMessage(channel, va("%s\n", lineText));

    size_t markerOffset = (size_t)(sourceCursor - lineStart);
    char marker[MAX_STRING_CHARS];
    if (markerOffset > sizeof(marker) - 2) {
        markerOffset = sizeof(marker) - 2;
    }
    Com_Memset(marker, ' ', markerOffset);
    marker[markerOffset] = '*';
    marker[markerOffset + 1] = '\0';
    Com_PrintMessage(channel, va("%s\n", marker));
}

/* Sources: CoDUOMP.exe 0x00481970..0x00481a0c and coduo_lnxded
 * 0x080a3c34..0x080a3d4e. */
void Scr_PrintPrevCodePos(int32_t channel, script_codepos_t codePos, int32_t sourcePosOffset)
{
    const uintptr_t address = (uintptr_t)codePos;
    const uintptr_t codeBegin = (uintptr_t)script_codeBase;

    if (script_runtimeDebugReportFlag == qfalse) {
        Com_PrintMessage(channel, va("@ %d\n", (int32_t)(address - codeBegin)));
        return;
    }

    qboolean loadedCodePos = ScriptCode_IsLoadedCodePos(codePos);
    int32_t fileIndex = (int32_t)script_sourceFileCount;
    do {
        fileIndex--;
        if (fileIndex < 1) {
            break;
        }

        uint8_t *fileCodeStart =
            loadedCodePos == qfalse ? script_sourceFiles[fileIndex].relocatedCodeStart : script_sourceFiles[fileIndex].normalCodeStart;
        if (fileCodeStart != NULL && address > (uintptr_t)fileCodeStart) {
            break;
        }
    } while (qtrue);

    uint32_t sourcePos = GetPrevSourcePos(codePos, sourcePosOffset);
    PrintSourcePos(channel, script_sourceFiles[fileIndex].filename, script_sourceFiles[fileIndex].source, sourcePos);
}

/* Sources: CoDUOMP.exe 0x00481a10..0x00481ac3 and coduo_lnxded
 * 0x080a3d50..0x080a3e10. */
void CompileError(uint32_t sourcePos, const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    va_list args;

    Com_Printf("\n");
    Com_Printf("******* script compile error *******\n");
    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (script_runtimeDebugReportFlag == qfalse) {
        Com_Printf("%s\n", message);
    } else {
        Com_Printf("%s: ", message);
        PrintSourcePos(SCRIPT_ERROR_PRINT_CHANNEL_DEFAULT, script_sourceFilename, script_sourcePos, sourcePos);
    }

    Com_Printf("************************************\n");
    Com_Error(ERR_DROP, "\x15"
                        "script compile error\n(see console for details)");
}

/* Sources: CoDUOMP.exe 0x00481ad0..0x00481b5c and coduo_lnxded
 * 0x080a3e12..0x080a3ea7. */
void CompileError2(script_codepos_t codePos, const char *format, ...)
{
    char message[MAX_STRING_CHARS];
    va_list args;

    Com_Printf("\n");
    Com_Printf("******* script compile error *******\n");
    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted diagnostic to its fixed
     * destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Com_Printf("%s: ", message);
    Scr_PrintPrevCodePos(SCRIPT_ERROR_PRINT_CHANNEL_DEFAULT, codePos, 0);
    Com_Printf("************************************\n");
    Com_Error(ERR_DROP, "\x15"
                        "script compile error\n(see console for details)");
}

/* Sources: CoDUOMP.exe 0x00481b60..0x00481bc1 and coduo_lnxded
 * 0x080a3ea8..0x080a3f49.  The same Mac function is named
 * RuntimeErrorInternal and retains the complete five-argument signature.
 * `detail` is present in the original interface but unused by this printer. */
void RuntimeErrorInternal(int32_t channel, script_codepos_t codePos, int32_t sourcePosOffset, const char *message, const char *detail)
{
    (void)detail;

    Com_PrintMessage(channel, va("\n******* script runtime error *******\n%s: ", message));
    Scr_PrintPrevCodePos(channel, codePos, sourcePosOffset);

    for (int32_t stackIndex = script_callStackDepth - 1; stackIndex >= 0; --stackIndex) {
        Com_PrintMessage(channel, "called from:\n");
        Scr_PrintPrevCodePos(channel, script_callStackCodepos[stackIndex], 0);
    }
    Com_PrintMessage(channel, "************************************\n");
}

/* Sources: CoDUOMP.exe 0x00481bd0..0x00481c53 and coduo_lnxded
 * 0x080a3f4a..0x080a402c.  RuntimeError is also the exact supporting Mac
 * symbol name. */
void RuntimeError(script_codepos_t codePos, int32_t sourcePosOffset, const char *message, const char *detail)
{
    if (script_runtimeDebugReportFlag == qfalse && script_forceErrorReport == qfalse) {
        return;
    }

    qboolean fatal = script_runtimeDeveloperFlag != 0 || script_forceErrorReport != qfalse ? qtrue : qfalse;
    int32_t channel = fatal == qfalse ? SCRIPT_ERROR_PRINT_CHANNEL_WARNING : SCRIPT_ERROR_PRINT_CHANNEL_DEFAULT;
    RuntimeErrorInternal(channel, codePos, sourcePosOffset, message, detail);

    if (fatal == qfalse) {
        return;
    }

    const char *detailPrefix = "\n";
    if (detail == NULL) {
        detailPrefix = "";
        detail = "";
    }

    int32_t errorCode = script_forceErrorReport == qfalse ? ERR_SCRIPT : ERR_DROP;
    Com_Error(errorCode,
              "\x15"
              "script runtime error\n(see console for details)%s%s",
              detailPrefix, detail);
}
