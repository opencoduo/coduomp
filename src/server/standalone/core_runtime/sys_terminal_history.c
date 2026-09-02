#include <ctype.h>
#include <string.h>

#include "qcommon/q_command.h"
#include "../core_cvar/cvar_private.h"
#include "../core_memory/core_memory_private.h"
#include "compat/coduo_ctype_compat.h"
#include "core_runtime_private.h"

enum {
    SYS_TTY_HISTORY_NEWEST_INDEX = 0,
    SYS_TTY_HISTORY_FIRST_NAVIGABLE_INDEX = 0,
    SYS_TTY_HISTORY_LAST_INDEX = CON_HISTORY_FIELD_COUNT - 1,
    SYS_TTY_LINE_DEFAULT_WIDTH = CON_INPUT_BUFFER_SIZE,
};

static const char sys_ttyCompletionArgSpace[] = " ";
static const char sys_ttyCompletionQuote[] = "\"";
static const char sys_ttyCompletionCommandFormat[] = "\\%s";
static const char sys_ttyCompletionListHeader[] = "]%s\n";

static const char *sys_ttyCompletionPrefix;
static char sys_ttyCompletionMatch[MAX_STRING_CHARS];
static int32_t sys_ttyCompletionMatchCount;
static console_input_field_t *sys_ttyCompletionLine;

typedef void (*sys_tty_completion_callback_t)(const char *name);

void Sys_TTYResetLine(console_input_field_t *line)
{
    memset(line->buffer, 0, sizeof(line->buffer));
    line->cursor = 0;
    line->scroll = 0;
    line->widthInChars = SYS_TTY_LINE_DEFAULT_WIDTH;
}

void Sys_TTYStoreHistoryLine(console_input_field_t *line)
{
    int index;

    for (index = SYS_TTY_HISTORY_LAST_INDEX; index > SYS_TTY_HISTORY_NEWEST_INDEX; index--) {
        sys_ttyHistory[index] = sys_ttyHistory[index - 1];
    }

    sys_ttyHistory[SYS_TTY_HISTORY_NEWEST_INDEX] = *line;

    if (sys_ttyHistoryCount < CON_HISTORY_FIELD_COUNT) {
        sys_ttyHistoryCount++;
    }

    sys_ttyHistoryCursor = SYS_TTY_HISTORY_RESET_CURSOR;
}

console_input_field_t *Sys_TTYPreviousHistoryLine(void)
{
    int32_t nextCursor;

    nextCursor = sys_ttyHistoryCursor + 1;
    if (nextCursor < sys_ttyHistoryCount) {
        sys_ttyHistoryCursor++;
        return &sys_ttyHistory[sys_ttyHistoryCursor];
    }

    return NULL;
}

console_input_field_t *Sys_TTYNextHistoryLine(void)
{
    if (sys_ttyHistoryCursor >= SYS_TTY_HISTORY_FIRST_NAVIGABLE_INDEX) {
        sys_ttyHistoryCursor--;
    }

    if (sys_ttyHistoryCursor == SYS_TTY_HISTORY_RESET_CURSOR) {
        return NULL;
    }

    return &sys_ttyHistory[sys_ttyHistoryCursor];
}

void Sys_TTYTrackCompletionMatch(const char *name)
{
    size_t prefixLength = strlen(sys_ttyCompletionPrefix);

    if (Q_stricmpn(name, sys_ttyCompletionPrefix, (int)prefixLength) == 0) {
        sys_ttyCompletionMatchCount++;
        if (sys_ttyCompletionMatchCount == 1) {
            Q_strncpyz(sys_ttyCompletionMatch, name, sizeof(sys_ttyCompletionMatch));
            return;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        for (int32_t index = 0; (size_t)index < sizeof(sys_ttyCompletionMatch) && name[index] != '\0'; ++index) {
            if (tolower(coduo_ctype_signed_byte_arg(sys_ttyCompletionMatch[index])) != tolower(coduo_ctype_signed_byte_arg(name[index]))) {
                sys_ttyCompletionMatch[index] = '\0';
            }
        }
    }
}

void Sys_TTYPrintCompletionMatch(const char *name)
{
    size_t matchLength = strlen(sys_ttyCompletionMatch);

    if (Q_stricmpn(name, sys_ttyCompletionMatch, (int)matchLength) == 0) {
        Com_Printf("    %s\n", name);
    }
}

void Sys_TTYAppendCompletionArgs(void)
{
    for (int32_t arg = 1; arg < Cmd_Argc(); ++arg) {
        const char *text;
        qboolean quote = qfalse;

        Q_strcat(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), sys_ttyCompletionArgSpace);
        text = Cmd_Argv(arg);

        for (const char *scan = text; *scan != '\0'; ++scan) {
            if (*scan == ' ') {
                Q_strcat(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), sys_ttyCompletionQuote);
                quote = qtrue;
                break;
            }
        }

        Q_strcat(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), text);

        if (quote != qfalse) {
            Q_strcat(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), sys_ttyCompletionQuote);
        }
    }
}

void Sys_TTYAppendCompletionSuffix(char *sourceText, char *prefix)
{
    char *match = strstr(sourceText, prefix);

    if (match == NULL) {
        Sys_TTYAppendCompletionArgs();
        return;
    }

    Q_strcat(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), match + strlen(prefix));
}

void Sys_TTYCompleteLine(console_input_field_t *line)
{
    console_input_field_t originalLine;

    sys_ttyCompletionLine = line;
    Cmd_TokenizeString(sys_ttyCompletionLine->buffer);
    sys_ttyCompletionPrefix = Cmd_Argv(0);
    if (*sys_ttyCompletionPrefix == '\\' || *sys_ttyCompletionPrefix == '/') {
        sys_ttyCompletionPrefix++;
    }

    sys_ttyCompletionMatchCount = 0;
    sys_ttyCompletionMatch[0] = '\0';

    if (*sys_ttyCompletionPrefix == '\0') {
        return;
    }

    Cmd_CommandCompletion(Sys_TTYTrackCompletionMatch);
    Cvar_CommandCompletion(Sys_TTYTrackCompletionMatch);
    if (sys_ttyCompletionMatchCount == 0) {
        return;
    }

    Com_Memcpy(&originalLine, sys_ttyCompletionLine, sizeof(originalLine));
    if (sys_ttyCompletionMatchCount == 1) {
        Com_sprintf(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), sys_ttyCompletionCommandFormat,
                    sys_ttyCompletionMatch);
        if (Cmd_Argc() == 1) {
            Q_strcat(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), sys_ttyCompletionArgSpace);
        } else {
            Sys_TTYAppendCompletionSuffix(originalLine.buffer, (char *)sys_ttyCompletionPrefix);
        }
        sys_ttyCompletionLine->cursor = (int32_t)strlen(sys_ttyCompletionLine->buffer);
        return;
    }

    Com_sprintf(sys_ttyCompletionLine->buffer, sizeof(sys_ttyCompletionLine->buffer), sys_ttyCompletionCommandFormat,
                sys_ttyCompletionMatch);
    sys_ttyCompletionLine->cursor = (int32_t)strlen(sys_ttyCompletionLine->buffer);
    Sys_TTYAppendCompletionSuffix(originalLine.buffer, (char *)sys_ttyCompletionPrefix);
    Com_Printf(sys_ttyCompletionListHeader, sys_ttyCompletionLine->buffer);
    Cmd_CommandCompletion(Sys_TTYPrintCompletionMatch);
    Cvar_CommandCompletion(Sys_TTYPrintCompletionMatch);
}
