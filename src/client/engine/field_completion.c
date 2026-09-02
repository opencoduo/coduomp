#include "q_shared.h"

#include "client/console.h"
#include "platform/crt_boundary.h"

#include <stdlib.h>
#include <string.h>

enum {
    FIELD_COMPLETION_MATCH_CAPACITY = 1024
};

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
int32_t cmd_wait;                                  /* original 0x0493dd80 */
char cmd_textData[CBUF_TEXT_CAPACITY];             /* original 0x0493dda0 */
cbuf_t cmd_text;                                   /* original 0x0494dda0 */
char cmd_args[CMD_ARGS_CAPACITY];                  /* original 0x009649b8 */
cmd_function_t *cmd_functions;                     /* original 0x00964db8 */
const char *cmd_argv[CMD_ARGUMENT_CAPACITY];       /* original 0x00964dc0 */
char cmd_tokenBuffer[CMD_TOKEN_BUFFER_CAPACITY];   /* original 0x009655c0 */
int32_t cmd_argc;                                  /* original 0x009677c0 */

/* The qcommon field completer is distinct from the client-console completion
 * implementation in client/command_completion.c. Its callbacks communicate
 * through this private state in the original executable. */
static console_input_field_t *fieldCompletionField; /* original 0x00969218 */
static const char *fieldCompletionString;           /* original 0x00981e84 */
static int32_t fieldCompletionMatchCount;           /* original 0x0098022c */
static char fieldCompletionShortestMatch[
    FIELD_COMPLETION_MATCH_CAPACITY];               /* original 0x00980278 */

/* Source: CoDUOMP.exe 0x0043ca50..0x0043ca6a.
 * Evidence: repaired executable-gap boundary and direct stores.
 * Name and field type: exact same-module Mac symbol Field_Clear. The original
 * clears the text but deliberately leaves pixel/font presentation fields
 * unchanged. */
void Field_Clear(console_input_field_t *field)
{
    memset(field->buffer, 0, sizeof(field->buffer));
    field->cursor = 0;
    field->scroll = 0;
    field->widthInChars = CON_INPUT_BUFFER_SIZE;
}

/* Source: CoDUOMP.exe 0x0043ca70..0x0043cafd.
 * Evidence: repaired executable-gap boundary and the callback addresses in
 * Field_CompleteCommand. Provisional source name distinguishes this private
 * qcommon callback from the client-console callback with the same likely
 * original static name. */
static void Field_FindMatches(const char *candidate)
{
    const int32_t prefixLength =
        (int32_t)strlen(fieldCompletionString);

    if (Q_stricmpn(fieldCompletionString, candidate,
                   prefixLength) != 0) {
        return;
    }

    ++fieldCompletionMatchCount;
    if (fieldCompletionMatchCount == 1) {
        strncpy(fieldCompletionShortestMatch, candidate,
                FIELD_COMPLETION_MATCH_CAPACITY - 1);
        fieldCompletionShortestMatch[
            FIELD_COMPLETION_MATCH_CAPACITY - 1] = '\0';
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (int32_t index = 0;
         index < FIELD_COMPLETION_MATCH_CAPACITY &&
         candidate[index] != '\0'; ++index) {
        if (coduo_crt_tolower(
                (int32_t)(int8_t)
                    fieldCompletionShortestMatch[index]) !=
            coduo_crt_tolower(
                (int32_t)(int8_t)candidate[index])) {
            fieldCompletionShortestMatch[index] = '\0';
        }
    }
}

/* Source: CoDUOMP.exe 0x0043cb00..0x0043cb35.
 * Evidence: repaired executable-gap boundary and the two completion-list
 * callback sites. */
static void Field_PrintMatches(const char *candidate)
{
    const int32_t prefixLength =
        (int32_t)strlen(fieldCompletionShortestMatch);

    if (Q_stricmpn(fieldCompletionShortestMatch, candidate,
                   prefixLength) == 0) {
        Com_Printf("    %s\n", candidate);
    }
}

/* Source: CoDUOMP.exe 0x0043cb40..0x0043cd17.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043cb40_0043cd18.mcode.
 * Provisional source name by role. Q_strcat is expanded at each append site
 * in the optimized Windows body. */
static void Field_ConcatArgs(void)
{
    for (int32_t argumentIndex = 1;
         argumentIndex < cmd_argc;
         ++argumentIndex) {
        const char *const argument = cmd_argv[argumentIndex];
        const qboolean quoteArgument =
            strchr(argument, ' ') != NULL ? qtrue : qfalse;

        Q_strcat(fieldCompletionField->buffer,
                 CON_INPUT_BUFFER_SIZE, " ");
        if (quoteArgument != qfalse) {
            Q_strcat(fieldCompletionField->buffer,
                     CON_INPUT_BUFFER_SIZE, "\"");
        }
        Q_strcat(fieldCompletionField->buffer,
                 CON_INPUT_BUFFER_SIZE, argument);
        if (quoteArgument != qfalse) {
            Q_strcat(fieldCompletionField->buffer,
                     CON_INPUT_BUFFER_SIZE, "\"");
        }
    }
}

/* Source: CoDUOMP.exe 0x0043cd20..0x0043cd65.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043cd20_0043cd66.mcode.
 * Provisional source name by role. */
static void Field_ConcatRemaining(const char *source,
                                  const char *separator)
{
    const char *const remaining = strstr(source, separator);

    if (remaining == NULL) {
        Field_ConcatArgs();
        return;
    }

    Q_strcat(fieldCompletionField->buffer, CON_INPUT_BUFFER_SIZE,
             remaining + strlen(separator));
}

/* Source: CoDUOMP.exe 0x0043cd70..0x0043cf4e.
 * Evidence: repaired executable-gap boundary, the qcommon completion globals,
 * and the typed field accesses at +0x00 and +0x1c. Source role follows the
 * established qcommon Field_CompleteCommand interface; this Windows build has
 * no same-module Mac traceback symbol for the function. */
void Field_CompleteCommand(console_input_field_t *field)
{
    fieldCompletionField = field;
    Cmd_TokenizeString(field->buffer);
    fieldCompletionString = Cmd_Argv(0);

    if (*fieldCompletionString == '\\' ||
        *fieldCompletionString == '/') {
        ++fieldCompletionString;
    }

    fieldCompletionMatchCount = 0;
    fieldCompletionShortestMatch[0] = '\0';
    if (*fieldCompletionString == '\0') {
        return;
    }

    Cmd_CommandCompletion(Field_FindMatches);
    Cvar_CommandCompletion(Field_FindMatches);
    if (fieldCompletionMatchCount == 0) {
        return;
    }

    const console_input_field_t originalField = *field;
    Com_sprintf(field->buffer, CON_INPUT_BUFFER_SIZE,
                "\\%s", fieldCompletionShortestMatch);

    if (fieldCompletionMatchCount == 1) {
        if (Cmd_Argc() == 1) {
            Q_strcat(field->buffer, CON_INPUT_BUFFER_SIZE, " ");
        } else {
            Field_ConcatRemaining(originalField.buffer,
                                  fieldCompletionString);
        }
        field->cursor = (int32_t)strlen(field->buffer);
        return;
    }

    field->cursor = (int32_t)strlen(field->buffer);
    Field_ConcatRemaining(originalField.buffer,
                          fieldCompletionString);
    Com_Printf("]%s\n", field->buffer);
    Cmd_CommandCompletion(Field_PrintMatches);
    Cvar_CommandCompletion(Field_PrintMatches);
}
