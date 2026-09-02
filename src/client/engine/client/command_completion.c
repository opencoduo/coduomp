#include "console.h"

#include "../platform/crt_boundary.h"

#include <string.h>

/* Original command-completion scratch state at CoDUOMP.exe 0x008ce4d8,
 * 0x008ce0c8, and 0x008ce0d8. CompleteCommand selects the prefix and clears
 * the count before enumerating command and cvar names through FindMatches. */
const char *completionString;
int32_t completionMatchCount;
char completionShortestMatch[CON_COMPLETION_MATCH_SIZE];

/* Source: CoDUOMP.exe 0x0040de90..0x0040e057.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040de90_0040e058.mcode.
 * Name and signature: exact same-module Mac symbol keyConcatArgs. Q_strcat is
 * expanded inline four times in the Windows body. */
void keyConcatArgs(void)
{
    const int32_t argumentCount = Cmd_Argc();

    for (int32_t argumentIndex = 1; argumentIndex < argumentCount; ++argumentIndex) {
        const char *argument = Cmd_Argv(argumentIndex);
        const qboolean quoteArgument = strchr(argument, ' ') != NULL ? qtrue : qfalse;

        Q_strcat(con_inputField.buffer, sizeof(con_inputField.buffer), " ");
        if (quoteArgument != qfalse)
            Q_strcat(con_inputField.buffer, sizeof(con_inputField.buffer), "\"");
        Q_strcat(con_inputField.buffer, sizeof(con_inputField.buffer), argument);
        if (quoteArgument != qfalse)
            Q_strcat(con_inputField.buffer, sizeof(con_inputField.buffer), "\"");
    }
}

/* Source: CoDUOMP.exe 0x0040e060..0x0040e0a1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e060_0040e0a2.mcode.
 * Name and signature: exact same-module Mac symbol ConcatRemaining. */
void ConcatRemaining(const char *source, const char *separator)
{
    const char *remaining = strstr(source, separator);

    if (remaining == NULL) {
        keyConcatArgs();
        return;
    }

    Q_strcat(con_inputField.buffer, sizeof(con_inputField.buffer), remaining + strlen(separator));
}

/* Source: CoDUOMP.exe 0x0040e0b0..0x0040e355.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e0b0_0040e356.mcode.
 * Name and signature: exact same-module Mac symbol CompleteCommand. */
void CompleteCommand(void)
{
    char *command = con_inputField.buffer;
    if (*command == '/' || *command == '\\')
        ++command;

    if (coduo_crt_strnicmp(command, "pb_", 3) == 0) {
        char punkBusterCommand[CON_INPUT_BUFFER_SIZE];

        Q_strncpyz(punkBusterCommand, command, (int32_t)sizeof(punkBusterCommand));
        if (coduo_crt_strnicmp(punkBusterCommand, "pb_sv_", 6) == 0) {
            PbServerCompleteCommand(punkBusterCommand, CON_INPUT_BUFFER_SIZE - 1);
        } else {
            PbClientCompleteCommand(punkBusterCommand, CON_INPUT_BUFFER_SIZE - 1);
        }

        Com_sprintf(con_inputField.buffer, CON_INPUT_BUFFER_SIZE, "\\%s", punkBusterCommand);
        con_inputField.cursor = (int32_t)strlen(con_inputField.buffer);
        Field_AdjustScroll(&con_inputField);
        return;
    }

    Cmd_TokenizeString(con_inputField.buffer);
    completionString = Cmd_Argv(0);
    if (*completionString == '/' || *completionString == '\\')
        ++completionString;

    completionMatchCount = 0;
    completionShortestMatch[0] = '\0';
    if (*completionString == '\0')
        return;

    Cmd_CommandCompletion(FindMatches);
    Cvar_CommandCompletion(FindMatches);
    if (completionMatchCount == 0)
        return;

    const console_input_field_t originalField = con_inputField;
    Com_sprintf(con_inputField.buffer, CON_INPUT_BUFFER_SIZE, "\\%s", completionShortestMatch);

    if (completionMatchCount == 1) {
        if (Cmd_Argc() == 1) {
            Q_strcat(con_inputField.buffer, CON_INPUT_BUFFER_SIZE, " ");
        } else {
            ConcatRemaining(originalField.buffer, completionString);
        }
        con_inputField.cursor = (int32_t)strlen(con_inputField.buffer);
        Field_AdjustScroll(&con_inputField);
        return;
    }

    con_inputField.cursor = (int32_t)strlen(con_inputField.buffer);
    ConcatRemaining(originalField.buffer, completionString);
    Field_AdjustScroll(&con_inputField);

    Com_Printf("]%s\n", con_inputField.buffer);
    Cmd_CommandCompletion(PrintMatches);
    Cvar_CommandCompletion(PrintCvarMatches);
}

/* Source: CoDUOMP.exe 0x0040dd50..0x0040dde6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040dd50_0040dde7.mcode.
 * Name and signature: exact same-module Mac symbol FindMatches. */
void FindMatches(const char *candidate)
{
    const int32_t prefixLength = (int32_t)strlen(completionString);

    if (Q_stricmpn(completionString, candidate, prefixLength) != 0)
        return;

    ++completionMatchCount;
    if (completionMatchCount == 1) {
        strncpy(completionShortestMatch, candidate, CON_COMPLETION_MATCH_SIZE - 1);
        completionShortestMatch[CON_COMPLETION_MATCH_SIZE - 1] = '\0';
        return;
    }

    int32_t commonLength = 0;
    while (candidate[commonLength] != '\0' && coduo_crt_tolower((int32_t)(signed char)completionShortestMatch[commonLength]) ==
                                                  coduo_crt_tolower((int32_t)(signed char)candidate[commonLength])) {
        ++commonLength;
    }
    completionShortestMatch[commonLength] = '\0';
}

/* Source: CoDUOMP.exe 0x0040ddf0..0x0040de25.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0040ddf0_0040de26.mcode.
 * Name and signature: exact same-module Mac symbol PrintMatches. */
void PrintMatches(const char *candidate)
{
    const int32_t prefixLength = (int32_t)strlen(completionShortestMatch);

    if (Q_stricmpn(completionShortestMatch, candidate, prefixLength) == 0)
        Com_Printf("    %s\n", candidate);
}

/* Source: CoDUOMP.exe 0x0040de30..0x0040de8c.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0040de30_0040de8d.mcode.
 * Name and signature: exact same-module Mac symbol PrintCvarMatches. */
void PrintCvarMatches(const char *candidate)
{
    const int32_t prefixLength = (int32_t)strlen(completionShortestMatch);

    if (Q_stricmpn(completionShortestMatch, candidate, prefixLength) == 0) {
        cvar_t *cvar = Cvar_FindVar(candidate);
        Com_Printf("    ^7%s = ^5%s^0\n", candidate, cvar != NULL ? cvar->string : "");
    }
}
